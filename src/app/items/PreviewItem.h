#pragma once

#include "../core/Rational.h"

#include <QQuickRhiItem>

#include <memory>

namespace yave {

class Timeline;
class Project;

namespace render {
class RhiCompositor;
}

/// QQuickRhiItem 派生のプレビュー表示 (3.7 参照)。
///
/// synchronize() は UI スレッドと Render スレッドがブロックし合う唯一の
/// タイミングであり、ここで RenderSnapshot をコピーする。それ以外では
/// Timeline に触れない。
class PreviewItem : public QQuickRhiItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qint64 frameIndex READ frameIndex WRITE setFrameIndex NOTIFY frameIndexChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged)

public:
    PreviewItem();

    qint64 frameIndex() const { return frameIndex_; }
    void   setFrameIndex(qint64 f);

    double zoom() const;
    void   setZoom(double z);

    void     attachTimeline(Timeline* timeline);
    Timeline* timeline() const { return timeline_; }

    QQuickRhiItemRenderer* createRenderer() override;

signals:
    void frameIndexChanged();
    void zoomChanged();

private:
    qint64  frameIndex_ = 0;
    double  zoom_ = 1.0;
    Timeline* timeline_ = nullptr;
};

} // namespace yave
