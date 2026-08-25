#pragma once

#include <QSize>
#include <QString>
#include <QUuid>

namespace yave::plugin {

enum class PluginKind { Vst3, SubtitleEffect, AviUtlFilter, AviUtlInput };

/// 走査結果のプラグイン 1 個分の情報。
struct PluginDescriptor
{
    QUuid   uid;                 ///< ホスト内部の一意 id (永続化用)
    PluginKind kind = PluginKind::Vst3;

    QString filePath;            ///< プラグインファイルの絶対パス
    QString nativeId;            ///< VST3 class id / エフェクト id / AviUtl フィルタ名
    QString name;
    QString vendor;
    QString version;

    QSize   preferredEditorSize; ///< VST3 のみ
    bool    hasEditor = false;
    bool    isInstrument = false; ///< VST3 のみ

    bool isValid() const { return !uid.isNull(); }
};

} // namespace yave::plugin
