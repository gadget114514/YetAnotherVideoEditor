#include "WasapiDevice.h"

#include <QThread>
#include <QStringList>

#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <thread>

namespace yave::audio {

namespace {

// WASAPI の COM 定義 (mingw-w64 / MSVC 共通)。__uuidof を使わず IID を直接参照する。
const CLSID kCLSID_MMDeviceEnumerator = {
    0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
const IID   kIID_IMMDeviceEnumerator = {
    0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
const IID   kIID_IAudioClient = {
    0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}};
const IID   kIID_IAudioRenderClient = {
    0xF294ACFC, 0x3146, 0x4483, {0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2}};

/// テンプレート不要の簡易 RAII COM リリーサ
template <typename T>
void safeRelease(T** p)
{
    if (*p) {
        (*p)->Release();
        *p = nullptr;
    }
}

class WasapiDevice final : public IAudioDevice
{
public:
    ~WasapiDevice() override { close(); }

    bool open(const QString& deviceId, int sampleRate, int bufferFrames,
              AudioCallback cb, void* userData, QString* errorOut) override
    {
        callback_ = cb;
        userData_ = userData;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        comInitHere_ = SUCCEEDED(hr);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) {
            if (errorOut)
                *errorOut = QStringLiteral("CoInitializeEx failed: 0x%1")
                                .arg(uint(hr), 8, 16, QLatin1Char('0'));
            return false;
        }

        IMMDeviceEnumerator* enumerator = nullptr;
        hr = CoCreateInstance(kCLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                              kIID_IMMDeviceEnumerator,
                              reinterpret_cast<void**>(&enumerator));
        if (FAILED(hr)) {
            if (errorOut)
                *errorOut = QStringLiteral("CoCreateInstance(MMDeviceEnumerator) failed");
            return false;
        }

        IMMDevice* device = nullptr;
        if (deviceId.isEmpty())
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        else
            hr = E_NOTIMPL;   ///< 特定デバイス ID の解決は将来拡張
        safeRelease(&enumerator);
        if (FAILED(hr)) {
            if (errorOut)
                *errorOut = QStringLiteral("GetDefaultAudioEndpoint failed");
            return false;
        }

        hr = device->Activate(kIID_IAudioClient, CLSCTX_ALL, nullptr,
                              reinterpret_cast<void**>(&audioClient_));
        safeRelease(&device);
        if (FAILED(hr)) {
            if (errorOut)
                *errorOut = QStringLiteral("IMMDevice::Activate failed");
            return false;
        }

        // 共有モード + イベント駆動。バッファ長は要求値を尊重させる。
        REFERENCE_TIME bufferDuration =
            REFERENCE_TIME(double(bufferFrames) / double(sampleRate) * 10000000.0);
        hr = audioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                      AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                      bufferDuration, 0, nullptr, nullptr);
        if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT || hr == E_INVALIDARG) {
            // WAVEFORMATEX 未指定の Initialize は Vista 系で失敗するため、
            // GetMixFormat ベースで再試行する。
            WAVEFORMATEX* mix = nullptr;
            if (SUCCEEDED(audioClient_->GetMixFormat(&mix)) && mix) {
                sampleRate_   = mix->nSamplesPerSec;
                channelCount_ = std::min(2, int(mix->nChannels));
                hr = audioClient_->Initialize(
                    AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                    bufferDuration, 0, mix, nullptr);
                CoTaskMemFree(mix);
            }
        } else {
            WAVEFORMATEX* mix = nullptr;
            if (SUCCEEDED(audioClient_->GetMixFormat(&mix)) && mix) {
                sampleRate_   = mix->nSamplesPerSec;
                channelCount_ = std::min(2, int(mix->nChannels));
                CoTaskMemFree(mix);
            }
        }
        if (FAILED(hr)) {
            if (errorOut)
                *errorOut = QStringLiteral("IAudioClient::Initialize failed: 0x%1")
                                .arg(uint(hr), 8, 16, QLatin1Char('0'));
            return false;
        }

        hr = audioClient_->SetEventHandle(eventHandle_);
        if (FAILED(hr)) {
            if (errorOut)
                *errorOut = QStringLiteral("SetEventHandle failed");
            return false;
        }

        hr = audioClient_->GetService(kIID_IAudioRenderClient,
                                      reinterpret_cast<void**>(&renderClient_));
        if (FAILED(hr)) {
            if (errorOut)
                *errorOut = QStringLiteral("GetService(IAudioRenderClient) failed");
            return false;
        }

        UINT32 frames = 0;
        audioClient_->GetBufferSize(&frames);
        bufferFrames_ = int(frames);

