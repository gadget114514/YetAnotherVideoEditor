#pragma once

#include "Clip.h"

namespace yave {

/// 音声クリップ。
class AudioClip : public Clip
{
public:
    enum class FadeCurve { Linear, EqualPower, Exponential };

    AudioClip() = default;
    explicit AudioClip(const QUuid& assetId);

    ClipType type() const override { return ClipType::Audio; }

    std::shared_ptr<Clip> clone() const override
    {
        auto c = std::shared_ptr<AudioClip>(new AudioClip(*this));
        c->setId(QUuid::createUuid());
        return c;
    }

    // --- ソース ---
    QUuid   assetId() const { return assetId_; }
    void    setAssetId(const QUuid& id) { assetId_ = id; }

    int64_t sourceOffset() const override { return sourceOffset_; }
    void    setSourceOffset(int64_t off) override { sourceOffset_ = off > 0 ? off : 0; }
    void    shiftSourceOffset(int64_t delta) override { setSourceOffset(sourceOffset_ + delta); }

    int64_t maxDurationFrames() const { return maxDurationFrames_; }
    void    setMaxDurationFrames(int64_t f) { maxDurationFrames_ = f; }
    int64_t maxDuration() const override { return maxDurationFrames_; }

    // --- 音量 ---
    double gain() const { return gain_; }
    void   setGain(double g) { gain_ = g < 0.0 ? 0.0 : g; }
    double pan() const { return pan_; }
    void   setPan(double p) { pan_ = qBound(-1.0, p, 1.0); }

    FadeCurve fadeInCurve() const { return fadeInCurve_; }
    void      setFadeInCurve(FadeCurve c) { fadeInCurve_ = c; }
    FadeCurve fadeOutCurve() const { return fadeOutCurve_; }
    void      setFadeOutCurve(FadeCurve c) { fadeOutCurve_ = c; }

    LayerItem makeLayerItem(int64_t frame, int zIndex, const Track& track) const override;

private:
    QUuid     assetId_;
    int64_t   sourceOffset_      = 0;
    int64_t   maxDurationFrames_ = -1;
    double    gain_              = 1.0;
    double    pan_               = 0.0;
    FadeCurve fadeInCurve_       = FadeCurve::EqualPower;
    FadeCurve fadeOutCurve_      = FadeCurve::Linear;
};

} // namespace yave
