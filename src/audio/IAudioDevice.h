#pragma once

#include "../core/TimeRange.h"

#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace yave::audio {

struct AudioDeviceInfo
{
    QString id;
    QString displayName;
    int     maxOutputChannels = 2;
    std::vector<int> supportedSampleRates;
    bool    isDefault = false;
};

/// RT コールバック。この関数の中で許されるのは:
///   - 事前確保済みバッファへの読み書き
///   - アトミック操作
///   - VST3 プラグインの process() 呼び出し
/// 禁止: malloc/free, mutex, Qt シグナル, ファイル I/O, 例外
using AudioCallback = void (*)(float* const* outputChannels,
                               int numChannels,
                               int numFrames,
                               void* userData) noexcept;

/// オーディオ出力デバイスの抽象。
///
/// QAudioSink を使わない理由: Qt Multimedia のオーディオ出力はバッファ長の
/// 細かい制御ができず、実測で 50〜100ms のレイテンシになる。
/// VST3 のリアルタイム処理には不足。
class IAudioDevice
{
public:
    virtual ~IAudioDevice() = default;

    static std::vector<AudioDeviceInfo> enumerate();

    /// プラットフォームごとの実装を返す。
    ///   Windows : WasapiDevice (共有モード既定)
    ///   macOS   : CoreAudioDevice (AudioUnit HALOutput)
    ///   それ以外 / 失敗時 : NullAudioDevice (無音ループ)
    static std::unique_ptr<IAudioDevice> create();

    virtual bool open(const QString& deviceId, int sampleRate, int bufferFrames,
                      AudioCallback cb, void* userData, QString* errorOut) = 0;
    virtual void close() = 0;
    virtual bool start() = 0;
    virtual void stop()  = 0;

    virtual int     sampleRate()    const = 0;
    virtual int     bufferFrames()  const = 0;
    virtual int64_t outputLatencySamples() const = 0;
};

} // namespace yave::audio
