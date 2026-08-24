# 4. ビデオエンジン (FFmpeg + GPU)

[← 目次に戻る](../design.md)

---

## 4.1 方針

- FFmpeg の C API を直接使う。`QMediaPlayer` / `libavdevice` は使わない。
- HW デコードを既定とし、失敗時のみ SW デコードへフォールバックする。
  フォールバックしたことはユーザーに見える形で通知する(4K で SW デコードすると
  明確に重くなるため、原因が分からないと調査できない)。
- **デコード済みフレームを可能な限り GPU メモリから出さない**。
  `AVFrame` の HW サーフェスを `QRhiTexture` として直接ラップする。

## 4.2 RAII ラッパ

FFmpeg のリソースは生ポインタと解放関数の対で管理されるため、必ずラッパを通す。

```cpp
// src/media/FFmpegRaii.h
namespace yave::media {

struct AVFrameDeleter        { void operator()(AVFrame* p)        const { av_frame_free(&p); } };
struct AVPacketDeleter       { void operator()(AVPacket* p)       const { av_packet_free(&p); } };
struct AVCodecContextDeleter { void operator()(AVCodecContext* p) const { avcodec_free_context(&p); } };
struct AVFormatContextDeleter{ void operator()(AVFormatContext* p)const { avformat_close_input(&p); } };
struct AVBufferRefDeleter    { void operator()(AVBufferRef* p)    const { av_buffer_unref(&p); } };
struct SwsContextDeleter     { void operator()(SwsContext* p)     const { sws_freeContext(p); } };

using FramePtr        = std::unique_ptr<AVFrame,         AVFrameDeleter>;
using PacketPtr       = std::unique_ptr<AVPacket,        AVPacketDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext,  AVCodecContextDeleter>;
using FormatContextPtr= std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using BufferRefPtr    = std::unique_ptr<AVBufferRef,     AVBufferRefDeleter>;

/// FFmpeg のエラーコードを可読文字列にする
QString avErrorString(int err);

} // namespace yave::media
```

> `av_frame_free` は `AVFrame**` を取り、ポインタを null にする。`unique_ptr` の
> `Deleter` はコピーを受け取るのでローカル変数のアドレスを渡す形になるが、これで正しく動く。

## 4.3 HW デバイスコンテキストの選択

```cpp
// src/media/HwDeviceContext.h
namespace yave::media {

enum class HwAccel { None, D3D11VA, Cuda, Qsv, VideoToolbox };

class HwDeviceContext
{
public:
    /// プラットフォームとコーデックに応じた優先順位で HW デバイスを試す。
    /// 成功した最初のものを返す。すべて失敗したら HwAccel::None のインスタンスを返す。
    static std::shared_ptr<HwDeviceContext> createBest(AVCodecID codecId);

    static std::shared_ptr<HwDeviceContext> create(HwAccel accel);

    HwAccel      accel()  const { return accel_; }
    AVBufferRef* ref()    const { return deviceRef_.get(); }   // 所有権は移動しない
    AVPixelFormat hwPixelFormat() const;                        // AV_PIX_FMT_D3D11 等

private:
    HwAccel        accel_ = HwAccel::None;
    BufferRefPtr   deviceRef_;
};

} // namespace yave::media
```

### 4.3.1 優先順位

| プラットフォーム | 優先順位 |
|---|---|
| Windows + NVIDIA | `Cuda` (NVDEC) → `D3D11VA` → `Qsv` → SW |
| Windows + Intel | `Qsv` → `D3D11VA` → SW |
| Windows + AMD | `D3D11VA` → SW |
| macOS | `VideoToolbox` → SW |

GPU ベンダの判定は `QRhi` が選択したアダプタ情報 (`QRhi::driverInfo()`) から行う。
**RHI が使っているのと同じ GPU でデコードすること**が重要で、
マルチ GPU 環境で別 GPU にデコードさせるとゼロコピーが成立せず、
PCIe 経由のコピーが挟まって 4K では致命的に遅くなる。

