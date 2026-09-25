#include "ProjectController.h"

#include "../library/LibraryStore.h"

#include "../../io/ProjectSerializer.h"
#include "../../util/Log.h"
#include "../../core/Timeline.h"
#include "../../core/TrackType.h"
#include "../../core/AssetLibrary.h"
#include "../../media/MediaProbe.h"
#include <QUrl>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <fstream>

namespace yave {

namespace {
constexpr auto kLastSavedPathKey = "project/lastSavedPath";

/// FileDialog 等から渡される "file:///..." URL 文字列をローカルパスへ変換する。
/// 既にローカルパスの場合はそのまま返す。
QString resolveLocalPath(const QString& pathOrUrl)
{
    if (pathOrUrl.contains(QLatin1String("://"))) {
        const QUrl url(pathOrUrl);
        if (url.isLocalFile())
            return url.toLocalFile();
    }
    return pathOrUrl;
}
}

ProjectController::ProjectController(QObject* parent)
    : QObject(parent)
{
    newProject();
}

void ProjectController::newProject(const QString& name)
{
    project_ = std::make_unique<Project>();
    connect(project_->undoStack(), &QUndoStack::cleanChanged, this, [this](bool clean) {
        Q_UNUSED(clean);
        emit modifiedChanged();
    });

    // 既定のタイムライン構成: 映像 1 + 音声 1 + 字幕 1
    project_->timeline()->appendTrack(TrackType::Video);
    project_->timeline()->appendTrack(TrackType::Audio);
    project_->timeline()->appendTrack(TrackType::Subtitle);

    project_->setName(name.isEmpty() ? tr("Untitled Project") : name);
    projectPath_.clear();
    emit projectClosed();
}

bool ProjectController::open(const QString& path)
{
    const QString localPath = resolveLocalPath(path);

    auto loaded = std::make_unique<Project>();
    const io::LoadResult result = io::ProjectSerializer::load(loaded.get(), localPath);
    if (!result.ok) {
        qCWarning(lcApp) << "Failed to open project:" << result.errorMessage;
        return false;
    }

    project_ = std::move(loaded);
    connect(project_->undoStack(), &QUndoStack::cleanChanged, this, [this](bool clean) {
        Q_UNUSED(clean);
        emit modifiedChanged();
    });
    projectPath_ = localPath;
    QSettings().setValue(QLatin1String(kLastSavedPathKey), localPath);
    emit projectOpened(localPath);
    return true;
}

bool ProjectController::save()
{
    if (projectPath_.isEmpty() || !project_)
        return false;
    io::SaveOptions opts;
    QString err;
    if (!io::ProjectSerializer::save(*project_, projectPath_, opts, &err)) {
        qCWarning(lcApp) << "Save failed:" << err;
        return false;
    }
    project_->clearModified();
    project_->undoStack()->setClean();
    QSettings().setValue(QLatin1String(kLastSavedPathKey), projectPath_);
    emit projectSaved(projectPath_);
    return true;
}

bool ProjectController::saveAs(const QString& path)
{
    projectPath_ = resolveLocalPath(path);
    return save();
}

bool ProjectController::restoreAutosave()
{
    const QString autosave = projectPath_ + QStringLiteral(".autosave");
    if (!QFileInfo::exists(autosave))
        return false;
    return open(autosave);
}

QString ProjectController::lastSavedPath() const
{
    return QSettings().value(QLatin1String(kLastSavedPathKey)).toString();
}

bool ProjectController::openLastSave()
{
    const QString path = lastSavedPath();
    if (path.isEmpty() || !QFileInfo::exists(path))
        return false;
    return open(path);
}

bool ProjectController::isModified() const
{
    return project_ && !project_->undoStack()->isClean();
}

QString ProjectController::registerAsset(const QString& absolutePathOrUrl)
{
    const QString absolutePath = resolveLocalPath(absolutePathOrUrl);

    qInfo() << "[registerAsset] resolved path:" << absolutePath
            << "(from:" << absolutePathOrUrl << ")";

    if (!project_) {
        qWarning() << "[registerAsset] REJECTED: no project is open";
        return {};
    }

    // 1. メディアの情報をプローブする
    media::MediaInfo info = media::MediaProbe::probe(absolutePath);

    qInfo() << "[registerAsset] probe ok:" << info.ok
            << "hasVideo:" << info.hasVideo
            << "hasAudio:" << info.hasAudio
            << "duration:" << info.durationFrames
            << "audioDuration:" << info.audioDurationFrames
            << "error:" << info.error;
    
    // 2. アセット種別の判定
    Asset::Kind kind = Asset::Kind::Video;
    if (info.ok) {
        if (info.hasVideo) {
            kind = Asset::Kind::Video;
        } else if (info.hasAudio) {
            kind = Asset::Kind::Audio;
        }
    } else {
        // プローブに失敗した場合、拡張子で簡易判定するか、既定でVideoとする
        QString ext = QFileInfo(absolutePath).suffix().toLower();
        if (ext == QLatin1String("mp3") || ext == QLatin1String("wav") || ext == QLatin1String("aac") || ext == QLatin1String("m4a")) {
            kind = Asset::Kind::Audio;
        } else if (ext == QLatin1String("png") || ext == QLatin1String("jpg") || ext == QLatin1String("jpeg") || ext == QLatin1String("bmp")) {
            kind = Asset::Kind::Image;
        }
    }

    // 3. アセットを登録
    Asset* a = project_->assets()->registerAsset(absolutePath, kind);
    if (!a) {
        qWarning() << "[registerAsset] REJECTED: failed to register asset for path:" << absolutePath;
        return {};
    }

    // 4. メディア情報・プロジェクトタイムベース換算の反映
    if (info.ok) {
        a->resolution = info.resolution;
        a->hasAudio = info.hasAudio;
        Rational projTimebase = project_->timebase();
        // timebase = num/den [秒/フレーム] なので、フレーム数 = 秒 * den / num。
        // (例: timebase=1001/60000 なら 1 秒 = 60000/1001 ≒ 59.94 フレーム)

        if (info.frameRateNum > 0) {
            a->frameRate = Rational{info.frameRateNum, info.frameRateDen};
            // 秒数 = frameCount * src_den / src_num
            double sec = double(info.durationFrames) * info.frameRateDen / info.frameRateNum;
            a->durationFrames = qRound64(sec * double(projTimebase.den) / projTimebase.num);
        } else if (info.hasAudio && info.audioSampleRate > 0) {
            double sec = double(info.audioDurationFrames) / info.audioSampleRate;
            a->durationFrames = qRound64(sec * double(projTimebase.den) / projTimebase.num);
        }
    }

    // 5. ライブラリの「いま開いているフォルダ」へは QML 側が入れる (1.7.5)。
    //    ここでは既定 (ルート直下) のままにしておく。
    qInfo() << "[registerAsset] registered id:" << a->id.toString(QUuid::WithoutBraces)
            << "kind:" << int(a->kind)
            << "durationFrames:" << a->durationFrames;
    app::LibraryStore::instance().refreshMedia();
    return a->id.toString(QUuid::WithoutBraces);
}

void ProjectController::assignAssetToFolder(const QString& assetId, const QString& folderId)
{
    if (!project_ || assetId.isEmpty())
        return;
    app::LibraryStore::instance().assignAssetToFolder(QUuid(assetId), QUuid(folderId));
    project_->markModified();
}

} // namespace yave
