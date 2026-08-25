#include "Log.h"

Q_LOGGING_CATEGORY(lcCore,     "yave.core")
Q_LOGGING_CATEGORY(lcMedia,    "yave.media")
Q_LOGGING_CATEGORY(lcRender,   "yave.render")
Q_LOGGING_CATEGORY(lcAudio,    "yave.audio")
Q_LOGGING_CATEGORY(lcSubtitle, "yave.subtitle")
Q_LOGGING_CATEGORY(lcAi,       "yave.ai")
Q_LOGGING_CATEGORY(lcPlugin,   "yave.plugin")
Q_LOGGING_CATEGORY(lcIo,       "yave.io")
Q_LOGGING_CATEGORY(lcI18n,     "yave.i18n")
Q_LOGGING_CATEGORY(lcApp,      "yave.app")

namespace yave::diag {

void registerLoggingCategories()
{
    // 既定では自前カテゴリを info 以上で出す。
    // QLoggingCategory の setFilterRules を使うと実行時に上書きできる。
    QLoggingCategory::setFilterRules(QStringLiteral("yave.*.debug=false"));
}

} // namespace yave::diag
