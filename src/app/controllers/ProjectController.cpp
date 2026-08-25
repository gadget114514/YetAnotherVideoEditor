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
#include <fstream>

namespace yave {

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
    auto loaded = std::make_unique<Project>();
    const io::LoadResult result = io::ProjectSerializer::load(loaded.get(), path);
    if (!result.ok) {
        qCWarning(lcApp) << "Failed to open project:" << result.errorMessage;
        return false;
    }

    project_ = std::move(loaded);
    projectPath_ = path;
    emit projectOpened(path);
    return true;
}

bool ProjectController::save()
{
    if (projectPath_.isEmpty())
        return false;
    io::SaveOptions opts;
    QString err;
    if (!io::ProjectSerializer::save(*project_, projectPath_, opts, &err)) {
        qCWarning(lcApp) << "Save failed:" << err;
        return false;
    }
    project_->clearModified();
    project_->undoStack()->setClean();
    emit projectSaved(projectPath_);
    return true;
}

bool ProjectController::saveAs(const QString& path)
{
    projectPath_ = path;
    return save();
}

bool ProjectController::restoreAutosave()
{
    const QString autosave = projectPath_ + QStringLiteral(".autosave");
    if (!QFileInfo::exists(autosave))
        return false;
    return open(autosave);
}

bool ProjectController::isModified() const
{
    return project_ && !project_->undoStack()->isClean();
}

QString ProjectController::registerAsset(const QString& absolutePathOrUrl)
{
    QString absolutePath = absolutePathOrUrl;
    if (absolutePathOrUrl.contains(QLatin1String("://"))) {
        QUrl url(absolutePathOrUrl);
        if (url.isLocalFile()) {
            absolutePath = url.toLocalFile();
        }
    }

    {
        std::ofstream f("d:/ws/YetAnotherVideoEditor/app_debug.log", std::ios::app);
        if (f.is_open()) {
            f << "[registerAsset] resolved path: " << absolutePath.toStdString()
              << " (from: " << absolutePathOrUrl.toStdString() << ")" << std::endl;
        }
    }

    if (!project_)
        return {};

    // 1. メディアの情報をプローブする
    media::MediaInfo info = media::MediaProbe::probe(absolutePath);

    {
        std::ofstream f("d:/ws/YetAnotherVideoEditor/app_debug.log", std::ios::app);
        if (f.is_open()) {
            f << "[registerAsset] probe ok: " << info.ok
              << ", hasVideo: " << info.hasVideo
              << ", hasAudio: " << info.hasAudio
              << ", duration: " << info.durationFrames
              << ", audioDuration: " << info.audioDurationFrames
              << ", error: " << info.error.toStdString() << std::endl;
        }
    }
    
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
    if (!a)
        return {};

    // 4. メディア情報・プロジェクトタイムベース換算の反映
    if (info.ok) {
        a->resolution = info.resolution;
        a->hasAudio = info.hasAudio;
        Rational projTimebase = project_->timebase();

        if (info.frameRateNum > 0) {
            a->frameRate = Rational{info.frameRateNum, info.frameRateDen};
            // 秒数 = frameCount * src_den / src_num
            double sec = double(info.durationFrames) * info.frameRateDen / info.frameRateNum;
            a->durationFrames = qRound64(sec * double(projTimebase.num) / projTimebase.den);
        } else if (info.hasAudio && info.audioSampleRate > 0) {
            double sec = double(info.audioDurationFrames) / info.audioSampleRate;
            a->durationFrames = qRound64(sec * double(projTimebase.num) / projTimebase.den);
        }
    }

    // 5. ライブラリの「いま開いているフォルダ」へは QML 側が入れる (1.7.5)。
    //    ここでは既定 (ルート直下) のままにしておく。
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
