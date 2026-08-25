#include "Project.h"
#include "AssetLibrary.h"
#include "Timeline.h"

namespace yave {

Project::Project(QObject* parent)
    : QObject(parent)
    // 子オブジェクトは unique_ptr で所有するため QObject の親には登録しない
    // (親子関係と unique_ptr の二重管理は二重解放を引き起こす)。
    , timeline_(std::make_unique<Timeline>(nullptr))
    , assetLibrary_(std::make_unique<AssetLibrary>())
    , undoStack_(new QUndoStack())
{
}

Project::~Project() = default;

Rational Project::timebase() const
{
    return timeline_->timebase();
}

void Project::setTimebase(const Rational& tb)
{
    timeline_->setTimebase(tb);
}

QSize Project::canvasSize() const
{
    return timeline_->canvasSize();
}

void Project::setCanvasSize(const QSize& s)
{
    timeline_->setCanvasSize(s);
}

} // namespace yave
