#include "LogModel.h"

#include <QCoreApplication>
#include <QDateTime>
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
    if (QThread::currentThread() != thread()) {
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

        // 端末にも出す (従来の挙動を維持)
        fprintf(stderr, "%s\n", qPrintable(msg));
    });
}

} // namespace yave::app