### 4.3.2 D3D11VA を選ぶ場合の追加要件

RHI が D3D11 バックエンドの場合、**FFmpeg に RHI と同じ `ID3D11Device` を使わせる**。
別デバイスで作られたテクスチャは共有ハンドル経由でしか渡せず、同期が煩雑になる。

```cpp
// D3D11 デバイスを FFmpeg に注入する
AVBufferRef* deviceRef = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
auto* ctx    = reinterpret_cast<AVHWDeviceContext*>(deviceRef->data);
auto* d3dCtx = static_cast<AVD3D11VADeviceContext*>(ctx->hwctx);

const QRhiD3D11NativeHandles* nh =
    static_cast<const QRhiD3D11NativeHandles*>(rhi->nativeHandles());
d3dCtx->device = static_cast<ID3D11Device*>(nh->dev);
d3dCtx->device->AddRef();                 // FFmpeg 側が Release するため

if (av_hwdevice_ctx_init(deviceRef) < 0) { /* フォールバック */ }
```

## 4.4 VideoDecoder

```cpp
// src/media/VideoDecoder.h
namespace yave::media {

struct DecodedFrame
{
    int64_t      frameIndex = -1;     // プロジェクトタイムベース上のフレーム番号
    FramePtr     frame;               // HW サーフェス or SW フレーム
    HwAccel      accel = HwAccel::None;
    QSize        size;
    AVColorSpace colorSpace = AVCOL_SPC_BT709;
    AVColorRange colorRange = AVCOL_RANGE_MPEG;
};

class VideoDecoder
{
public:
    struct OpenParams {
        QString  filePath;
        int      streamIndex = -1;          // -1 = 最良のビデオストリームを自動選択
        Rational projectTimebase;
        bool     allowHwAccel = true;
        int      threadCount  = 0;          // 0 = 自動
    };

    bool open(const OpenParams& p, QString* errorOut = nullptr);
    void close();

    /// 指定フレームへシークする。GOP 先頭へシークしてから前方デコードする(精密シーク)。
    bool seekTo(int64_t projectFrame);

    /// 次のフレームをデコードして返す。EOF なら nullopt。
    std::optional<DecodedFrame> decodeNext();

    /// 指定フレームを取得する。必要なら内部でシーク+前方デコードを行う。
    std::optional<DecodedFrame> frameAt(int64_t projectFrame);

    int64_t  durationFrames() const { return durationFrames_; }
    Rational sourceTimebase() const { return srcTimebase_; }
    HwAccel  activeAccel()    const { return hwDevice_ ? hwDevice_->accel() : HwAccel::None; }
    bool     usedFallback()   const { return fallbackUsed_; }

private:
    static AVPixelFormat getFormatCallback(AVCodecContext* ctx, const AVPixelFormat* fmts);

    FormatContextPtr fmt_;
    CodecContextPtr  codec_;
    std::shared_ptr<HwDeviceContext> hwDevice_;
    int      streamIndex_    = -1;
    Rational srcTimebase_;
    Rational projectTimebase_;
    int64_t  durationFrames_ = 0;
    int64_t  nextFrame_      = 0;      // 次に decodeNext() が返すフレーム番号
    bool     fallbackUsed_   = false;
};

} // namespace yave::media
```

### 4.4.1 HW ピクセルフォーマットのネゴシエーション

`get_format` コールバックで HW フォーマットを選ばないと、FFmpeg は SW フォーマットに
落としてしまう。

