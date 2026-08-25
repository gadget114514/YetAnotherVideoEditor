#pragma once

#include "SubtitleStyle.h"

#include <QHash>
#include <QString>

namespace yave::subtitle {

/// プロジェクト共通の字幕スタイルプリセット 1 個分。
struct SubtitleStylePreset
{
    QString       id = QStringLiteral("default");
    QString       displayName;
    SubtitleStyle style;
};

/// プリセットテーブル。Project 相当のオブジェクトが 1 個保持する。
/// 存在しない id への参照は default プリセットへフォールバックする。
class SubtitleStylePresetTable
{
public:
    SubtitleStylePresetTable() { ensureDefault(); }

    void set(const SubtitleStylePreset& p) { presets_.insert(p.id, p); }
    void remove(const QString& id)
    {
        if (id == QLatin1String("default"))
            return;
        presets_.remove(id);
        ensureDefault();
    }

    /// 必ず非 null を返す (見つからなければ default)。
    const SubtitleStylePreset& preset(const QString& id) const
    {
        auto it = presets_.constFind(id);
        if (it != presets_.cend())
            return it.value();
        return presets_.constFind(QStringLiteral("default")).value();
    }

    bool contains(const QString& id) const { return presets_.contains(id); }
    QList<QString> ids() const { return presets_.keys(); }
    int count() const { return presets_.size(); }

private:
    void ensureDefault()
    {
        if (!presets_.contains(QStringLiteral("default"))) {
            SubtitleStylePreset p;
            p.id          = QStringLiteral("default");
            p.displayName = QStringLiteral("Default");
            presets_.insert(p.id, p);
        }
    }

    QHash<QString, SubtitleStylePreset> presets_;
};

} // namespace yave::subtitle
