#pragma once

#include "BlendMode.h"
#include "Rational.h"
#include "TrackType.h"

#include <QMatrix4x4>
#include <QRectF>
#include <QSize>
#include <QUuid>

#include <array>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace yave {

/// ビデオソース参照 (Render Thread が FrameCache へ要求を出すのに十分な情報)
struct VideoSourceRef
{
    QUuid   assetId;
    int64_t sourceFrameIndex = 0;   ///< ソース内フレーム (speed 適用済み)
    double  speed            = 1.0;
};

/// 字幕レンダリング参照
struct SubtitleRenderRef
{
    QUuid    clipId;                 ///< SubtitleClip の id
    int64_t  frameInClip      = 0;   ///< クリップ先頭からの相対フレーム
    uint64_t contentRevision  = 0;   ///< レイアウト / アトラスキャッシュのキー判定用
};

/// AI 生成物などのファイルソース参照
struct GeneratedSourceRef
{
    QString filePath;
    int64_t sourceFrameIndex = 0;
};

/// フィルタ 1 段分の解決済みパラメータ (3.9)。
/// Render Thread へ QVariantMap を渡さないため、固定長の float 配列に潰す。
/// スロットの意味は filterId ごとに 3.9.2 の表で定める。
struct ResolvedFilter
{
    QString              filterId;
    std::array<float, 8> params{};
};

/// トランジションに参加しているレイヤーの情報 (3.10)。
/// 同じ境界の 2 レイヤーが、同一の id / progress を持って必ず対で現れる。
struct TransitionRef
{
    QString              transitionId;
    float                progress   = 0.0f;   ///< 0 = from が全面、1 = to が全面
    bool                 isIncoming = false;  ///< true なら自分が to 側
    int                  shaderMode = 0;      ///< transition.frag の mode
    std::array<float, 4> params{};            ///< mode 依存 (角度 / 方向 など)
    std::array<float, 4> fillColor{};         ///< fadeToBlack の中間色
};

/// Render Thread へ渡す 1 レイヤー分の情報。
/// 値のコピーで受け渡すため、Timeline 本体への参照は持たない。
struct LayerItem
{
    QUuid            clipId;
    TrackType        trackType   = TrackType::Video;
    int              zIndex      = 0;          ///< 小さいほど背面
    BlendMode        blendMode   = BlendMode::Normal;
    float            opacity     = 1.0f;
    QMatrix4x4       transform;                ///< レイヤー変換 (位置/拡縮/回転)
    QRectF           cropRect{0.0, 0.0, 1.0, 1.0};

    /// このレイヤーに掛けるビデオフィルタ (3.9)。適用順は配列順。
    std::vector<ResolvedFilter>  filters;

    /// 境界のトランジションに参加している場合のみ値を持つ (3.10)。
    /// 対になるレイヤーは同じ zIndex を持つ。
    std::optional<TransitionRef> transition;

    /// 種別ごとの解決済み情報
    std::variant<std::monostate,
                 VideoSourceRef,
                 SubtitleRenderRef,
                 GeneratedSourceRef> source;
};

/// UI -> Render スレッドの受け渡し構造体。イミュータブル。
///
/// Timeline に読み書きロックを掛ける代わりに、UI スレッドが毎フレーム
/// スナップショットを生成して値渡しする。1 フレーム分は数百バイト程度。
struct RenderSnapshot
{
    int64_t                 frameIndex = 0;
    Rational                timebase{1001, 60000};
    QSize                   canvasSize{3840, 2160};
    uint64_t                timelineRevision = 0;
    std::vector<LayerItem>  layers;             ///< 背面 -> 前面 の順に格納済み
};

} // namespace yave