```cpp
AVPixelFormat VideoDecoder::getFormatCallback(AVCodecContext* ctx, const AVPixelFormat* fmts)
{
    auto* self = static_cast<VideoDecoder*>(ctx->opaque);
    if (self->hwDevice_) {
        const AVPixelFormat want = self->hwDevice_->hwPixelFormat();
        for (const AVPixelFormat* p = fmts; *p != AV_PIX_FMT_NONE; ++p) {
            if (*p == want)
                return *p;
        }
    }
    // HW フォーマットが提示されなかった -> SW フォールバック
    self->fallbackUsed_ = true;
    qCWarning(lcMedia) << "HW pixel format unavailable, falling back to software decode";
    return fmts[0];
}
```

`open()` 内での設定:

```cpp
codec_->opaque      = this;
codec_->get_format  = &VideoDecoder::getFormatCallback;
if (hwDevice_)
    codec_->hw_device_ctx = av_buffer_ref(hwDevice_->ref());
codec_->thread_count = params.threadCount;
codec_->thread_type  = FF_THREAD_FRAME | FF_THREAD_SLICE;
```

### 4.4.2 精密シーク

```cpp
bool VideoDecoder::seekTo(int64_t projectFrame)
{
    // プロジェクトフレーム -> ソースの pts へ
    const int64_t srcPts = rescaleFrames(projectFrame, projectTimebase_, srcTimebase_,
                                         RoundMode::Nearest);
    // AVSEEK_FLAG_BACKWARD で「その pts 以前の最も近いキーフレーム」へ
    if (av_seek_frame(fmt_.get(), streamIndex_, srcPts, AVSEEK_FLAG_BACKWARD) < 0)
        return false;
    avcodec_flush_buffers(codec_.get());

    // キーフレームから目標まで前方デコードして捨てる
    while (auto f = decodeNextRaw()) {
        if (f->frameIndex >= projectFrame) {
            pendingFrame_ = std::move(f);   // 目標フレームは保持して次回返す
            nextFrame_    = projectFrame;
            return true;
        }
    }
    return false;
}
```

> **GOP が長い素材 (H.265 の 250 フレーム GOP など) では、シークのたびに
> 最大 250 フレーム分のデコードが走る**。これはスクラブ操作で致命的になるため、
> 4.7 のプロキシ生成でこれを回避する。

## 4.5 ゼロコピー: HW サーフェス → QRhiTexture

### 4.5.1 Windows / D3D11VA

`AVFrame::data[0]` が `ID3D11Texture2D*`、`data[1]` が配列インデックス
(`intptr_t` にキャストされたスライス番号) になっている。
D3D11VA のデコード出力は**テクスチャ配列**なので、そのままでは
シェーダリソースビューを個別に作る必要がある。

```cpp
// src/platform/win/D3D11Interop.cpp
QRhiTexture* wrapD3D11Frame(QRhi* rhi, const AVFrame* frame, TexturePool& pool)
{
    auto* srcTex = reinterpret_cast<ID3D11Texture2D*>(frame->data[0]);
    const UINT arraySlice = static_cast<UINT>(reinterpret_cast<intptr_t>(frame->data[1]));

    // NV12 のテクスチャ配列から 1 スライスを取り出して共有テクスチャへコピーする。
    // GPU 内コピーであり PCIe を経由しないため「ゼロコピー」として扱う。
    // (D3D11 の制約上、デコーダ出力の配列テクスチャは
    //  D3D11_BIND_SHADER_RESOURCE を持たないことが多く、直接サンプルできない)
    QRhiTexture* dst = pool.acquire({QSize(frame->width, frame->height),
                                     QRhiTexture::RGBA8,       // 実体は NV12 -> 2 プレーン扱い
                                     QRhiTexture::UsedAsTransferSource});

    const QRhiD3D11TextureNativeHandles* dstNh =
        static_cast<const QRhiD3D11TextureNativeHandles*>(dst->nativeHandles());
    auto* dstTex = static_cast<ID3D11Texture2D*>(dstNh->texture);

    ID3D11DeviceContext* ctx = /* RHI と同じ immediate context */;
    ctx->CopySubresourceRegion(dstTex, 0, 0, 0, 0, srcTex, arraySlice, nullptr);
    return dst;
}
```

