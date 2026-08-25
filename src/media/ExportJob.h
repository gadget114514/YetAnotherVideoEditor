#pragma once

#include "MediaInfo.h"
#include "VideoDecoder.h"

#include <QSize>
#include <QString>

#include <cstdint>

namespace yave {

struct Rational;

} // namespace yave

namespace yave::media {

class VideoEncoder;
class AudioEncoder;
class Muxer;

using ExportProgressFn = bool (*)(double progress01, void* userData);

struct ExportSettings
{
    QString outputPath;                 ///< 絶対パス (書き出し先)
    QSize   resolution{3840, 2160};
    Rational frameRate{1001, 60000};
    int64_t startFrame = 0;
    int64_t endFrame   = 0;    // 映像
    QString videoCodec = QStringLiteral("libx264");
    int     videoBitrateKbps = 40000;
    QString videoPreset = QStringLiteral("slow");    ///< x264 / x265 プリセット

    // 音声
    bool    exportAudio = true;
    QString audioCodec  = QStringLiteral("aac");
    int     audioBitrateKbps = 320;
};

/// 書き出しジョブ。プレビューとは別経路で動作する (3.4.2)。
///
/// 合成コードは RhiCompositor のものを再利用し、
/// プレゼントの代わりに readback -> エンコーダへ渡す。
class ExportJob
{
public:
    explicit ExportJob(ExportSettings settings);

    /// 同期実行。progress コールバックが false を返すと中断する。
    /// 戻り値はエラーメッセージ (成功時は空文字列)。
    QString run(ExportProgressFn progress, void* userData);

    void cancel() { cancelled_ = true; }

    const ExportSettings& settings() const { return settings_; }

private:
    ExportSettings settings_;
    bool           cancelled_ = false;
};

} // namespace yave::media
