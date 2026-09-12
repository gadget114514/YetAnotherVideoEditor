#pragma once

#include <QAbstractListModel>
#include <QStringList>

namespace yave::app {

/// アプリ内ログウィンドウ用のモデル。qInstallMessageHandler で捕まえた
/// qDebug/qInfo/qWarning/qCritical と、コントローラ側からの手動ログ
/// (log()) をリングバッファに保持し、Inspector と並ぶタブとして表示する。
class LogModel : public QAbstractListModel
{
    Q_OBJECT
public:
    static LogModel& instance();

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// メッセージハンドラや C++ 側からの追記。UI スレッド以外から呼ばれても
    /// 安全なようにメインスレッドへ回す。
    Q_INVOKABLE void append(const QString& line);
    Q_INVOKABLE void clear();

    /// qInstallMessageHandler をこのモデルへ接続する (main() から一度呼ぶ)。
    static void installQtMessageHandler();

private:
    LogModel();

    enum Role { TextRole = Qt::UserRole + 1 };

    static constexpr int kMaxLines = 2000;
    QStringList lines_;
};

} // namespace yave::app
