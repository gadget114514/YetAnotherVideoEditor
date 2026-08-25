#pragma once

#include "MediaInfo.h"

#include "../core/Rational.h"

#include <QByteArray>
#include <QSize>
#include <QString>

#include <cstdint>
#include <memory>

namespace yave {

struct Rational;

} // namespace yave

// ---------------------------------------------------------------------------
// FFmpeg 型の前方宣言は**グローバルスコープ**で行うこと。
//
// 注意: extern "C" はリンケージ指定でありスコープを作らない。名前空間の中に
// 書くと ::AVFormatContext と yave::media::AVFormatContext が別型になり、
// libav* ヘッダとの照合に失敗する。
// ---------------------------------------------------------------------------
#if defined(YAVE_HAVE_FFMPEG)
extern "C" {
struct AVFrame;
struct AVPacket;
struct AVCodecContext;
struct AVFormatContext;
}
#endif

namespace yave::media {

/// FFmpeg の AVFrame / AVPacket / AVCodecContext 用 unique_ptr Deleter。
#if defined(YAVE_HAVE_FFMPEG)
struct AVFrameDeleter { void operator()(AVFrame* p) const noexcept; };
struct AVPacketDeleter { void operator()(AVPacket* p) const noexcept; };
struct AVCodecContextDeleter { void operator()(AVCodecContext* p) const noexcept; };
struct AVFormatContextDeleter
{
    // avformat_close_input は double ポインタを要求するため専用ラッパ
    void operator()(AVFormatContext* p) const noexcept;
};

using FramePtr        = std::unique_ptr<AVFrame, AVFrameDeleter>;
using PacketPtr       = std::unique_ptr<AVPacket, AVPacketDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using FormatContextPtr =
    std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;

FramePtr  makeFrame();
PacketPtr makePacket();
#endif

// ===========================================================================
//  VideoDecoder: 1 インスタンス = 1 クリップの順次 / シーク読み出し。
//
//  HW デコード (D3D11VA / NVDEC / QSV / VideoToolbox) は HwDeviceContext 経由で
//  設定する。FFmpeg 無効ビルドでは open() が常に失敗する。
// ===========================================================================

enum class HwAccel { None, D3D11VA, Nvdec, Qsv, VideoToolbox };

class VideoDecoder
{
public:
    struct OpenParams
    {
        QString filePath;
        int     streamIndex = -1;      ///< -1 = 自動選択
        Rational projectTimebase{1001, 60000};   ///< 尺換算用 (Rational を値で持つ)
        bool    allowHwAccel = true;
        int     threadCount  = 0;       ///< 0 = 自動
    };

    VideoDecoder();
    ~VideoDecoder();

    bool open(const OpenParams& params, QString* errorOut);
    void close();

    bool isOpen() const { return opened_; }

    /// 指定ソースフレームへシークする (キーフレーム単位。後続のデコードで確定)。
    bool seekToSourceFrame(int64_t sourceFrame);

    /// 次のフレームをデコードする。
    /// 成功時は RGBA8 (premultiplied) のピクセルを outPixels へ出力する。
    /// 戻り値 false は EOF もしくはエラー (hasError() で判別)。
    bool decodeNext(int64_t* sourceFrameOut, QByteArray* outPixels,
                    QSize* sizeOut);

    bool hasError() const { return error_; }

    MediaInfo info() const { return info_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool                  opened_ = false;
    bool                  error_  = false;
    MediaInfo             info_;
};

/// 音声デコード。48kHz stereo float へ正規化して出力する。
class AudioDecoder
{
public:
    explicit AudioDecoder(const QString& filePath);
    ~AudioDecoder();

    bool open(QString* errorOut);

    /// 最大 maxFrames サンプル分を読む。戻り値は実際に読めたサンプル数。
    /// outFrames には interleaved float32 stereo を書く。
    int64_t read(float* interleavedOut, int64_t maxFrames);

    int64_t totalFrames() const { return totalFrames_; }
    bool hasError() const { return error_; }

private:
    struct Impl;
    QString filePath_;
    std::unique_ptr<Impl> impl_;
    int64_t totalFrames_ = 0;
    bool    error_ = false;
};

} // namespace yave::media
