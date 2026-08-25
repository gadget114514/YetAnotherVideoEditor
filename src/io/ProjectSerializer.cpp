#include "ProjectSerializer.h"
#include "EnumMapping.h"
#include "JsonKeys.h"
#include "PathResolver.h"
#include "SchemaMigration.h"

#include "../ai/AiGenerationParams.h"
#include "../ai/AiGenerationTask.h"
#include "../core/AiPlaceholderClip.h"
#include "../core/AssetLibrary.h"
#include "../core/AudioClip.h"
#include "../core/Clip.h"
#include "../core/ColorClip.h"
#include "../core/Project.h"
#include "../core/Timeline.h"
#include "../core/Track.h"
#include "../core/VideoClip.h"
#include "../core/Transition.h"
#include "../core/VideoFilter.h"
#include "../subtitle/SubtitleClip.h"
#include "../subtitle/TitleClip.h"
#include "../subtitle/SubtitleEffectFactoryBridge.h"
#include "../subtitle/SubtitleEffectInstance.h"
#include "../subtitle/SubtitleStylePreset.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
#include <iterator>

namespace yave::io {

using namespace keys;
using ai::AiGenerationParams;

// ===========================================================================
//  ヘルパ
// ===========================================================================

namespace {

QJsonObject rationalToJson(const Rational& r)
{
    QJsonObject o;
    o[QStringLiteral("num")] = double(r.num);
    o[QStringLiteral("den")] = double(r.den);
    return o;
}

Rational rationalFromJson(const QJsonObject& o, const Rational& fallback)
{
    if (!o.contains(QStringLiteral("num")))
        return fallback;
    return Rational(int64_t(o[QStringLiteral("num")].toDouble()),
                    int64_t(o[QStringLiteral("den")].toDouble(1)));
}

QJsonObject timeRangeToJson(const TimeRange& r)
{
    QJsonObject o;
    o[kRangeStart]    = double(r.start);
    o[kRangeDuration] = double(r.duration);
    return o;
}

TimeRange timeRangeFromJson(const QJsonValue& v, const TimeRange& fallback = {})
{
    if (!v.isObject())
        return fallback;
    const QJsonObject o = v.toObject();
    TimeRange r;
    r.start    = int64_t(o[kRangeStart].toDouble(0));
    r.duration = int64_t(o[kRangeDuration].toDouble(0));
    return r;
}

QString uuidToJson(const QUuid& u)
{
    return u.isNull() ? QString() : u.toString(QUuid::WithoutBraces);
}

QUuid uuidFromJson(const QJsonValue& v)
{
    const QString s = v.toString();
    return s.isEmpty() ? QUuid() : QUuid(s);
}

QJsonObject rectToJson(const QRectF& r)
{
    QJsonObject o;
    o[QStringLiteral("x")]      = r.x();
    o[QStringLiteral("y")]      = r.y();
    o[QStringLiteral("width")]  = r.width();
    o[QStringLiteral("height")] = r.height();
    return o;
}

QRectF rectFromJson(const QJsonValue& v, const QRectF& fallback)
{
    if (!v.isObject())
        return fallback;
    const QJsonObject o = v.toObject();
    return QRectF(o[QStringLiteral("x")].toDouble(),
                  o[QStringLiteral("y")].toDouble(),
                  o[QStringLiteral("width")].toDouble(fallback.width()),
                  o[QStringLiteral("height")].toDouble(fallback.height()));
}

void mergeInto(QJsonObject& dst, const QJsonObject& src)
{
    for (auto it = src.begin(); it != src.end(); ++it)
        dst[it.key()] = it.value();
}

/// 既知キーを除いたフィールドを取り出す (前方互換のための保持)
QJsonObject extractUnknownFields(const QJsonObject& o, const QStringList& knownKeys)
{
    QJsonObject unknown;
    for (auto it = o.begin(); it != o.end(); ++it)
        if (!knownKeys.contains(it.key()))
            unknown[it.key()] = it.value();
    return unknown;
}

/// クリップ共通の filters 配列を読む (9.4.1)。
/// v2 以前の effectChain は migrate_2_to_3 が filters へ移すため、ここでは見ない。
std::vector<VideoFilterInstance> deserializeFilters(const QJsonObject& o)
{
    std::vector<VideoFilterInstance> out;
    const QJsonArray arr = o[kFilters].toArray();
    out.reserve(size_t(arr.size()));
    for (const QJsonValue& v : arr) {
        VideoFilterInstance inst = VideoFilterInstance::fromJson(v.toObject());
        if (!inst.filterId.isEmpty())
            out.push_back(std::move(inst));
    }
    return out;
}

QStringList baseClipKeys()
{
    static const QStringList keys = {
        kClipId, kClipType, QStringLiteral("name"), kEnabled,
        QStringLiteral("locked"), QStringLiteral("opacity"),
        QStringLiteral("blendMode"), QStringLiteral("fadeIn"), QStringLiteral("fadeOut"),
        QStringLiteral("generatedByTaskId"), kRange, kFilters
    };
    return keys;
}

QStringList videoClipKeys()
{
    static const QStringList keys = baseClipKeys()
        << kAssetId << kSourceOffset << QStringLiteral("speed")
        << QStringLiteral("reversed") << QStringLiteral("transform")
        << QStringLiteral("crop");
    return keys;
}

} // anonymous namespace

namespace keys {

QStringList knownClipKeys()
{
    static const QStringList keys = videoClipKeys();
    return keys;
}

QStringList knownSubtitleClipKeys()
{
    static const QStringList keys = baseClipKeys()
        << kStylePresetId << kText << kStyleOverride << kEffectStack << kWordTimings
        << kPresetId;
    return keys;
}

} // namespace keys

// ===========================================================================
//  トップレベル: save / load
// ===========================================================================

bool ProjectSerializer::save(const Project& project, const QString& path,
                             const SaveOptions& opts, QString* errorOut)
{
    const QJsonObject root = serializeProject(project, opts);
    const QByteArray data  = QJsonDocument(root).toJson(
        opts.indented ? QJsonDocument::Indented : QJsonDocument::Compact);

    // 原子的書き込み。QSaveFile は「一時ファイルへ書く -> fsync -> rename」を行う。
    // rename は同一ボリューム内では原子的なので、途中でクラッシュしても
    // 既存ファイルは無傷のまま残る。
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = file.errorString();
        return false;
    }
    if (file.write(data) != data.size()) {
        if (errorOut)
            *errorOut = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (errorOut)
            *errorOut = file.errorString();
        return false;
    }

