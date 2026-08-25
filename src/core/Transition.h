#pragma once

#include <QJsonObject>
#include <QString>
#include <QUuid>
#include <QVariantMap>

#include <array>
#include <cstdint>
#include <vector>

namespace yave {

/// クリップ境界に置くトランジション (3.10)。
///
/// クリップ同士は重ねない。Track が境界ごとに 0 個か 1 個だけ持つ。
struct Transition
{
    QUuid       id;
    QString     transitionId;        ///< "yave.trans.dissolve" 等
    QUuid       fromClipId;
    QUuid       toClipId;            ///< 端の境界では null (黒との合成)
    int64_t     centerFrame    = 0;  ///< 境界フレーム
    int64_t     durationFrames = 0;  ///< 全体長。前後へ半分ずつ伸びる
    QVariantMap params;

    int64_t startFrame() const { return centerFrame - durationFrames / 2; }
    int64_t endFrame()   const { return startFrame() + durationFrames; }
    bool    contains(int64_t f) const { return f >= startFrame() && f < endFrame(); }

    /// 0 = from が全面、1 = to が全面。区間外は 0 / 1 にクランプする。
    float progressAt(int64_t f) const;

    QJsonObject toJson() const;
    static Transition fromJson(const QJsonObject& o);
};

/// 組み込みトランジションの ID (3.10.2)。
namespace builtinTransition {
inline constexpr auto kDissolve    = "yave.trans.dissolve";
inline constexpr auto kFadeToBlack = "yave.trans.fadeToBlack";
inline constexpr auto kWipe        = "yave.trans.wipe";
inline constexpr auto kSlide       = "yave.trans.slide";
inline constexpr auto kPush        = "yave.trans.push";
} // namespace builtinTransition

struct TransitionDesc
{
    QString transitionId;
    QString displayNameKey;        ///< 翻訳キー (10章)
    QVariantMap defaultParams;
    bool    allowsMissingPartner = false;   ///< 端の境界にも置けるか
};

/// 組み込みトランジションの一覧。ライブラリパネル (1.7.5) が並べる。
const std::vector<TransitionDesc>& builtinTransitions();

const TransitionDesc* findTransitionDesc(const QString& transitionId);

/// TransitionPass のシェーダへ渡す mode 番号。未知なら dissolve (0)。
int transitionShaderMode(const QString& transitionId);

/// params を Render Thread へ渡す固定長 float 配列へ潰す。
/// スロットの意味は transition.frag の params と一致させること。
std::array<float, 4> resolveTransitionParams(const Transition& t);

/// fadeToBlack の中間色 (RGBA 0..1)。他のモードでは使わない。
std::array<float, 4> resolveTransitionColor(const Transition& t);

} // namespace yave
