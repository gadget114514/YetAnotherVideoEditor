#pragma once

#include <QObject>
#include <QPointF>
#include <QStringList>

namespace yave::app {

/// QML の DropArea では外部ファイルドロップの urls が取得できない場合があるため、
/// ウィンドウレベルの QEvent::Drop を捕捉してファイル URL を供給する。
///
/// QQuickWindow へ installEventFilter で取り付ける。QML 側の fileDropArea.onDropped
/// は drop.urls が空なら lastUrls を使う。lastPos はウィンドウ座標。
class FileDropFilter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList lastUrls READ lastUrls NOTIFY lastUrlsChanged)
    Q_PROPERTY(QPointF lastPos READ lastPos NOTIFY lastPosChanged)
public:
    explicit FileDropFilter(QObject* parent = nullptr);

    bool eventFilter(QObject* watched, QEvent* event) override;

    QStringList lastUrls() const { return lastUrls_; }
    QPointF     lastPos() const { return lastPos_; }

signals:
    void lastUrlsChanged();
    void lastPosChanged();

private:
    QStringList lastUrls_;
    QPointF     lastPos_;
};

} // namespace yave::app