実装上は **NV12 を Y プレーンと UV プレーンの 2 テクスチャとして扱う**のが素直。

```cpp
struct HwTexturePair {
    QRhiTexture* luma   = nullptr;   // R8,    width x height
    QRhiTexture* chroma = nullptr;   // RG8,   width/2 x height/2
};
```

`yuv_to_rgb.frag` でこの 2 枚をサンプルして RGB に変換し、合成パイプラインへ渡す。

### 4.5.2 macOS / VideoToolbox

`AVFrame::data[3]` が `CVPixelBufferRef`。`IOSurface` 経由で `MTLTexture` を作れる。
**こちらは真のゼロコピー**で、コピーが 1 回も発生しない。

```objc
// src/platform/mac/MetalInterop.mm
HwTexturePair wrapVideoToolboxFrame(QRhi* rhi, const AVFrame* frame, TexturePool& pool)
{
    CVPixelBufferRef pb = reinterpret_cast<CVPixelBufferRef>(frame->data[3]);
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(pb);

    const QRhiMetalNativeHandles* nh =
        static_cast<const QRhiMetalNativeHandles*>(rhi->nativeHandles());
    id<MTLDevice> device = (__bridge id<MTLDevice>)nh->dev;

    // Plane 0: Y (R8Unorm)
    MTLTextureDescriptor* dy = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                     width:IOSurfaceGetWidthOfPlane(surface, 0)
                                    height:IOSurfaceGetHeightOfPlane(surface, 0)
                                 mipmapped:NO];
    dy.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> yTex = [device newTextureWithDescriptor:dy iosurface:surface plane:0];

    // Plane 1: CbCr (RG8Unorm)
    MTLTextureDescriptor* dc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatRG8Unorm
                                     width:IOSurfaceGetWidthOfPlane(surface, 1)
                                    height:IOSurfaceGetHeightOfPlane(surface, 1)
                                 mipmapped:NO];
    dc.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> cTex = [device newTextureWithDescriptor:dc iosurface:surface plane:1];

    HwTexturePair out;
    out.luma   = rhi->newTexture(QRhiTexture::R8,  ...);
    out.chroma = rhi->newTexture(QRhiTexture::RG8, ...);
    out.luma  ->createFrom({quint64(yTex), 0});   // QRhiTexture::NativeTexture
    out.chroma->createFrom({quint64(cTex), 0});
    return out;
}
```

> **重要: `CVPixelBufferRef` のライフタイム**。`AVFrame` を解放すると `CVPixelBuffer` も
> 解放され、そこから作った `MTLTexture` が無効になる。GPU がそのフレームを使い終わるまで
> `AVFrame` を生かしておく必要がある。`RhiCompositor` はフレーム submit 完了まで
> `DecodedFrame` を `inFlightFrames_[currentFrameSlot]` に保持し、
> `QRhi::FramesInFlight` (通常 2) 分遅れて解放する。

### 4.5.3 SW フォールバック経路

```cpp
// HW サーフェスを CPU へ読み戻す (遅い。最終手段)
FramePtr swFrame(av_frame_alloc());
if (av_hwframe_transfer_data(swFrame.get(), hwFrame, 0) < 0) { /* error */ }
// -> QRhiTextureUploadDescription でアップロード
```

4K NV12 の 1 フレームは約 12MB。60fps で読み戻すと 720MB/s の PCIe 転送になり、
実測でフレーム時間が 5〜8ms 増える。**Adaptive Quality の Level 判定に
`usedFallback()` を反映させ、フォールバック時は最初からプレビュー解像度を落とす。**

## 4.6 フレームキャッシュとデコードワーカ

### 4.6.1 FrameCache