    rotateBackups(path, 5);
    return true;
}

LoadResult ProjectSerializer::load(Project* project, const QString& path)
{
    LoadResult result;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.errorMessage = QObject::tr("Cannot open project file: %1").arg(path);
        return result;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        result.errorMessage =
            QObject::tr("Invalid JSON at offset %1: %2")
                .arg(parseError.offset).arg(parseError.errorString());
        return result;
    }

    QJsonObject root = doc.object();

    // ---- スキーマバージョン ----
    const int version = root[kSchemaVersion].toInt(kCurrentSchemaVersion);
    result.loadedSchemaVersion = version;

    if (version > kCurrentSchemaVersion) {
        // 開くこと自体は許可するが、明確に警告する
        result.warnings.append(QObject::tr(
            "This project was saved with a newer version of YAVE (schema %1). "
            "Some data may be lost if you save it with this version.").arg(version));
    } else if (version < kCurrentSchemaVersion) {
        applyMigrations(root, version, &result);
    }

    if (!deserializeProject(project, root, &result))
        return result;

    // ---- アセットのパス解決 ----
    deserializeAssets(project, root[kAssets].toArray(), &result, path);

    result.ok = true;
    return result;
}

bool ProjectSerializer::saveAutosave(const Project& project, const QString& path)
{
    SaveOptions opts;
    opts.indented          = false;   ///< CBOR 相当の速度優先設定
    opts.omitDefaultValues = true;
    return save(project, path, opts, nullptr);
}

LoadResult ProjectSerializer::loadAutosave(Project* project, const QString& path)
{
    return load(project, path);
}

// ===========================================================================
//  プロジェクト全体
// ===========================================================================

QJsonObject ProjectSerializer::serializeProject(const Project& p, const SaveOptions& o)
{
    QJsonObject root;

    root[kSchemaVersion] = kCurrentSchemaVersion;

    QJsonObject app;
    app[QStringLiteral("name")]    = QStringLiteral("YAVE");
    app[QStringLiteral("version")] = QCoreApplication::applicationVersion().isEmpty()
                                         ? QStringLiteral("1.0.0")
                                         : QCoreApplication::applicationVersion();
    root[QStringLiteral("application")] = app;
    root[QStringLiteral("savedAt")]     =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    // ---- プロジェクト設定 ----
    QJsonObject proj;
    proj[QStringLiteral("name")]       = p.name();
    proj[QStringLiteral("timebase")]   = rationalToJson(p.timebase());
    QJsonObject canvas;
    canvas[QStringLiteral("width")]    = p.canvasSize().width();
    canvas[QStringLiteral("height")]   = p.canvasSize().height();
    proj[QStringLiteral("canvasSize")] = canvas;
    proj[QStringLiteral("sampleRate")] = p.sampleRate();
    proj[QStringLiteral("channels")]   = p.channels();
    proj[QStringLiteral("duration")]   = double(p.timeline()->duration());
    proj[QStringLiteral("playhead")]   = double(p.playhead());
    proj[QStringLiteral("workRange")]  = timeRangeToJson(p.workRange());
    proj[QStringLiteral("colorSpace")] = p.colorSpaceName();
    root[kProject] = proj;

    // ---- アセット / 字幕スタイルプリセット ----
    root[kAssets] = serializeAssets(p);
    root[QStringLiteral("subtitleStylePresets")] = serializeSubtitleStylePresets(p);

    // ---- 無限レイヤー: Track の動的リスト。配列順 = Z オーダー ----
    root[kTracks] = serializeTracks(*p.timeline(), o);

    // ---- マスター音声 / 設定 ----
    QJsonObject master;
    master[QStringLiteral("gain")] = p.masterGain();
    root[QStringLiteral("masterAudio")] = master;

    // ---- メディアライブラリのフォルダ構成 (9.2.1) ----
    {
        const MediaFolderTree& tree = p.mediaFolders();
        QJsonArray folders;
        for (const MediaFolder& f : tree.folders) {
            QJsonObject fo;
            fo[QStringLiteral("id")] = uuidToJson(f.id);
            fo[kParentId] = f.parentId.isNull() ? QJsonValue(QJsonValue::Null)
                                                : QJsonValue(uuidToJson(f.parentId));
            fo[QStringLiteral("name")] = f.name;
            folders.append(fo);
        }
        QJsonObject assignments;
        for (auto it = tree.assignments.cbegin(); it != tree.assignments.cend(); ++it)
            assignments[uuidToJson(it.key())] = uuidToJson(it.value());

        QJsonObject library;
        library[kFolders]     = folders;
        library[kAssignments] = assignments;
        root[kLibrary] = library;
    }

    QJsonObject settings;
    settings[QStringLiteral("pdcEnabled")]   = p.isPdcEnabled();
    settings[QStringLiteral("proxyEnabled")] = p.isProxyEnabled();
    settings[QStringLiteral("autoCommitAi")] = p.isAutoCommitAi();
    root[QStringLiteral("settings")] = settings;

    return root;
}

bool ProjectSerializer::deserializeProject(Project* p, const QJsonObject& o, LoadResult* r)
{
    const QJsonObject proj = o[kProject].toObject();

    p->setName(proj[QStringLiteral("name")].toString());
    p->setTimebase(rationalFromJson(proj[QStringLiteral("timebase")].toObject(),
                                    timebase::Fps59_94));

    const QJsonObject canvas = proj[QStringLiteral("canvasSize")].toObject();
    if (!canvas.isEmpty()) {
        p->setCanvasSize(QSize(canvas[QStringLiteral("width")].toInt(3840),
                               canvas[QStringLiteral("height")].toInt(2160)));
    }

    p->setSampleRate(proj[QStringLiteral("sampleRate")].toInt(48000));
    p->setChannels(proj[QStringLiteral("channels")].toInt(2));
    p->setColorSpaceName(proj[QStringLiteral("colorSpace")].toString(QStringLiteral("bt709")));
    p->setPlayhead(int64_t(proj[QStringLiteral("playhead")].toDouble(0)));
    p->setWorkRange(timeRangeFromJson(proj[QStringLiteral("workRange")]));

    const QJsonObject master = o[QStringLiteral("masterAudio")].toObject();
    p->setMasterGain(master[QStringLiteral("gain")].toDouble(1.0));

    const QJsonObject settings = o[QStringLiteral("settings")].toObject();
    p->setPdcEnabled(settings[QStringLiteral("pdcEnabled")].toBool(true));
    p->setProxyEnabled(settings[QStringLiteral("proxyEnabled")].toBool(true));
    p->setAutoCommitAi(settings[QStringLiteral("autoCommitAi")].toBool(false));

    // ---- メディアライブラリのフォルダ構成 (9.2.1) ----
    {
        const QJsonObject library = o[kLibrary].toObject();
        MediaFolderTree tree;
        for (const QJsonValue& v : library[kFolders].toArray()) {
            const QJsonObject fo = v.toObject();
            MediaFolder f;
            f.id       = uuidFromJson(fo[QStringLiteral("id")]);
            f.parentId = uuidFromJson(fo[kParentId]);
            f.name     = fo[QStringLiteral("name")].toString();
            if (!f.id.isNull())
                tree.folders.push_back(std::move(f));
        }
        const QJsonObject assignments = library[kAssignments].toObject();
        for (auto it = assignments.begin(); it != assignments.end(); ++it) {
            const QUuid assetId = QUuid(it.key());
            const QUuid folderId = uuidFromJson(it.value());
            if (!assetId.isNull())
                tree.assignments.insert(assetId, folderId);
        }
        // 壊れたフォルダ参照は黙って捨てる (9.2.1)
        tree.dropDanglingAssignments();
        p->setMediaFolders(std::move(tree));
    }

    // ---- 字幕スタイルプリセット ----
    // プリセットテーブルの実体は app 層 (SubtitleStylePresetTable) が管理し、
    // ここでは読み飛ばす。未知フィールド保持により保存時には書き戻される。
    deserializeTracks(p->timeline(), o[kTracks].toArray(), p, r);

    return true;
}

