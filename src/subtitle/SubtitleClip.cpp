#include "SubtitleClip.h"
#include "SubtitleEffectFactoryBridge.h"
#include "SubtitleStylePreset.h"

#include <yave/sdk/ISubtitleEffect.h>

#include "../core/RenderSnapshot.h"
#include "../core/Track.h"

namespace yave::subtitle {

SubtitleClip::SubtitleClip() = default;
SubtitleClip::~SubtitleClip() = default;

SubtitleClip::SubtitleClip(const SubtitleClip& other)
    : Clip(other)   ///< 基底の共通部分をコピー (id も複製。clone で振り直す)
    , text_(other.text_)
    , stylePresetId_(other.stylePresetId_)
    , styleOverride_(other.styleOverride_)
    , wordTimings_(other.wordTimings_)
    , contentRevision_(other.contentRevision_)
{
    // エフェクトスタックの永続データのみコピーする
    effects_.reserve(other.effects_.size());
    for (const auto& inst : other.effects_) {
        SubtitleEffectInstance copy;
        copy.instanceId = inst.instanceId;
        copy.effectId   = inst.effectId;
        copy.pluginId   = inst.pluginId;
        copy.enabled    = inst.enabled;
        copy.params     = inst.params;
        copy.missing    = inst.missing;
        effects_.push_back(std::move(copy));
    }
}

std::shared_ptr<Clip> SubtitleClip::clone() const
{
    auto c = std::make_shared<SubtitleClip>(*this);
    c->setId(QUuid::createUuid());

    // エフェクトは新規生成する (prepare() の前計算結果はインスタンス固有)。
    auto& stack = c->mutableEffectStack();
    for (auto& inst : stack)
        inst.effect = createEffectViaFactory(inst.effectId);

    return c;
}

void SubtitleClip::setText(const SubtitleText& t)
{
    text_ = t;
    bumpContentRevision();
}

void SubtitleClip::setPlainText(const QString& s)
{
    text_.setPlain(s);
    bumpContentRevision();
}

void SubtitleClip::setStylePresetId(const QString& id)
{
    stylePresetId_ = id;
    bumpContentRevision();
}

void SubtitleClip::setStyleOverride(const SubtitleStyleDiff& d)
{
    styleOverride_ = d;
    bumpContentRevision();
}

void SubtitleClip::clearStyleOverride()
{
    styleOverride_ = {};
    bumpContentRevision();
}

SubtitleStyle SubtitleClip::resolvedStyle(const SubtitleStylePresetTable& presets) const
{
    const SubtitleStyle base = presets.preset(stylePresetId_).style;
    return SubtitleStyleDiff::apply(base, styleOverride_);
}

// ===========================================================================
//  エフェクトスタック
// ===========================================================================

void SubtitleClip::addEffect(SubtitleEffectInstance inst)
{
    effects_.push_back(std::move(inst));
    bumpContentRevision();
}

void SubtitleClip::insertEffect(int index, SubtitleEffectInstance inst)
{
    index = qBound(0, index, int(effects_.size()));
    effects_.insert(effects_.begin() + index, std::move(inst));
    bumpContentRevision();
}

void SubtitleClip::removeEffect(int index)
{
    if (index < 0 || index >= int(effects_.size()))
        return;
    effects_.erase(effects_.begin() + index);
    bumpContentRevision();
}

void SubtitleClip::moveEffect(int from, int to)
{
    if (from == to || from < 0 || from >= int(effects_.size()))
        return;
    to = qBound(0, to, int(effects_.size()) - 1);
    auto inst = std::move(effects_[size_t(from)]);
    effects_.erase(effects_.begin() + from);
    effects_.insert(effects_.begin() + to, std::move(inst));
    bumpContentRevision();
}

void SubtitleClip::setEffectEnabled(int index, bool enabled)
{
    if (index < 0 || index >= int(effects_.size()))
        return;
    effects_[size_t(index)].enabled = enabled;
}

bool SubtitleClip::hasMissingEffects() const
{
    for (const auto& e : effects_)
        if (e.missing)
            return true;
    return false;
}

bool SubtitleClip::hasBlockLevelEffect() const
{
    for (const auto& e : effects_)
        if (e.effect && e.effect->isBlockLevel())
            return true;
    return false;
}

// ===========================================================================
//  レンダリング
// ===========================================================================

LayerItem SubtitleClip::makeLayerItem(int64_t frame, int zIndex, const Track& track) const
{
    LayerItem item;
    item.clipId     = id();
    item.trackType  = track.type();
    item.zIndex     = zIndex;
    item.blendMode  = blendMode();
    item.opacity    = float(effectiveOpacity(frame));
    item.transform  = transform();
    item.cropRect   = QRectF(0.0, 0.0, 1.0, 1.0);   ///< 字幕は crop を使わない

    SubtitleRenderRef ref;
    ref.clipId           = id();
    ref.frameInClip      = frame - range().start;
    ref.contentRevision  = contentRevision_;
    item.source = std::move(ref);
    return item;
}

} // namespace yave::subtitle