```cpp
// src/media/FrameCache.h
class FrameCache
{
public:
    struct Key { QUuid assetId; int64_t frameIndex; };

    explicit FrameCache(qint64 budgetBytes = 3LL * 1024 * 1024 * 1024);

    std::shared_ptr<DecodedFrame> get(const Key& k);      // ヒットで LRU 更新
    void put(const Key& k, std::shared_ptr<DecodedFrame> f);

    /// 再生ヘッドから遠いフレームを優先的に捨てる。
    /// 単純な LRU だと「シークで飛んだ先を読んだ直後に、直前まで再生していた
    /// 領域が全部捨てられる」ため、再生位置からの距離も評価に入れる。
    void evictFor(int64_t playheadFrame, qint64 needBytes);

    void clear(const QUuid& assetId);   // 素材のプロキシ切替時など
};
```

退避スコア: `score = (現在フレームからの距離) * w1 + (最終アクセスからの経過) * w2`。
スコアの大きいものから捨てる。

### 4.6.2 先読み (look-ahead)

```
再生中: playhead + 1 .. playhead + N フレームを先読み (N = 30 が既定 = 0.5 秒)
停止中: playhead ± 15 フレームを先読み (スクラブ操作に備える)
```

先読み要求は `DecodeWorkerPool` のキューに入れる。キューは**再生ヘッドからの距離で
優先度付き**にし、シークが発生したら古い要求を破棄する
(破棄しないと、シーク後に不要になったフレームのデコードで CPU が埋まる)。

```cpp
class DecodeWorkerPool
{
public:
    struct Request {
        QUuid   assetId;
        int64_t frameIndex;
        int     priority;      // 小さいほど優先
        uint64_t generation;   // シークごとにインクリメント。古い世代は破棄
    };

    void submit(const Request& r);
    void invalidateOlderThan(uint64_t generation);
    void shutdown();
};
```

## 4.7 プロキシメディア

4K H.265 のような重い素材をそのまま編集するのは現実的でないため、プロキシを生成する。

| 項目 | 設定 |
|---|---|
| 解像度 | 元の 1/2 (2160p → 1080p) または固定 1280x720 (ユーザー選択) |
| コーデック | **All-Intra** (DNxHR LB / ProRes Proxy / MJPEG)。GOP=1 でシークが即座 |
| 音声 | PCM 16bit(そのまま) |
| 生成 | バックグラウンド。`ffmpeg` 相当の処理を `libav*` で自前実装 |
| 保存先 | `.yave_cache/proxy/<assetId>.mov` |

プレビューはプロキシ、書き出しは元素材、という切替を `AssetLibrary` が担う。

```cpp
class AssetLibrary
{
public:
    enum class Quality { Proxy, Full };
    QString resolvePath(const QUuid& assetId, Quality q) const;
    bool    hasProxy(const QUuid& assetId) const;
    void    requestProxyAsync(const QUuid& assetId);
signals:
    void proxyReady(const QUuid& assetId);
};
```

> **これを最初から設計に入れる理由**: 「4K/60p リアルタイムプレビュー」という要件を
> 素材直読みだけで満たすのは、長 GOP 素材では HW デコードを使っても難しい。
> プロキシは回避策ではなく、プロ用エディタでは標準的な設計要素である。

## 4.8 エンコードと書き出し

### 4.8.1 ExportJob

```cpp
// src/media/ExportJob.h
struct ExportSettings
{
    QString      outputPath;
    QSize        resolution{3840, 2160};
    Rational     frameRate = timebase::Fps59_94;
    AVCodecID    videoCodec = AV_CODEC_ID_H264;
    QString      videoEncoderName;      // "h264_nvenc" / "h264_qsv" / "h264_videotoolbox" / "libx264"
    int64_t      videoBitrate = 40'000'000;
    QString      preset;                // エンコーダ依存
    AVCodecID    audioCodec = AV_CODEC_ID_AAC;
    int          audioBitrate = 320'000;
    int          sampleRate  = 48000;
    int          channels    = 2;
    TimeRange    range;                 // 書き出し区間 (In/Out)
    bool         useHwEncode = true;
    bool         burnSubtitles = true;  // false なら別途 .srt を出力
};

class ExportJob : public QObject
{
    Q_OBJECT
public:
    void start(const ExportSettings& s, Timeline* timeline);
    void cancel();
signals:
    void progress(int64_t doneFrames, int64_t totalFrames);
    void finished(bool ok, const QString& error);
};
```