// ===========================================================================
//  アセット
// ===========================================================================

QJsonArray ProjectSerializer::serializeAssets(const Project& p)
{
    QJsonArray arr;
    const AssetLibrary* lib = p.assets();
    if (!lib)
        return arr;

    for (const QUuid& id : lib->allIds()) {
        const Asset* a = lib->asset(id);
        if (!a)
            continue;

        QJsonObject ao;
        ao[QStringLiteral("id")]       = uuidToJson(a->id);
        ao[QStringLiteral("path")]     = PathResolver::normalizeSeparators(a->relativePath);
        ao[QStringLiteral("kind")]     = enumToString(a->kind);
        ao[QStringLiteral("hash")]     = a->hash;
        ao[QStringLiteral("duration")] = double(a->durationFrames);
        ao[QStringLiteral("frameRate")] = rationalToJson(a->frameRate);
        if (!a->resolution.isEmpty()) {
            QJsonObject res;
            res[QStringLiteral("width")]  = a->resolution.width();
            res[QStringLiteral("height")] = a->resolution.height();
            ao[QStringLiteral("resolution")] = res;
        }
        ao[QStringLiteral("hasAudio")] = a->hasAudio;
        if (!a->generatedByTaskId.isNull())
            ao[QStringLiteral("generatedByTaskId")] = uuidToJson(a->generatedByTaskId);
        arr.append(ao);
    }
    return arr;
}

void ProjectSerializer::deserializeAssets(Project* p, const QJsonArray& arr,
                                          LoadResult* r, const QString& projectFilePath)
{
    AssetLibrary* lib = p->assets();
    if (!lib)
        return;

    const PathResolver resolver(projectFilePath);
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();

        Asset a;
        a.id  = uuidFromJson(o[QStringLiteral("id")]);
        if (a.id.isNull())
            continue;
        a.kind      = enumFromString<Asset::Kind>(o[QStringLiteral("kind")].toString(),
                                                  Asset::Kind::Video);
        a.hash      = o[QStringLiteral("hash")].toString();
        a.durationFrames = int64_t(o[QStringLiteral("duration")].toDouble(0));
        a.frameRate = rationalFromJson(o[QStringLiteral("frameRate")].toObject(),
                                       timebase::Fps59_94);
        a.hasAudio  = o[QStringLiteral("hasAudio")].toBool(false);
        a.generatedByTaskId = uuidFromJson(o[QStringLiteral("generatedByTaskId")]);

        const QJsonObject res = o[QStringLiteral("resolution")].toObject();
        if (!res.isEmpty())
            a.resolution = QSize(res[QStringLiteral("width")].toInt(),
                                 res[QStringLiteral("height")].toInt());

        bool resolved = false;
        a.resolvedAbsolutePath = resolver.toAbsolute(o[QStringLiteral("path")].toString(),
                                                     &resolved);
        a.relativePath = PathResolver::normalizeSeparators(o[QStringLiteral("path")].toString());
        a.isMissing = !resolved;
        if (a.isMissing) {
            r->missingAssetPaths.append(a.relativePath);
            r->warnings.append(QObject::tr("Asset '%1' could not be found. "
                                           "Relink it from the library panel.")
                                   .arg(a.relativePath));
        }

        lib->addResolvedAsset(a);
    }
}

// ===========================================================================
//  字幕スタイルプリセット
// ===========================================================================

QJsonArray ProjectSerializer::serializeSubtitleStylePresets(const Project&)
{
    QJsonArray arr;
    // default プリセットのみを常に書き出す。
    // 完全なプリセットテーブルは app 層が保持し、io 層経由で拡張する。
    QJsonObject po;
    po[QStringLiteral("id")]   = QStringLiteral("default");
    po[QStringLiteral("name")] = QStringLiteral("Default");
    QJsonObject style;
    style[QStringLiteral("fontFamily")]    = QStringLiteral("Noto Sans JP");
    style[QStringLiteral("fontPointSize")] = 48.0;
    po[QStringLiteral("style")] = style;
    arr.append(po);
    return arr;
}

// ===========================================================================
//  トラック配列 (無限レイヤー)
// ===========================================================================

QJsonArray ProjectSerializer::serializeTracks(const Timeline& tl, const SaveOptions& o)
{
    QJsonArray arr;
    // 配列の順序がそのまま Z オーダーになる。
    // index 0 = 最背面 ... index N-1 = 最前面。
    // zOrder フィールドは意図的に持たない (二重管理を避けるため)。
    const int n = tl.trackCount();
    for (int i = 0; i < n; ++i) {
        const Track* t = tl.trackAt(i);
        if (!t)
            continue;
        arr.append(serializeTrack(*t, o));
    }
    return arr;
}

