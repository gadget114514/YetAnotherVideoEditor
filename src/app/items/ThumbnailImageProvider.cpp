#include "ThumbnailImageProvider.h"

#include "../library/LibraryStore.h"

#include "../../core/AssetLibrary.h"
#include "../../core/Project.h"
#include "../../media/VideoDecoder.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QImage>
#include <QQuickImageResponse>
#include <QRunnable>
#include <QStandardPaths>

namespace yave::app {

namespace {

constexpr int kThumbWidth = 320;

/// パス + 更新時刻からキャッシュファイル名を作る。
/// 素材を差し替えたのに古い絵が残る、を防ぐため mtime を混ぜる。
QString cacheFileFor(const QString& absolutePath)
{
    const QFileInfo fi(absolutePath);
    const QString key = absolutePath + QLatin1Char('|')
                        + QString::number(fi.lastModified().toSecsSinceEpoch())
                        + QLatin1Char('|') + QString::number(fi.size());
    const QByteArray hash =
        QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex().left(24);
    return ThumbnailImageProvider::cacheDir() + QLatin1Char('/')
           + QString::fromLatin1(hash) + QStringLiteral(".jpg");
}

/// 動画から 1 枚取り出す。
/// **先頭ではなく 1 秒地点**を優先する: 先頭が黒 / フェードインの素材が多く、
/// 一覧に真っ黒なタイルが並ぶと見分けがつかないため (1.7.5)。
QImage decodeVideoThumbnail(const QString& absolutePath)
{
    media::VideoDecoder decoder;
    media::VideoDecoder::OpenParams params;
    params.filePath     = absolutePath;
    params.allowHwAccel = false;      ///< サムネイル 1 枚に HW 初期化は割に合わない
    params.threadCount  = 1;

    QString error;
    if (!decoder.open(params, &error))
        return {};

    const media::MediaInfo info = decoder.info();
    const double fps = (info.frameRateNum > 0 && info.frameRateDen > 0)
                           ? double(info.frameRateNum) / double(info.frameRateDen)
                           : 30.0;
    const int64_t target = int64_t(fps);   ///< 約 1 秒地点
    if (target > 0)
        decoder.seekToSourceFrame(target);

    QByteArray pixels;
    QSize      size;
    int64_t    frameIndex = 0;
    if (!decoder.decodeNext(&frameIndex, &pixels, &size)) {
        // 1 秒地点が取れない短い素材は先頭へ落とす
        decoder.seekToSourceFrame(0);
        if (!decoder.decodeNext(&frameIndex, &pixels, &size))
            return {};
    }
    if (pixels.isEmpty() || !size.isValid())
        return {};

    const QImage img(reinterpret_cast<const uchar*>(pixels.constData()),
                     size.width(), size.height(), size.width() * 4,
                     QImage::Format_RGBA8888_Premultiplied);
    return img.copy().convertToFormat(QImage::Format_RGB32);
}

QImage buildThumbnail(const QString& absolutePath, const QString& kind)
{
    QImage img;
    if (kind == QLatin1String("image")) {
        img.load(absolutePath);
    } else {
        img = decodeVideoThumbnail(absolutePath);
    }
    if (img.isNull())
        return {};
    return img.scaledToWidth(kThumbWidth, Qt::SmoothTransformation);
}

class ThumbnailResponse : public QQuickImageResponse, public QRunnable
{
public:
    ThumbnailResponse(QString absolutePath, QString kind)
        : path_(std::move(absolutePath)), kind_(std::move(kind))
    {
        setAutoDelete(false);
    }

    QQuickTextureFactory* textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(image_);
    }

    void run() override
    {
        if (path_.isEmpty()) {
            emit finished();
            return;
        }

        const QString cacheFile = cacheFileFor(path_);
        if (QFileInfo::exists(cacheFile) && image_.load(cacheFile)) {
            emit finished();
            return;
        }

        image_ = buildThumbnail(path_, kind_);
        if (!image_.isNull())
            image_.save(cacheFile, "JPG", 85);

        emit finished();
    }

private:
    QString path_;
    QString kind_;
    QImage  image_;
};

} // anonymous namespace

ThumbnailImageProvider::ThumbnailImageProvider()
{
    // 2 本で足りる。一覧のスクロールで大量に要求が来ても、
    // デコードで CPU を占有して再生を邪魔しないことのほうが大事。
    pool_.setMaxThreadCount(2);
}

ThumbnailImageProvider::~ThumbnailImageProvider()
{
    pool_.waitForDone();
}

QString ThumbnailImageProvider::cacheDir()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                        + QStringLiteral("/thumbs");
    QDir().mkpath(dir);
    return dir;
}

QQuickImageResponse* ThumbnailImageProvider::requestImageResponse(const QString& id,
                                                                  const QSize&)
{
    // アセット ID -> 絶対パスの解決は **UI スレッドで**行う。
    // Project / AssetLibrary はワーカースレッドから触らない。
    QString path;
    QString kind = QStringLiteral("video");

    if (Project* project = LibraryStore::instance().project()) {
        if (AssetLibrary* lib = project->assets()) {
            if (const Asset* a = lib->asset(QUuid(id))) {
                path = a->resolvedAbsolutePath;
                if (a->kind == Asset::Kind::Image)
                    kind = QStringLiteral("image");
            }
        }
    }

    auto* response = new ThumbnailResponse(path, kind);
    pool_.start(response);
    // QQuickAsyncImageProvider は finished() の後にレスポンスを delete する。
    // QRunnable の autoDelete は切ってあるので二重解放にはならない。
    return response;
}

} // namespace yave::app
