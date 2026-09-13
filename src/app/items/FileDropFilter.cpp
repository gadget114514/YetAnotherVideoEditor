#include "FileDropFilter.h"

#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QMimeData>
#include <QEvent>
#include <QUrl>

namespace yave::app {

FileDropFilter::FileDropFilter(QObject* parent) : QObject(parent) {}

bool FileDropFilter::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched);

    // QML の DropArea では外部ファイルドロップの urls が取得できないことがある
    // (Qt の既知挙動) ため、ウィンドウレベルの QDropEvent から MIME データを
    // 直接読んで QML 側へ供給する。イベント自体は消費せず QML へ通す。
    switch (event->type()) {
    case QEvent::DragEnter: {
        auto* e = static_cast<QDragEnterEvent*>(event);
        // ファイルが含まれる場合は受け入れてドロップカーソルを出す
        if (e->mimeData()->hasUrls()) {
            e->acceptProposedAction();
            return false;   // QML 側にも通す
        }
        return false;
    }
    case QEvent::DragMove: {
        auto* e = static_cast<QDragMoveEvent*>(event);
        if (e->mimeData()->hasUrls()) {
            e->acceptProposedAction();
            return false;
        }
        return false;
    }
    case QEvent::Drop: {
        auto* e = static_cast<QDropEvent*>(event);
        const QMimeData* mime = e->mimeData();
        QStringList urls;
        if (mime && mime->hasUrls()) {
            const auto list = mime->urls();
            urls.reserve(list.size());
            for (const QUrl& u : list)
                urls.append(u.toString());
        }
        lastUrls_ = urls;
        lastPos_  = e->position();
        emit lastUrlsChanged();
        emit lastPosChanged();
        return false;   // QML の fileDropArea.onDropped にも通す
    }
    default:
        break;
    }
    return false;
}

} // namespace yave::app