QJsonObject ProjectSerializer::serializeTrack(const Track& t, const SaveOptions& o)
{
    QJsonObject obj;

    obj[kTrackId]      = uuidToJson(t.id());
    obj[kTrackName]    = t.name();
    obj[kTrackType]    = enumToString(t.type());          ///< "video" / "audio" / ...
    obj[kTrackVisible] = t.isVisible();
    obj[kTrackLocked]  = t.isLocked();

    if (!o.omitDefaultValues || t.isMuted()) obj[QStringLiteral("muted")] = t.isMuted();
    if (!o.omitDefaultValues || t.isSolo())  obj[QStringLiteral("solo")]  = t.isSolo();

    obj[QStringLiteral("height")] = t.uiHeight();
    obj[QStringLiteral("color")]  = t.color().name(QColor::HexRgb);

    switch (t.type()) {
    case TrackType::Video:
    case TrackType::AiGenerated:
        if (!o.omitDefaultValues || t.opacity() != 1.0)
            obj[QStringLiteral("opacity")] = t.opacity();
        if (!o.omitDefaultValues || t.blendMode() != BlendMode::Normal)
            obj[QStringLiteral("blendMode")] = enumToString(t.blendMode());
        break;
    case TrackType::Audio:
        if (!o.omitDefaultValues || t.gain() != 1.0) obj[QStringLiteral("gain")] = t.gain();
        if (!o.omitDefaultValues || t.pan() != 0.0)  obj[QStringLiteral("pan")]  = t.pan();
        break;
    case TrackType::Subtitle:
        obj[QStringLiteral("defaultStylePresetId")] = t.defaultStylePresetId();
        break;
    }

    // ---- クリップ配列 (時刻順) ----
    QJsonArray clips;
    for (const std::shared_ptr<Clip>& c : t.clips()) {
        if (!c)
            continue;
        clips.append(serializeClip(*c, o));
    }
    obj[kClips] = clips;

    // ---- トランジション (9.3.2)。クリップ境界に付く独立オブジェクト ----
    if (!t.transitions().empty()) {
        QJsonArray transitions;
        for (const Transition& tr : t.transitions())
            transitions.append(tr.toJson());
        obj[kTransitions] = transitions;
    }

    return obj;
}

std::unique_ptr<Track> ProjectSerializer::deserializeTrack(const QJsonObject& o,
                                                           Project* p, LoadResult* r)
{
    const TrackType type = enumFromString<TrackType>(o[kTrackType].toString(),
                                                     TrackType::Video);
    auto track = std::make_unique<Track>(type);

    const QUuid id = uuidFromJson(o[kTrackId]);
    track->setId(id.isNull() ? QUuid::createUuid() : id);
    track->setName(o[kTrackName].toString());
    track->setVisible(o[kTrackVisible].toBool(true));
    track->setLocked(o[kTrackLocked].toBool(false));
    track->setMuted(o[QStringLiteral("muted")].toBool(false));
    track->setSolo(o[QStringLiteral("solo")].toBool(false));
    track->setUiHeight(o[QStringLiteral("height")].toInt(64));
    if (o.contains(QStringLiteral("color")))
        track->setColor(QColor(o[QStringLiteral("color")].toString()));

    switch (type) {
    case TrackType::Video:
    case TrackType::AiGenerated:
        track->setOpacity(o[QStringLiteral("opacity")].toDouble(1.0));
        track->setBlendMode(enumFromString<BlendMode>(
            o[QStringLiteral("blendMode")].toString(), BlendMode::Normal));
        break;
    case TrackType::Audio:
        track->setGain(o[QStringLiteral("gain")].toDouble(1.0));
        track->setPan(o[QStringLiteral("pan")].toDouble(0.0));
        break;
    case TrackType::Subtitle:
        track->setDefaultStylePresetId(
            o[QStringLiteral("defaultStylePresetId")].toString(QStringLiteral("default")));
        break;
    }

    // ---- クリップ配列 ----
    const QJsonArray clips = o[kClips].toArray();
    for (int i = 0; i < clips.size(); ++i) {
        auto clip = deserializeClip(clips.at(i).toObject(), p, r);
        if (!clip)
            continue;
        if (!track->insertClip(clip)) {
            // 重なりがあった。無限レイヤーなので、警告して詰める。
            r->warnings.append(
                QObject::tr("Clip '%1' on track '%2' overlaps another clip and was shifted.")
                    .arg(clip->name(), track->name()));
            const int64_t freeStart = track->findFreeStart(track->contentDuration(),
                                                           clip->range().duration);
            clip->setRange({freeStart, clip->range().duration});
            track->insertClip(clip);
        }
    }

    // ---- トランジション (9.3.2) ----
    // 参照先クリップが無い / 境界が一致しない / ハンドルが足りないものは
    // setTransitions() 側で落ちる。手で編集された JSON を無条件に信じない。
    const QJsonArray transitions = o[kTransitions].toArray();
    if (!transitions.isEmpty()) {
        std::vector<Transition> list;
        list.reserve(size_t(transitions.size()));
        for (const QJsonValue& v : transitions)
            list.push_back(Transition::fromJson(v.toObject()));

        const size_t requested = list.size();
        track->setTransitions(std::move(list));
        const size_t kept = track->transitions().size();
        if (kept < requested) {
            r->warnings.append(
                QObject::tr("%n transition(s) on track '%1' were dropped as invalid.", "",
                            int(requested - kept)).arg(track->name()));
        }
    }

    track->assertInvariants();
    return track;
}

void ProjectSerializer::deserializeTracks(Timeline* tl, const QJsonArray& arr,
                                          Project* p, LoadResult* r)
{
    // 配列の順序をそのまま Z オーダーとして復元する。
    // 途中のトラックが壊れていても、後続を読み込めるようにする
    // (1 トラックの破損で全体が開けなくなるのを避ける)。
    for (int i = 0; i < arr.size(); ++i) {
        const QJsonValue v = arr.at(i);
        if (!v.isObject()) {
            r->warnings.append(QObject::tr("Track #%1 is not an object; skipped.").arg(i));
            continue;
        }
        auto track = deserializeTrack(v.toObject(), p, r);
        if (!track) {
            r->warnings.append(QObject::tr("Track #%1 could not be loaded; skipped.").arg(i));
            continue;
        }
        // 末尾へ追加していけば、配列順 = Z オーダーが自然に再現される
        tl->reinsertTrack(tl->trackCount(), std::move(track));
    }
}

// ===========================================================================
//  クリップ (基底 + 型ごとの分岐)
// ===========================================================================