        // コールバックへ渡す非インターリーブ (プラナー) スクラッチを事前確保する。
        // RT スレッド内では確保を行わない。
        scratchData_.resize(size_t(bufferFrames_) * size_t(channelCount_));
        scratchPlanar_.resize(size_t(channelCount_));
        for (int c = 0; c < channelCount_; ++c)
            scratchPlanar_[size_t(c)] =
                scratchData_.data() + size_t(c) * size_t(bufferFrames_);

        return true;
    }

    void close() override
    {
        stopThread();
        safeRelease(&renderClient_);
        safeRelease(&audioClient_);

        if (eventHandle_) {
            CloseHandle(eventHandle_);
            eventHandle_ = nullptr;
        }
    }

    bool start() override
    {
        if (!audioClient_ || !renderClient_)
            return false;
        if (!eventHandle_) {
            eventHandle_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            audioClient_->SetEventHandle(eventHandle_);
        }
        running_.store(true, std::memory_order_release);

        if (FAILED(audioClient_->Start()))
            return false;

        thread_ = std::thread([this] { renderLoop(); });
        return true;
    }

    void stop() override
    {
        stopThread();
        if (audioClient_)
            audioClient_->Stop();
    }

    int     sampleRate() const override { return sampleRate_; }
    int     bufferFrames() const override { return bufferFrames_; }
    int64_t outputLatencySamples() const override
    {
        UINT32 padding = 0;
        if (audioClient_ && SUCCEEDED(audioClient_->GetCurrentPadding(&padding)))
            return int64_t(bufferFrames_) - int64_t(padding);
        return int64_t(bufferFrames_);
    }

private:
    void stopThread()
    {
        running_.store(false, std::memory_order_release);
        if (thread_.joinable()) {
            SetEvent(eventHandle_);
            thread_.join();
        }
    }

    void renderLoop()
    {
        // Pro Audio スレッド昇格。失敗しても続行する。
        DWORD taskIndex = 0;
        HANDLE task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

        while (running_.load(std::memory_order_acquire)) {
            DWORD wait = WaitForSingleObject(eventHandle_, 2000);
            if (wait != WAIT_OBJECT_0)
                continue;

            UINT32 padding = 0;
            if (FAILED(audioClient_->GetCurrentPadding(&padding)))
                break;
            const UINT32 available = UINT32(bufferFrames_) - padding;
            if (available == 0)
                continue;

            BYTE* data = nullptr;
            if (FAILED(renderClient_->GetBuffer(available, &data)))
                break;

            const int n = std::min(int(available), int(bufferFrames_));

            if (callback_ && n > 0) {
                // コールバックは「チャンネル配列へのポインタ配列」(プラナー) を要求する。
                // WASAPI 共有モードのステレオ float (インターリーブ) バッファへ
                // 変換するために、スクラッチへ書き込んでからコピーする。
                callback_(scratchPlanar_.data(), channelCount_, n, userData_);

                if (channelCount_ >= 2) {
                    const float* l = scratchPlanar_[0];
                    const float* r = scratchPlanar_[1];
                    for (int i = 0; i < n; ++i) {
                        data[size_t(i) * 2]       = l[i];
                        data[size_t(i) * 2 + 1]   = r[i];
                    }
                } else {
                    std::memset(data, 0, size_t(n) * size_t(channelCount_) * sizeof(float));
                }
            } else {
                // コールバック未登録: 無音を書き込む
                std::memset(data, 0, size_t(n) * size_t(channelCount_) * sizeof(float));
            }

            renderClient_->ReleaseBuffer(available, 0);
        }

        if (task)
            AvRevertMmThreadCharacteristics(task);
    }

    IAudioClient*      audioClient_ = nullptr;
    IAudioRenderClient* renderClient_ = nullptr;
    HANDLE             eventHandle_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    AudioCallback      callback_ = nullptr;
    void*              userData_ = nullptr;
    int                sampleRate_ = 48000;
    int                bufferFrames_ = 512;
    int                channelCount_ = 2;
    bool               comInitHere_ = false;
    std::atomic<bool>  running_{false};
    std::thread        thread_;

    // コールバックへ渡すプラナーバッファ (open() で事前確保)
    std::vector<float>  scratchData_;
    std::vector<float*> scratchPlanar_;
};

} // anonymous namespace

std::unique_ptr<IAudioDevice> createWasapiDevice()
{
    return std::make_unique<WasapiDevice>();
}

std::vector<AudioDeviceInfo> enumerateWasapiDevices()
{
    // 既定デバイスのみを報告する (個別列挙は将来拡張)。
    AudioDeviceInfo info;
    info.id          = QStringLiteral("wasapi-default");
    info.displayName = QStringLiteral("Default WASAPI Output");
    info.maxOutputChannels = 2;
    info.supportedSampleRates = {44100, 48000, 96000};
    info.isDefault   = true;
    return {info};
}

} // namespace yave::audio
