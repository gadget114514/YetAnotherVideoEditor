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

namespace yave::audio {

// ===========================================================================
//  NullAudioDevice: デバイス実装が無い環境向けの無音フォールバック。
//   開発環境 / CI でエンジン単体のテストを可能にするために使う。
// ===========================================================================

namespace {

class NullAudioDevice final : public IAudioDevice
{
public:
    bool open(const QString&, int sampleRate, int bufferFrames,
              AudioCallback cb, void* userData, QString*) override
    {
        sampleRate_ = sampleRate;
        bufferFrames_ = std::max(64, bufferFrames);
        cb_       = cb;
        userData_ = userData;
        return true;
    }

    void close() override {}

    bool start() override
    {
        // 実タイマーの代わりに「呼ばれない」だけのデバイスとして振る舞う。
        // テストは rtCallback を直接叩くことでクロック進行を検証できる。
        running_ = true;
        return true;
    }

    void stop() override { running_ = false; }

    int     sampleRate() const override { return sampleRate_; }
    int     bufferFrames() const override { return bufferFrames_; }
    int64_t outputLatencySamples() const override
    { return int64_t(bufferFrames_); }

private:
    AudioCallback cb_ = nullptr;
    void*         userData_ = nullptr;
    int           sampleRate_ = 48000;
    int           bufferFrames_ = 512;
    bool          running_ = false;
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