QJsonObject ProjectSerializer::serializeClip(const Clip& c, const SaveOptions& o)
{
    // 認識できなかったフィールドを先に入れておき、既知フィールドで上書きする。
    // これにより、新しいバージョンで追加されたデータを古いバージョンで
    // 開いて保存しても失われない (前方互換、9.11.2)。
    QJsonObject obj = c.unknownFields();

    obj[kClipId]   = uuidToJson(c.id());
    obj[kClipType] = enumToString(c.type());
    obj[kRange]    = timeRangeToJson(c.range());

    if (!c.name().isEmpty()) obj[QStringLiteral("name")] = c.name();
    if (!o.omitDefaultValues || !c.isEnabled())
        obj[kEnabled] = c.isEnabled();
    if (!o.omitDefaultValues || c.isLocked())
        obj[QStringLiteral("locked")] = c.isLocked();
    if (!o.omitDefaultValues || c.opacity() != 1.0)
        obj[QStringLiteral("opacity")] = c.opacity();
    if (!o.omitDefaultValues || c.blendMode() != BlendMode::Normal)
        obj[QStringLiteral("blendMode")] = enumToString(c.blendMode());
    if (c.fadeInFrames() > 0)  obj[QStringLiteral("fadeIn")]  = double(c.fadeInFrames());
    if (c.fadeOutFrames() > 0) obj[QStringLiteral("fadeOut")] = double(c.fadeOutFrames());

    if (!c.generatedByTaskId().isNull())
        obj[QStringLiteral("generatedByTaskId")] = uuidToJson(c.generatedByTaskId());

    // ---- ビデオフィルタースタック (9.4.1)。配列順が適用順 ----
    if (!c.filters().empty()) {
        QJsonArray filters;
        for (const VideoFilterInstance& f : c.filters())
            filters.append(f.toJson());
        obj[kFilters] = filters;
    }

    switch (c.type()) {

    case ClipType::Video:
    case ClipType::Image: {
        const auto& vc = static_cast<const VideoClip&>(c);
        obj[kAssetId]      = uuidToJson(vc.assetId());
        obj[kSourceOffset] = double(vc.sourceOffset());
        if (!o.omitDefaultValues || vc.speed() != 1.0)
            obj[QStringLiteral("speed")] = vc.speed();
        if (!o.omitDefaultValues || vc.isReversed())
            obj[QStringLiteral("reversed")] = vc.isReversed();
        if (!o.omitDefaultValues || vc.cropRect() != QRectF(0.0, 0.0, 1.0, 1.0))
            obj[QStringLiteral("crop")] = rectToJson(vc.cropRect());
        break;
    }

    case ClipType::Audio: {
        const auto& ac = static_cast<const AudioClip&>(c);
        obj[kAssetId]      = uuidToJson(ac.assetId());
        obj[kSourceOffset] = double(ac.sourceOffset());
        if (!o.omitDefaultValues || ac.gain() != 1.0) obj[QStringLiteral("gain")] = ac.gain();
        if (!o.omitDefaultValues || ac.pan() != 0.0)  obj[QStringLiteral("pan")]  = ac.pan();
        break;
    }

    case ClipType::Subtitle: {
        // 字幕は専用の入れ子構造を持つ
        const auto& sc = static_cast<const subtitle::SubtitleClip&>(c);
        mergeInto(obj, serializeSubtitleClip(sc));
        break;
    }

    case ClipType::Title: {
        // タイトルは字幕と同じ構造 + presetId (9.4.6)
        const auto& tc = static_cast<const subtitle::TitleClip&>(c);
        mergeInto(obj, serializeSubtitleClip(tc));
        obj[kPresetId] = tc.presetId();
        break;
    }

    case ClipType::AiPlaceholder: {
        const auto& pc = static_cast<const AiPlaceholderClip&>(c);
        obj[QStringLiteral("taskId")]  = uuidToJson(pc.taskId());
        obj[QStringLiteral("state")]   = int(pc.state());
        obj[QStringLiteral("progress")] = pc.progress();
        break;
    }

    case ClipType::Color: {
        const auto& cc = static_cast<const ColorClip&>(c);
        obj[QStringLiteral("colorValue")] = cc.color().name(QColor::HexArgb);
        break;
    }
    }

    return obj;
}

std::shared_ptr<Clip> ProjectSerializer::deserializeClip(const QJsonObject& o,
                                                         Project*, LoadResult* r)
{
    const ClipType type = enumFromString<ClipType>(o[kClipType].toString(), ClipType::Video);

    std::shared_ptr<Clip> clip;
    switch (type) {
    case ClipType::Video:
    case ClipType::Image:         clip = std::make_shared<VideoClip>();         break;
    case ClipType::Audio:         clip = std::make_shared<AudioClip>();         break;
    case ClipType::Subtitle:
    case ClipType::Title:         return deserializeSubtitleClip(o, nullptr, r);
    case ClipType::AiPlaceholder: clip = std::make_shared<AiPlaceholderClip>(); break;
    case ClipType::Color:         clip = std::make_shared<ColorClip>();         break;
    }
    if (!clip)
        return nullptr;

    // ---- 未知フィールドを保持する (前方互換) ----
    clip->setUnknownFields(extractUnknownFields(o, knownClipKeys()));

    const QUuid id = uuidFromJson(o[kClipId]);
    clip->setId(id.isNull() ? QUuid::createUuid() : id);
    clip->setRange(timeRangeFromJson(o[kRange]));
    clip->setName(o[QStringLiteral("name")].toString());
    clip->setEnabled(o[kEnabled].toBool(true));
    clip->setLocked(o[QStringLiteral("locked")].toBool(false));
    clip->setOpacity(o[QStringLiteral("opacity")].toDouble(1.0));
    clip->setBlendMode(enumFromString<BlendMode>(
        o[QStringLiteral("blendMode")].toString(), BlendMode::Normal));
    clip->setFadeInFrames(int64_t(o[QStringLiteral("fadeIn")].toDouble(0)));
    clip->setFadeOutFrames(int64_t(o[QStringLiteral("fadeOut")].toDouble(0)));
    clip->setGeneratedByTaskId(uuidFromJson(o[QStringLiteral("generatedByTaskId")]));
    clip->setFilters(deserializeFilters(o));

    switch (type) {
    case ClipType::Video:
    case ClipType::Image: {
        auto* vc = static_cast<VideoClip*>(clip.get());
        vc->setAssetId(uuidFromJson(o[kAssetId]));
        vc->setSourceOffset(int64_t(o[kSourceOffset].toDouble(0)));
        vc->setSpeed(o[QStringLiteral("speed")].toDouble(1.0));
        vc->setReversed(o[QStringLiteral("reversed")].toBool(false));
        vc->setCropRect(rectFromJson(o[QStringLiteral("crop")],
                                     QRectF(0.0, 0.0, 1.0, 1.0)));
        break;
    }
    case ClipType::Audio: {
        auto* ac = static_cast<AudioClip*>(clip.get());
        ac->setAssetId(uuidFromJson(o[kAssetId]));
        ac->setSourceOffset(int64_t(o[kSourceOffset].toDouble(0)));
        ac->setGain(o[QStringLiteral("gain")].toDouble(1.0));
        ac->setPan(o[QStringLiteral("pan")].toDouble(0.0));
        break;
    }
    case ClipType::AiPlaceholder: {
        auto* pc = static_cast<AiPlaceholderClip*>(clip.get());
        pc->setTaskId(uuidFromJson(o[QStringLiteral("taskId")]));
        pc->setState(AiPlaceholderClip::State(
                         qBound(0, o[QStringLiteral("state")].toInt(0), 4)));
        pc->setProgress(o[QStringLiteral("progress")].toDouble(0.0));
        break;
    }
    case ClipType::Color: {
        auto* cc = static_cast<ColorClip*>(clip.get());
        cc->setColor(QColor(o[QStringLiteral("colorValue")].toString()));
        break;
    }
    case ClipType::Subtitle:
    case ClipType::Title:
        break;   ///< handled above
    }

    return clip;
}

