#pragma once

#include "Clip.h"

#include <QColor>

namespace yave {

/// 単色クリップ。背景 / マット用。
class ColorClip : public Clip
{
public:
    ClipType type() const override { return ClipType::Color; }

    std::shared_ptr<Clip> clone() const override
    {
        auto c = std::shared_ptr<ColorClip>(new ColorClip(*this));
        c->setId(QUuid::createUuid());
        return c;
    }

    QColor color() const { return color_; }
    void   setColor(const QColor& c) { color_ = c; }

    LayerItem makeLayerItem(int64_t frame, int zIndex, const Track& track) const override;

private:
    QColor color_{Qt::black};
};

} // namespace yave