### 4.8.2 書き出しの流れ

```
for frame in range:
    snapshot = timeline->buildSnapshot(frame)          // プレビューと同じコード
    compositor->renderToOffscreen(snapshot, fullResRt) // プレビューと同じコード
    rgba = compositor->readback(fullResRt)             // ここだけプレビューと違う
    encoder->encodeVideoFrame(rgba, frame)
audioEngine->renderOffline(range) -> encoder->encodeAudio(...)
muxer->finalize()
```

**プレビューと書き出しで合成コードを共有する**ことが品質保証上もっとも重要。
別実装にすると「プレビューと書き出し結果が違う」という最悪のバグクラスを生む。

### 4.8.3 エンコーダ名のマッピング

```cpp
QString pickVideoEncoder(AVCodecID codec, HwAccel accel)
{
    switch (codec) {
    case AV_CODEC_ID_H264:
        if (accel == HwAccel::Cuda)          return "h264_nvenc";
        if (accel == HwAccel::Qsv)           return "h264_qsv";
        if (accel == HwAccel::VideoToolbox)  return "h264_videotoolbox";
        return "libx264";
    case AV_CODEC_ID_HEVC:
        if (accel == HwAccel::Cuda)          return "hevc_nvenc";
        if (accel == HwAccel::Qsv)           return "hevc_qsv";
        if (accel == HwAccel::VideoToolbox)  return "hevc_videotoolbox";
        return "libx265";
    case AV_CODEC_ID_PRORES:
        if (accel == HwAccel::VideoToolbox)  return "prores_videotoolbox";
        return "prores_ks";
    default:
        return {};
    }
}
```

エンコーダ固有オプションは `AVDictionary` で渡す。プリセット名がエンコーダごとに
異なる (`libx264` の `slow` と `h264_nvenc` の `p6` など) ため、
UI では「品質: 低/中/高/最高」という抽象レベルで選ばせ、
`EncoderPresetMapper` が実際のオプションへ変換する。

## 4.9 カラーマネジメント

| 入力 | 扱い |
|---|---|
| BT.709 limited (一般的な SDR 動画) | シェーダで full range へ展開して線形 RGB に |
| BT.601 | 同上 (行列が異なる) |
| BT.2020 PQ (HDR10) | PQ EOTF を適用して線形化。SDR プレビュー時は Hable トーンマップ |
| sRGB 静止画 | sRGB EOTF |

合成は**線形空間**で行い、最後にプレビュー / 出力のトランスファ関数を適用する。

> 線形空間で合成する理由: ガンマ空間のまま加算・乗算すると、クロスフェードの中間で
> 明るさが不自然に沈む。これはプロ用途では許容されない。

```glsl
// yuv_to_rgb.frag (抜粋)
const mat3 kBt709 = mat3( 1.0,      1.0,      1.0,
                          0.0,     -0.1873,   1.8556,
                          1.5748,  -0.4681,   0.0);

vec3 yuvToLinear(float y, vec2 cbcr, int space, int range)
{
    if (range == 0) {                        // limited -> full
        y     = (y * 255.0 - 16.0)  / 219.0;
        cbcr  = (cbcr * 255.0 - 16.0) / 224.0;
    }
    cbcr -= 0.5;
    vec3 rgb = kBt709 * vec3(y, cbcr.x, cbcr.y);
    return pow(max(rgb, 0.0), vec3(2.2));    // 簡易。厳密には BT.1886
}
```
