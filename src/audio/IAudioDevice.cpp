#include "IAudioDevice.h"

#if defined(YAVE_ENABLE_WASAPI)
namespace yave::audio {
std::unique_ptr<IAudioDevice> createWasapiDevice();
std::vector<AudioDeviceInfo>  enumerateWasapiDevices();
}
#endif

#if defined(YAVE_ENABLE_COREAUDIO)
namespace yave::audio {
std::unique_ptr<IAudioDevice> createCoreAudioDevice();
std::vector<AudioDeviceInfo>  enumerateCoreAudioDevices();
}
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace yave::audio {

// ===========================================================================
//  NullAudioDevice: デバイス実装が無い環境向けの無音フォールバック。
//   開発環境 / CI でエンジン単体のテストを可能にするために使う。
// ===========================================================================

namespace {

class NullAudioDevice final : public IAudioDevice
{
public:
    ~NullAudioDevice() override { close(); }

    bool open(const QString&, int sampleRate, int bufferFrames,
              AudioCallback cb, void* userData, QString*) override
    {
        sampleRate_ = sampleRate > 0 ? sampleRate : 48000;
        bufferFrames_ = std::max(64, bufferFrames);
        cb_       = cb;
        userData_ = userData;

        scratchData_.assign(size_t(bufferFrames_) * 2, 0.0f);
        scratchPlanar_ = { scratchData_.data(), scratchData_.data() + bufferFrames_ };
        return true;
    }

    void close() override { stop(); }

    bool start() override
    {
        stop();
        running_.store(true, std::memory_order_release);
        thread_ = std::thread([this] {
            const int block = bufferFrames_;
            const auto blockDur = std::chrono::microseconds(
                int64_t(double(block) / double(sampleRate_) * 1000000.0));
            while (running_.load(std::memory_order_acquire)) {
                const auto t0 = std::chrono::steady_clock::now();
                if (cb_) {
                    std::fill(scratchData_.begin(), scratchData_.end(), 0.0f);
                    cb_(scratchPlanar_.data(), 2, block, userData_);
                }
                const auto elapsed = std::chrono::steady_clock::now() - t0;
                if (elapsed < blockDur) {
                    std::this_thread::sleep_for(blockDur - elapsed);
                }
            }
        });
        return true;
    }

    void stop() override
    {
        running_.store(false, std::memory_order_release);
        if (thread_.joinable())
            thread_.join();
    }

    int     sampleRate() const override { return sampleRate_; }
    int     bufferFrames() const override { return bufferFrames_; }
    int64_t outputLatencySamples() const override
    { return int64_t(bufferFrames_); }

private:
    AudioCallback       cb_ = nullptr;
    void*               userData_ = nullptr;
    int                 sampleRate_ = 48000;
    int                 bufferFrames_ = 512;
    std::atomic<bool>   running_{false};
    std::thread         thread_;
    std::vector<float>  scratchData_;
    std::vector<float*> scratchPlanar_;
};

} // anonymous namespace

std::unique_ptr<IAudioDevice> IAudioDevice::create()
{
#if defined(YAVE_ENABLE_WASAPI)
    if (auto dev = createWasapiDevice())
        return dev;
#endif
#if defined(YAVE_ENABLE_COREAUDIO)
    if (auto dev = createCoreAudioDevice())
        return dev;
#endif
    return std::make_unique<NullAudioDevice>();
}

std::vector<AudioDeviceInfo> IAudioDevice::enumerate()
{
#if defined(YAVE_ENABLE_WASAPI)
    return enumerateWasapiDevices();
#endif
    return {};
}

} // namespace yave::audio