// ===========================================================================
//  字幕クリップ
// ===========================================================================

QJsonObject ProjectSerializer::serializeSubtitleClip(const subtitle::SubtitleClip& c)
{
    QJsonObject obj;

    obj[kStylePresetId] = c.stylePresetId();

    // ---- テキスト (プレーン + リッチスパン) ----
    QJsonObject text;
    text[kTextPlain] = c.text().plain();

    QJsonArray spans;
    for (const subtitle::TextSpan& s : c.text().spans()) {
        QJsonObject so;
        so[QStringLiteral("start")]  = s.start;
        so[QStringLiteral("length")] = s.length;
        if (s.bold)      so[QStringLiteral("bold")]      = *s.bold;
        if (s.italic)    so[QStringLiteral("italic")]    = *s.italic;
        if (s.underline) so[QStringLiteral("underline")] = *s.underline;
        if (s.color)     so[QStringLiteral("color")]     = s.color->name(QColor::HexArgb);
        if (s.fontFamily) so[QStringLiteral("fontFamily")] = *s.fontFamily;
        if (s.sizeScale) so[QStringLiteral("sizeScale")]  = *s.sizeScale;
        if (!s.ruby.isEmpty()) so[QStringLiteral("ruby")] = s.ruby;
        spans.append(so);
    }
    if (!spans.isEmpty())
        text[kTextSpans] = spans;
    obj[kText] = text;

    // ---- スタイル差分 (プリセットからのオーバーライド分のみ) ----
    const subtitle::SubtitleStyleDiff& d = c.styleOverride();
    QJsonObject styleDiff;
    if (d.fontFamily)    styleDiff[QStringLiteral("fontFamily")]    = *d.fontFamily;
    if (d.fontPointSize) styleDiff[QStringLiteral("fontPointSize")] = *d.fontPointSize;
    if (d.fontWeight)    styleDiff[QStringLiteral("fontWeight")]    = *d.fontWeight;
    if (d.italic)        styleDiff[QStringLiteral("italic")]        = *d.italic;
    if (d.fillColor)     styleDiff[QStringLiteral("fillColor")]     = d.fillColor->name(QColor::HexArgb);
    if (d.outlineColor)  styleDiff[QStringLiteral("outlineColor")]  = d.outlineColor->name(QColor::HexArgb);
    if (d.outlineWidth)  styleDiff[QStringLiteral("outlineWidth")]  = *d.outlineWidth;
    if (d.shadowColor)   styleDiff[QStringLiteral("shadowColor")]   = d.shadowColor->name(QColor::HexArgb);
    if (d.shadowBlur)    styleDiff[QStringLiteral("shadowBlur")]    = *d.shadowBlur;
    if (d.boxEnabled)    styleDiff[QStringLiteral("boxEnabled")]    = *d.boxEnabled;
    if (d.boxColor)      styleDiff[QStringLiteral("boxColor")]      = d.boxColor->name(QColor::HexArgb);
    if (d.hAlign)        styleDiff[QStringLiteral("hAlign")]        = *d.hAlign;
    if (d.vAlign)        styleDiff[QStringLiteral("vAlign")]        = *d.vAlign;
    if (d.anchor) {
        QJsonObject anchor;
        anchor[QStringLiteral("x")] = d.anchor->x();
        anchor[QStringLiteral("y")] = d.anchor->y();
        styleDiff[QStringLiteral("anchor")] = anchor;
    }
    if (d.lineSpacing)   styleDiff[QStringLiteral("lineSpacing")]   = *d.lineSpacing;
    if (d.letterSpacing) styleDiff[QStringLiteral("letterSpacing")] = *d.letterSpacing;
    if (d.maxWidthRatio) styleDiff[QStringLiteral("maxWidthRatio")] = *d.maxWidthRatio;
    if (d.rotationDeg)   styleDiff[QStringLiteral("rotationDeg")]   = *d.rotationDeg;
    if (d.scale) {
        QJsonObject scale;
        scale[QStringLiteral("x")] = d.scale->x();
        scale[QStringLiteral("y")] = d.scale->y();
        styleDiff[QStringLiteral("scale")] = scale;
    }
    if (d.opacity)       styleDiff[QStringLiteral("opacity")]       = *d.opacity;
    if (d.vertical)      styleDiff[QStringLiteral("vertical")]      = *d.vertical;
    if (!styleDiff.isEmpty())
        obj[kStyleOverride] = styleDiff;

    // ---- エフェクトスタック ----
    if (!c.effectStack().empty())
        obj[kEffectStack] = serializeSubtitleEffectStack(c.effectStack());

    // ---- STT 由来の単語タイミング ----
    if (c.hasWordTimings()) {
        QJsonArray wt;
        for (const auto& w : c.wordTimings()) {
            QJsonObject wo;
            wo[QStringLiteral("charStart")]  = w.charStart;
            wo[QStringLiteral("charLength")] = w.charLength;
            wo[QStringLiteral("startSec")]   = w.startSec;
            wo[QStringLiteral("endSec")]     = w.endSec;
            wt.append(wo);
        }
        obj[kWordTimings] = wt;
    }

    return obj;
}

