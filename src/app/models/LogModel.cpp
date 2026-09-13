#include "LogModel.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QClipboard>
#include <QMetaObject>
#include <QThread>

namespace yave::app {

LogModel::LogModel() = default;

LogModel& LogModel::instance()
{
    static LogModel model;
    return model;
}

int LogModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return lines_.size();
}

QVariant LogModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= lines_.size())
        return {};
    if (role == TextRole)
        return lines_.at(index.row());
    return {};
}

QHash<int, QByteArray> LogModel::roleNames() const
{
    return { { TextRole, "text" } };
}

void LogModel::append(const QString& line)
{
    // QApplication 構築前に instance() が作られた場合、QObject のスレッド所属が
    // null になる。キュー投入が永遠に実行されないのを防ぐため、可能なら
    // メインスレッドへ所属させてから通常のスレッド判定へ乗せる。
    QThread* owner = thread();
    if (!owner) {
        if (auto* app = QCoreApplication::instance()) {
            moveToThread(app->thread());
            owner = app->thread();
        }
    }

    if (owner && QThread::currentThread() != owner) {
        QMetaObject::invokeMethod(this, [this, line] { append(line); }, Qt::QueuedConnection);
        return;
    }

    const QString stamped = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"))
                            + QStringLiteral("  ") + line;

    beginInsertRows({}, lines_.size(), lines_.size());
    lines_.append(stamped);
    endInsertRows();

    if (lines_.size() > kMaxLines) {
        const int overflow = lines_.size() - kMaxLines;
        beginRemoveRows({}, 0, overflow - 1);
        lines_.erase(lines_.begin(), lines_.begin() + overflow);
        endRemoveRows();
    }
}

void LogModel::clear()
{
    if (lines_.isEmpty())
        return;
    beginResetModel();
    lines_.clear();
    endResetModel();
}

QString LogModel::allText() const
{
    return lines_.join(QLatin1Char('\n'));
}

QString LogModel::lineAt(int index) const
{
    if (index < 0 || index >= lines_.size())
        return {};
    // 行頭の "HH:mm:ss.zzz  " (13 文字 + 2 空白) を除いた本文を返す
    const QString& line = lines_.at(index);
    const int space = line.indexOf(QStringLiteral("  "));
    if (space > 0)
        return line.mid(space + 2);
    return line;
}

void LogModel::copyAllToClipboard() const
{
    if (auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
        app->clipboard()->setText(allText());
}

void LogModel::copyLineToClipboard(int index) const
{
    const QString text = lineAt(index);
    if (text.isEmpty())
        return;
    if (auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
        app->clipboard()->setText(text);
}

void LogModel::copyToClipboard(const QString& text) const
{
    if (text.isEmpty())
        return;
    if (auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
        app->clipboard()->setText(text);
}

void LogModel::installQtMessageHandler()
{
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext& context, const QString& msg) {
        const char* level = "DEBUG";
        switch (type) {
        case QtDebugMsg:    level = "DEBUG"; break;
        case QtInfoMsg:     level = "INFO";  break;
        case QtWarningMsg:  level = "WARN";  break;
        case QtCriticalMsg: level = "ERROR"; break;
        case QtFatalMsg:    level = "FATAL"; break;
        }
        const QString category = context.category ? QString::fromUtf8(context.category)
                                                    : QStringLiteral("default");
        LogModel::instance().append(QStringLiteral("[%1] %2: %3").arg(level, category, msg));

        // 端末にも出す (従来の挙動を維持)。
        // stderr がファイル/パイプへリダイレクトされていると完全バッファリングに
        // なり、プロセス終了までテール出力が反映されないことがあるため明示 flush する。
        fprintf(stderr, "%s\n", qPrintable(msg));
        fflush(stderr);
    });
}

} // namespace yave::app
