#include "LibraryStore.h"

#include "../../core/AssetLibrary.h"
#include "../../core/Project.h"
#include "../../core/Transition.h"
#include "../../core/VideoFilter.h"
#include "../../subtitle/TitleClip.h"
#include "../../plugin/SubtitleEffectRegistry.h"
#include "../../subtitle/effects/BuiltinEffects.h"

#include <yave/sdk/ISubtitleEffect.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>

#include <algorithm>

namespace yave::app {

namespace {

constexpr auto kSettingsRoot = "ui/library";

QString categoryKeyOf(LibraryCategory c)
{
    switch (c) {
    case LibraryCategory::Media:      return QStringLiteral("media");
    case LibraryCategory::Transition: return QStringLiteral("transition");
    case LibraryCategory::Title:      return QStringLiteral("title");
    case LibraryCategory::Subtitle:   return QStringLiteral("subtitle");
    case LibraryCategory::Filter:     return QStringLiteral("filter");
    case LibraryCategory::Effect:     return QStringLiteral("effect");
    }
    return QStringLiteral("media");
}

/// 同じ親の下で名前が衝突したら "名前 (2)" にする (1.7.5)。
QString uniqueName(const std::vector<LibraryFolder>& siblings, const QString& wanted)
{
    const auto taken = [&siblings](const QString& n) {
        return std::any_of(siblings.begin(), siblings.end(),
                           [&n](const LibraryFolder& f) { return f.name == n; });
    };
    if (!taken(wanted))
        return wanted;
    for (int i = 2; i < 1000; ++i) {
        const QString candidate = QStringLiteral("%1 (%2)").arg(wanted).arg(i);
        if (!taken(candidate))
            return candidate;
    }
    return wanted;
}

QString assetKindKey(Asset::Kind kind)
{
    switch (kind) {
    case Asset::Kind::Video:     return QStringLiteral("video");
    case Asset::Kind::Audio:     return QStringLiteral("audio");
    case Asset::Kind::Image:     return QStringLiteral("image");
    case Asset::Kind::Generated: return QStringLiteral("generated");
    }
    return QStringLiteral("video");
}

/// 翻訳キー ("effect.fade.name") をそのまま表示すると読めないため、
/// 翻訳が無い場合は末尾の識別子から人間が読める名前を作る。
QString humanizeKey(const QString& key)
{
    const QString translated = QCoreApplication::translate("Library", key.toUtf8().constData());
    if (translated != key)
        return translated;

    const QStringList parts = key.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    QString base = parts.size() >= 2 ? parts.at(parts.size() - 2) : key;
    if (base.isEmpty())
        return key;
    base[0] = base[0].toUpper();
    return base;
}

} // anonymous namespace

QString libraryCategoryKey(LibraryCategory c) { return categoryKeyOf(c); }

bool libraryCategoryFromKey(const QString& key, LibraryCategory* out)
{
    for (int i = 0; i < kLibraryCategoryCount; ++i) {
        const auto c = LibraryCategory(i);
        if (categoryKeyOf(c) == key) {
            if (out)
                *out = c;
            return true;
        }
    }
    return false;
}

QString libraryCategoryDisplayName(LibraryCategory c)
{
    switch (c) {
    case LibraryCategory::Media:
        return QCoreApplication::translate("Library", "Media");
    case LibraryCategory::Transition:
        return QCoreApplication::translate("Library", "Transitions");
    case LibraryCategory::Title:
        return QCoreApplication::translate("Library", "Titles");
    case LibraryCategory::Subtitle:
        return QCoreApplication::translate("Library", "Subtitles");
    case LibraryCategory::Filter:
        return QCoreApplication::translate("Library", "Filters");
    case LibraryCategory::Effect:
        return QCoreApplication::translate("Library", "Effects");
    }
    return {};
}

// ===========================================================================
//  生成 / カタログ構築
// ===========================================================================

LibraryStore& LibraryStore::instance()
{
    static LibraryStore s;
    return s;
}

LibraryStore::LibraryStore()
{
    rebuildCatalog();
    loadSettings();
}

void LibraryStore::rebuildCatalog()
{
    catalogItems_.clear();

    const auto add = [this](LibraryCategory cat, const QString& id, const QString& name,
                            const QString& kind) {
        LibraryItem item;
        item.itemId   = id;
        item.category = cat;
        item.name     = name;
        item.kind     = kind;
        item.builtin  = true;
        catalogItems_.push_back(std::move(item));
    };

    // ---- トランジション (3.10.2) ----
    for (const TransitionDesc& d : builtinTransitions())
        add(LibraryCategory::Transition, d.transitionId, humanizeKey(d.displayNameKey),
            QStringLiteral("transition"));

    // ---- タイトル (3.11) ----
    for (const subtitle::TitlePresetDesc& d : subtitle::builtinTitlePresets())
        add(LibraryCategory::Title, d.presetId, humanizeKey(d.displayNameKey),
            QStringLiteral("title"));

    // ---- 字幕 ----
    // 空の字幕クリップ + スタイルプリセット。プリセットはプロジェクト側の
    // テーブルが本体だが、ここでは既定の 1 個だけを出す。
    add(LibraryCategory::Subtitle, QStringLiteral("yave.subtitle.empty"),
        QCoreApplication::translate("Library", "Empty Subtitle"), QStringLiteral("subtitle"));
    add(LibraryCategory::Subtitle, QStringLiteral("yave.subtitle.preset.default"),
        QCoreApplication::translate("Library", "Default Style"), QStringLiteral("subtitle"));

    // ---- フィルタ (3.9.2) ----
    for (const VideoFilterDesc& d : builtinVideoFilters())
        add(LibraryCategory::Filter, d.filterId, humanizeKey(d.displayNameKey),
            QStringLiteral("filter"));

    // ---- エフェクト (組み込み字幕エフェクト) ----
    for (const auto& proto : subtitle::BuiltinEffectRegistry::instance().prototypes()) {
        if (!proto)
            continue;
        add(LibraryCategory::Effect, proto->id(), humanizeKey(proto->displayName()),
            QStringLiteral("effect"));
    }

    // 外部プラグイン由来のエフェクト / AviUtl フィルタは、走査完了後に
    // PluginManager から足す (8章)。現状は列挙 API が無いため未接続。

    for (auto& item : catalogItems_) {
        item.folderId     = catalogAssignments_.value(item.itemId);
        item.iconOverride = iconOverrides_.value(item.itemId);
    }
}

// ===========================================================================
//  永続化
// ===========================================================================

void LibraryStore::loadSettings()
{
    QSettings settings;

    catalogFolders_.clear();
    catalogAssignments_.clear();

    const int n = settings.beginReadArray(QStringLiteral("%1/folders").arg(kSettingsRoot));
    for (int i = 0; i < n; ++i) {
        settings.setArrayIndex(i);
        LibraryFolder f;
        f.id       = QUuid(settings.value(QStringLiteral("id")).toString());
        f.parentId = QUuid(settings.value(QStringLiteral("parentId")).toString());
        f.name     = settings.value(QStringLiteral("name")).toString();
        LibraryCategory cat = LibraryCategory::Media;
        libraryCategoryFromKey(settings.value(QStringLiteral("category")).toString(), &cat);
        f.category = cat;
        if (!f.id.isNull() && f.category != LibraryCategory::Media)
            catalogFolders_.push_back(std::move(f));
    }
    settings.endArray();

    settings.beginGroup(QStringLiteral("%1/assignments").arg(kSettingsRoot));
    for (const QString& key : settings.childKeys())
        catalogAssignments_.insert(key, QUuid(settings.value(key).toString()));
    settings.endGroup();

    settings.beginGroup(QStringLiteral("%1/icons").arg(kSettingsRoot));
    for (const QString& key : settings.childKeys())
        iconOverrides_.insert(key, settings.value(key).toString());
    settings.endGroup();

    for (auto& item : catalogItems_) {
        item.folderId     = catalogAssignments_.value(item.itemId);
        item.iconOverride = iconOverrides_.value(item.itemId);
    }
}

void LibraryStore::saveSettings() const
{
    QSettings settings;

    settings.beginWriteArray(QStringLiteral("%1/folders").arg(kSettingsRoot),
                             int(catalogFolders_.size()));
    for (int i = 0; i < int(catalogFolders_.size()); ++i) {
        settings.setArrayIndex(i);
        const LibraryFolder& f = catalogFolders_[size_t(i)];
        settings.setValue(QStringLiteral("id"), f.id.toString(QUuid::WithoutBraces));
        settings.setValue(QStringLiteral("parentId"),
                          f.parentId.isNull() ? QString()
                                              : f.parentId.toString(QUuid::WithoutBraces));
        settings.setValue(QStringLiteral("name"), f.name);
        settings.setValue(QStringLiteral("category"), categoryKeyOf(f.category));
    }
    settings.endArray();

    settings.remove(QStringLiteral("%1/assignments").arg(kSettingsRoot));
    settings.beginGroup(QStringLiteral("%1/assignments").arg(kSettingsRoot));
    for (auto it = catalogAssignments_.cbegin(); it != catalogAssignments_.cend(); ++it) {
        if (!it.value().isNull())
            settings.setValue(it.key(), it.value().toString(QUuid::WithoutBraces));
    }
    settings.endGroup();

    settings.remove(QStringLiteral("%1/icons").arg(kSettingsRoot));
    settings.beginGroup(QStringLiteral("%1/icons").arg(kSettingsRoot));
    for (auto it = iconOverrides_.cbegin(); it != iconOverrides_.cend(); ++it) {
        if (!it.value().isEmpty())
            settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
}

void LibraryStore::setProject(Project* project)
{
    project_ = project;
    emit foldersChanged(int(LibraryCategory::Media));
    emit itemsChanged(int(LibraryCategory::Media));
}

// ===========================================================================
//  参照
// ===========================================================================

std::vector<LibraryFolder> LibraryStore::folders(LibraryCategory cat, const QUuid& parentId) const
{
    std::vector<LibraryFolder> out;

    if (cat == LibraryCategory::Media) {
        if (!project_)
            return out;
        for (const MediaFolder& f : project_->mediaFolders().folders) {
            if (f.parentId == parentId)
                out.push_back(LibraryFolder{ f.id, f.parentId, f.name, LibraryCategory::Media });
        }
    } else {
        for (const LibraryFolder& f : catalogFolders_) {
            if (f.category == cat && f.parentId == parentId)
                out.push_back(f);
        }
    }

    std::sort(out.begin(), out.end(), [](const LibraryFolder& a, const LibraryFolder& b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return out;
}

bool LibraryStore::hasSubfolders(LibraryCategory cat, const QUuid& folderId) const
{
    return !folders(cat, folderId).empty();
}

const LibraryFolder* LibraryStore::folder(const QUuid& folderId) const
{
    for (const LibraryFolder& f : catalogFolders_) {
        if (f.id == folderId)
            return &f;
    }
    return nullptr;
}

std::vector<LibraryItem> LibraryStore::items(LibraryCategory cat, const QUuid& folderId) const
{
    std::vector<LibraryItem> out;

    if (cat == LibraryCategory::Media) {
        const AssetLibrary* lib = project_ ? project_->assets() : nullptr;
        if (!lib)
            return out;
        const MediaFolderTree& tree = project_->mediaFolders();

        for (const QUuid& id : lib->allIds()) {
            const Asset* a = lib->asset(id);
            if (!a)
                continue;
            if (tree.assignments.value(id) != folderId)
                continue;

            LibraryItem item;
            item.itemId         = id.toString(QUuid::WithoutBraces);
            item.category       = LibraryCategory::Media;
            item.assetId        = id;
            item.folderId       = folderId;
            item.name           = QFileInfo(a->relativePath).fileName();
            item.kind           = assetKindKey(a->kind);
            item.durationFrames = a->durationFrames;
            item.missing        = a->isMissing;
            item.iconOverride   = iconOverrides_.value(item.itemId);
            out.push_back(std::move(item));
        }
    } else {
        for (const LibraryItem& item : catalogItems_) {
            if (item.category == cat && item.folderId == folderId)
                out.push_back(item);
        }
    }

    std::sort(out.begin(), out.end(), [](const LibraryItem& a, const LibraryItem& b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return out;
}

// ===========================================================================
//  フォルダ操作 (1.7.5)
// ===========================================================================

QUuid LibraryStore::createFolder(LibraryCategory cat, const QUuid& parentId, const QString& name)
{
    const QString unique = uniqueName(folders(cat, parentId), name);

    if (cat == LibraryCategory::Media) {
        if (!project_)
            return {};
        MediaFolder f;
        f.id       = QUuid::createUuid();
        f.parentId = parentId;
        f.name     = unique;
        project_->mutableMediaFolders().folders.push_back(f);
        project_->markModified();
        emit foldersChanged(int(cat));
        return f.id;
    }

    LibraryFolder f;
    f.id       = QUuid::createUuid();
    f.parentId = parentId;
    f.name     = unique;
    f.category = cat;
    catalogFolders_.push_back(f);
    saveSettings();
    emit foldersChanged(int(cat));
    return f.id;
}

bool LibraryStore::renameFolder(const QUuid& folderId, const QString& newName)
{
    if (folderId.isNull() || newName.trimmed().isEmpty())
        return false;

    if (project_) {
        auto& folders = project_->mutableMediaFolders().folders;
        for (auto& f : folders) {
            if (f.id != folderId)
                continue;
            f.name = newName.trimmed();
            project_->markModified();
            emit foldersChanged(int(LibraryCategory::Media));
            return true;
        }
    }

    for (auto& f : catalogFolders_) {
        if (f.id != folderId)
            continue;
        f.name = newName.trimmed();
        saveSettings();
        emit foldersChanged(int(f.category));
        return true;
    }
    return false;
}

bool LibraryStore::removeFolder(const QUuid& folderId)
{
    if (folderId.isNull())
        return false;

    // --- メディア (プロジェクト側) ---
    if (project_) {
        MediaFolderTree& tree = project_->mutableMediaFolders();
        const auto it = std::find_if(tree.folders.begin(), tree.folders.end(),
                                     [&](const MediaFolder& f) { return f.id == folderId; });
        if (it != tree.folders.end()) {
            const QUuid parentId = it->parentId;
            tree.folders.erase(it);
            // 中身は消さず親へ繰り上げる (1.7.5)
            for (auto& f : tree.folders) {
                if (f.parentId == folderId)
                    f.parentId = parentId;
            }
            for (auto a = tree.assignments.begin(); a != tree.assignments.end(); ++a) {
                if (a.value() == folderId)
                    *a = parentId;
            }
            project_->markModified();
            emit foldersChanged(int(LibraryCategory::Media));
            emit itemsChanged(int(LibraryCategory::Media));
            return true;
        }
    }

    // --- カタログ側 ---
    const auto it = std::find_if(catalogFolders_.begin(), catalogFolders_.end(),
                                 [&](const LibraryFolder& f) { return f.id == folderId; });
    if (it == catalogFolders_.end())
        return false;

    const LibraryCategory cat = it->category;
    const QUuid parentId      = it->parentId;
    catalogFolders_.erase(it);

    for (auto& f : catalogFolders_) {
        if (f.parentId == folderId)
            f.parentId = parentId;
    }
    for (auto a = catalogAssignments_.begin(); a != catalogAssignments_.end(); ++a) {
        if (a.value() == folderId)
            *a = parentId;
    }
    for (auto& item : catalogItems_) {
        if (item.folderId == folderId)
            item.folderId = parentId;
    }
    saveSettings();
    emit foldersChanged(int(cat));
    emit itemsChanged(int(cat));
    return true;
}

bool LibraryStore::moveItems(const QStringList& itemIds, LibraryCategory cat,
                             const QUuid& toFolderId)
{
    if (itemIds.isEmpty())
        return false;

    if (cat == LibraryCategory::Media) {
        if (!project_)
            return false;
        MediaFolderTree& tree = project_->mutableMediaFolders();
        for (const QString& id : itemIds) {
            const QUuid assetId(id);
            if (assetId.isNull())
                continue;
            if (toFolderId.isNull())
                tree.assignments.remove(assetId);
            else
                tree.assignments.insert(assetId, toFolderId);
        }
        project_->markModified();
        emit itemsChanged(int(cat));
        return true;
    }

    for (const QString& id : itemIds) {
        catalogAssignments_.insert(id, toFolderId);
        for (auto& item : catalogItems_) {
            if (item.itemId == id && item.category == cat)
                item.folderId = toFolderId;
        }
    }
    saveSettings();
    emit itemsChanged(int(cat));
    return true;
}

bool LibraryStore::moveFolder(const QUuid& folderId, const QUuid& toParentId)
{
    if (folderId.isNull() || folderId == toParentId)
        return false;

    // 自分の子孫へは移動できない (木が壊れる)
    const auto isDescendant = [this](const QUuid& candidate, const QUuid& ancestor) {
        QUuid cur = candidate;
        for (int guard = 0; guard < 64 && !cur.isNull(); ++guard) {
            if (cur == ancestor)
                return true;
            const LibraryFolder* f = folder(cur);
            if (!f)
                break;
            cur = f->parentId;
        }
        return false;
    };

    for (auto& f : catalogFolders_) {
        if (f.id != folderId)
            continue;
        if (isDescendant(toParentId, folderId))
            return false;
        f.parentId = toParentId;
        saveSettings();
        emit foldersChanged(int(f.category));
        return true;
    }

    if (project_) {
        MediaFolderTree& tree = project_->mutableMediaFolders();
        for (auto& f : tree.folders) {
            if (f.id != folderId)
                continue;
            // メディア側も同様に子孫チェックを行う
            QUuid cur = toParentId;
            for (int guard = 0; guard < 64 && !cur.isNull(); ++guard) {
                if (cur == folderId)
                    return false;
                const MediaFolder* p = tree.find(cur);
                cur = p ? p->parentId : QUuid();
            }
            f.parentId = toParentId;
            project_->markModified();
            emit foldersChanged(int(LibraryCategory::Media));
            return true;
        }
    }
    return false;
}

// ===========================================================================
//  アイコン
// ===========================================================================

void LibraryStore::setItemIcon(const QString& itemId, const QString& absolutePath)
{
    if (itemId.isEmpty())
        return;
    iconOverrides_.insert(itemId, absolutePath);
    for (auto& item : catalogItems_) {
        if (item.itemId == itemId)
            item.iconOverride = absolutePath;
    }
    saveSettings();
    emit iconChanged(itemId);
}

void LibraryStore::clearItemIcon(const QString& itemId)
{
    if (!iconOverrides_.remove(itemId))
        return;
    for (auto& item : catalogItems_) {
        if (item.itemId == itemId)
            item.iconOverride.clear();
    }
    saveSettings();
    emit iconChanged(itemId);
}

QString LibraryStore::itemIconOverride(const QString& itemId) const
{
    return iconOverrides_.value(itemId);
}

void LibraryStore::assignAssetToFolder(const QUuid& assetId, const QUuid& folderId)
{
    if (!project_ || assetId.isNull())
        return;
    MediaFolderTree& tree = project_->mutableMediaFolders();
    if (folderId.isNull())
        tree.assignments.remove(assetId);
    else
        tree.assignments.insert(assetId, folderId);
    emit itemsChanged(int(LibraryCategory::Media));
}

} // namespace yave::app