std::shared_ptr<subtitle::SubtitleClip>
ProjectSerializer::deserializeSubtitleClip(const QJsonObject& o, Project*, LoadResult* r)
{
    // type が "title" なら TitleClip を作る。以降の読み込みは字幕と共通 (9.4.6)。
    const bool isTitle =
        enumFromString<ClipType>(o[kClipType].toString(), ClipType::Subtitle) == ClipType::Title;

    std::shared_ptr<subtitle::SubtitleClip> clip;
    if (isTitle) {
        auto title = std::make_shared<subtitle::TitleClip>();
        title->setPresetId(o[kPresetId].toString());
        clip = title;
    } else {
        clip = std::make_shared<subtitle::SubtitleClip>();
    }
    clip->setUnknownFields(extractUnknownFields(o, knownSubtitleClipKeys()));
    clip->setFilters(deserializeFilters(o));

    const QUuid id = uuidFromJson(o[kClipId]);
    clip->setId(id.isNull() ? QUuid::createUuid() : id);
    clip->setRange(timeRangeFromJson(o[kRange]));
    clip->setEnabled(o[kEnabled].toBool(true));
    clip->setName(o[QStringLiteral("name")].toString());
    clip->setStylePresetId(o[kStylePresetId].toString(QStringLiteral("default")));
    clip->setGeneratedByTaskId(uuidFromJson(o[QStringLiteral("generatedByTaskId")]));

    // ---- テキスト ----
    const QJsonObject textObj = o[kText].toObject();
    subtitle::SubtitleText text;
    text.setPlain(textObj[kTextPlain].toString());
    for (const QJsonValue& sv : textObj[kTextSpans].toArray()) {
        const QJsonObject so = sv.toObject();
        subtitle::TextSpan span;
        span.start  = so[QStringLiteral("start")].toInt();
        span.length = so[QStringLiteral("length")].toInt();
        if (so.contains(QStringLiteral("bold")))
            span.bold = so[QStringLiteral("bold")].toBool();
        if (so.contains(QStringLiteral("italic")))
            span.italic = so[QStringLiteral("italic")].toBool();
        if (so.contains(QStringLiteral("underline")))
            span.underline = so[QStringLiteral("underline")].toBool();
        if (so.contains(QStringLiteral("color")))
            span.color = QColor(so[QStringLiteral("color")].toString());
        if (so.contains(QStringLiteral("fontFamily")))
            span.fontFamily = so[QStringLiteral("fontFamily")].toString();
        if (so.contains(QStringLiteral("sizeScale")))
            span.sizeScale = so[QStringLiteral("sizeScale")].toDouble();
        span.ruby = so[QStringLiteral("ruby")].toString();
        text.addSpan(span);
    }
    clip->setText(text);

    // ---- スタイル差分 ----
    const QJsonObject sd = o[kStyleOverride].toObject();
    if (!sd.isEmpty()) {
        subtitle::SubtitleStyleDiff diff;
        if (sd.contains(QStringLiteral("fontFamily")))
            diff.fontFamily = sd[QStringLiteral("fontFamily")].toString();
        if (sd.contains(QStringLiteral("fontPointSize")))
            diff.fontPointSize = sd[QStringLiteral("fontPointSize")].toDouble();
        if (sd.contains(QStringLiteral("fontWeight")))
            diff.fontWeight = sd[QStringLiteral("fontWeight")].toInt();
        if (sd.contains(QStringLiteral("italic")))
            diff.italic = sd[QStringLiteral("italic")].toBool();
        if (sd.contains(QStringLiteral("fillColor")))
            diff.fillColor = QColor(sd[QStringLiteral("fillColor")].toString());
        if (sd.contains(QStringLiteral("outlineColor")))
            diff.outlineColor = QColor(sd[QStringLiteral("outlineColor")].toString());
        if (sd.contains(QStringLiteral("outlineWidth")))
            diff.outlineWidth = sd[QStringLiteral("outlineWidth")].toDouble();
        if (sd.contains(QStringLiteral("shadowColor")))
            diff.shadowColor = QColor(sd[QStringLiteral("shadowColor")].toString());
        if (sd.contains(QStringLiteral("shadowBlur")))
            diff.shadowBlur = sd[QStringLiteral("shadowBlur")].toDouble();
        if (sd.contains(QStringLiteral("boxEnabled")))
            diff.boxEnabled = sd[QStringLiteral("boxEnabled")].toBool();
        if (sd.contains(QStringLiteral("boxColor")))
            diff.boxColor = QColor(sd[QStringLiteral("boxColor")].toString());
        if (sd.contains(QStringLiteral("hAlign")))
            diff.hAlign = sd[QStringLiteral("hAlign")].toInt();
        if (sd.contains(QStringLiteral("vAlign")))
            diff.vAlign = sd[QStringLiteral("vAlign")].toInt();
        if (sd.contains(QStringLiteral("anchor"))) {
            const QJsonObject a = sd[QStringLiteral("anchor")].toObject();
            diff.anchor = QPointF(a[QStringLiteral("x")].toDouble(0.5),
                                  a[QStringLiteral("y")].toDouble(0.92));
        }
        if (sd.contains(QStringLiteral("lineSpacing")))
            diff.lineSpacing = sd[QStringLiteral("lineSpacing")].toDouble();
        if (sd.contains(QStringLiteral("letterSpacing")))
            diff.letterSpacing = sd[QStringLiteral("letterSpacing")].toDouble();
        if (sd.contains(QStringLiteral("maxWidthRatio")))
            diff.maxWidthRatio = sd[QStringLiteral("maxWidthRatio")].toDouble();
        if (sd.contains(QStringLiteral("rotationDeg")))
            diff.rotationDeg = sd[QStringLiteral("rotationDeg")].toDouble();
        if (sd.contains(QStringLiteral("scale"))) {
            const QJsonObject sc = sd[QStringLiteral("scale")].toObject();
            diff.scale = QPointF(sc[QStringLiteral("x")].toDouble(1.0),
                                 sc[QStringLiteral("y")].toDouble(1.0));
        }
        if (sd.contains(QStringLiteral("opacity")))
            diff.opacity = sd[QStringLiteral("opacity")].toDouble();
        if (sd.contains(QStringLiteral("vertical")))
            diff.vertical = sd[QStringLiteral("vertical")].toBool();
        clip->setStyleOverride(diff);
    }

    // ---- エフェクトスタック ----
    if (o.contains(kEffectStack)) {
        auto stack = deserializeSubtitleEffectStack(o[kEffectStack].toArray(), r);
        clip->mutableEffectStack() = std::move(stack);
    }

    // ---- 単語タイミング ----
    if (o.contains(kWordTimings)) {
        std::vector<subtitle::SubtitleClip::WordTiming> wt;
        for (const QJsonValue& v : o[kWordTimings].toArray()) {
            const QJsonObject wo = v.toObject();
            subtitle::SubtitleClip::WordTiming timing;
            timing.charStart  = wo[QStringLiteral("charStart")].toInt();
            timing.charLength = wo[QStringLiteral("charLength")].toInt();
            timing.startSec   = wo[QStringLiteral("startSec")].toDouble();
            timing.endSec     = wo[QStringLiteral("endSec")].toDouble();
            wt.push_back(timing);
        }
        clip->setWordTimings(std::move(wt));
    }

    return clip;
}

