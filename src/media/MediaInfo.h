#pragma once

#include <QSize>
#include <QString>

namespace yave::media {

/// 素材ファイルのストリーム情報。
struct MediaInfo
{
    bool     ok = false;
    QString  error;

    // --- 映像 ---
    bool    hasVideo = false;
    QSize   resolution;
    /// ソースの fps を有理数で表したもの (num / den = fps の分子/分母)
    int64_t frameRateNum = 0;
    int64_t frameRateDen = 1;
    int64_t durationFrames = 0;      ///< プロジェクトタイムベース換算ではない。ソース内フレーム数

    // --- 音声 ---
    bool    hasAudio = false;
    int     audioSampleRate = 0;
    int     audioChannels   = 0;
    int64_t audioDurationFrames = 0;
};

} // namespace yave::media