QJsonArray ProjectSerializer::serializeSubtitleEffectStack(
    const std::vector<subtitle::SubtitleEffectInstance>& stack)
{
    QJsonArray arr;

    // 配列の順序 = 適用順。index 0 が最初に適用される。
    for (const subtitle::SubtitleEffectInstance& inst : stack) {
        QJsonObject io_;
        io_[kInstanceId] = uuidToJson(inst.instanceId);
        io_[kEffectId]   = inst.effectId;
        if (!inst.pluginId.isEmpty())
            io_[kPluginId] = inst.pluginId;
        io_[kEnabled] = inst.enabled;

        // 未インストールプラグイン (missing == true) の場合も
        // params には読み込み時の値がそのまま入っているため、
        // 何も失わずに書き戻せる。
        io_[kParams] = inst.params.toJson();

        arr.append(io_);
    }
    return arr;
}

std::vector<subtitle::SubtitleEffectInstance>
ProjectSerializer::deserializeSubtitleEffectStack(const QJsonArray& arr, LoadResult* r)
{
    std::vector<subtitle::SubtitleEffectInstance> stack;
    stack.reserve(size_t(arr.size()));

    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();

        subtitle::SubtitleEffectInstance inst;
        const QUuid iid = uuidFromJson(o[kInstanceId]);
        inst.instanceId = iid.isNull() ? QUuid::createUuid() : iid;
        inst.effectId   = o[kEffectId].toString();
        inst.pluginId   = o[kPluginId].toString();
        inst.enabled    = o[kEnabled].toBool(true);
        inst.params     = yave::sdk::ParameterValues::fromJson(o[kParams].toObject());

        // プラグインが見つからなくても params を破棄しない。
        // missing フラグを立ててスタックに残し、保存時にそのまま書き戻す。
        // 後でプラグインをインストールすれば、そのまま復活する。
        inst.effect  = subtitle::createEffectViaFactory(inst.effectId);
        inst.missing = (inst.effect == nullptr);

        if (inst.missing && !inst.effectId.isEmpty()) {
            r->missingPluginIds.append(inst.effectId);
            r->warnings.append(
                QObject::tr("Subtitle effect '%1' is not installed. "
                            "Its settings are preserved but it will not be applied.")
                    .arg(inst.effectId));
        }

        stack.push_back(std::move(inst));
    }
    return stack;
}

// ===========================================================================
//  AI パラメータ / タスク
// ===========================================================================

QJsonObject ProjectSerializer::serializeAiParams(const AiGenerationParams& p)
{
    // AiGenerationParams 自身が完全な JSON 変換を実装しているため委譲する。
    // プロジェクトスキーマ上のキー名 ("params" 配下の入れ子構造) との対応は
    // 9.5 のドキュメントを参照。
    return p.toJson();
}

AiGenerationParams ProjectSerializer::deserializeAiParamsObject(const QJsonObject& o)
{
    return AiGenerationParams::fromJson(o);
}

QJsonObject ProjectSerializer::serializeAiTask(const ai::AiGenerationTask& t)
{
    QJsonObject o;
    o[QStringLiteral("id")]           = uuidToJson(t.id());
    o[QStringLiteral("state")]        = enumToString(t.state());
    o[QStringLiteral("createdAt")]    = t.createdAt().toString(Qt::ISODate);
    if (t.completedAt().isValid())
        o[QStringLiteral("completedAt")] = t.completedAt().toString(Qt::ISODate);
    o[QStringLiteral("retryCount")]   = t.retryCount();
    if (!t.errorMessage().isEmpty())
        o[QStringLiteral("errorMessage")] = t.errorMessage();

    // 生成パラメータを完全に永続化する。
    // これによりキャッシュが消えても同じ設定で再生成できる。
    o[kParams] = serializeAiParams(t.params());

    // ---- 生成済みアセット ----
    QJsonArray assets;
    for (const ai::GeneratedAsset& a : t.assets()) {
        QJsonObject ao;
        ao[QStringLiteral("type")]      = enumToString(a.type);
        ao[QStringLiteral("path")]      = a.path;         ///< プロジェクト相対
        ao[QStringLiteral("collected")] = a.collected;
        QJsonObject res;
        res[QStringLiteral("width")]  = a.resolution.width();
        res[QStringLiteral("height")] = a.resolution.height();
        ao[QStringLiteral("resolution")]     = res;
        ao[QStringLiteral("durationFrames")] = double(a.durationFrames);
        ao[QStringLiteral("frameRate")]      = rationalToJson(a.frameRate);
        if (!a.metadata.isEmpty())
            ao[QStringLiteral("metadata")] = a.metadata;
        assets.append(ao);
    }
    o[QStringLiteral("assets")] = assets;

    return o;
}

// ===========================================================================
//  マイグレーション / バックアップ
// ===========================================================================

void ProjectSerializer::applyMigrations(QJsonObject& root, int fromVersion, LoadResult* r)
{
    int v = fromVersion;
    while (v < kCurrentSchemaVersion) {
        auto it = std::find_if(migrations().begin(), migrations().end(),
                               [v](const Migration& m) { return m.fromVersion == v; });
        if (it == migrations().end()) {
            r->warnings.append(QObject::tr(
                "No migration path from schema version %1.").arg(v));
            break;
        }
        it->fn(root, r);
        ++v;
        r->migrated = true;
    }
}

void ProjectSerializer::rotateBackups(const QString& path, int generations)
{
    // MyProject.yave.bak(N-1) -> .bakN ... 直前の保存世代を残す
    QFile newest(path + QStringLiteral(".bak1"));
    if (newest.exists()) {
        for (int i = generations - 1; i >= 1; --i) {
            QFile::rename(path + QStringLiteral(".bak%1").arg(i),
                          path + QStringLiteral(".bak%1").arg(i + 1));
        }
    }
    QFile::copy(path, path + QStringLiteral(".bak1"));
}

} // namespace yave::io
