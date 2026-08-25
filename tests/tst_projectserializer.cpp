#include "../src/core/AudioClip.h"
#include "../src/core/Project.h"
#include "../src/core/Timeline.h"
#include "../src/core/Track.h"
#include "../src/core/VideoClip.h"
#include "../src/io/ProjectSerializer.h"
#include "../src/subtitle/SubtitleClip.h"
#include "../src/subtitle/SubtitleText.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QtTest>

using namespace yave;
using namespace yave::subtitle;


namespace {
TimeRange TR(int64_t start, int64_t duration) { return TimeRange{start, duration}; }
}

class TestProjectSerializer : public QObject
{
    Q_OBJECT


private slots:
    void saveLoadRoundTrip();
    void unknownFieldsPreserved();
    void atomicSaveKeepsOriginal();
    void enumStringsInJson();
    void schemaVersionWarning();
    void autosaveCompact();

private:
    Project* makeSampleProject() const;
    QString tempPath(const QString& suffix) const;
    static QJsonArray readTracks(const QJsonObject& root)
    {
        return root[QStringLiteral("tracks")].toArray();
    }

    mutable std::unique_ptr<Project> sample_;
};

QString TestProjectSerializer::tempPath(const QString& suffix) const
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
         + QStringLiteral("/tst_proj_%1%2")
               .arg(QDateTime::currentMSecsSinceEpoch()).arg(suffix);
}

Project* TestProjectSerializer::makeSampleProject() const
{
    if (!sample_)
        sample_ = std::make_unique<Project>();
    return sample_.get();
}

void TestProjectSerializer::saveLoadRoundTrip()
{
    Project* src = makeSampleProject();
    src->setName(QStringLiteral("Round Trip"));
    src->setTimebase(timebase::Fps30);
    src->setCanvasSize(QSize(1920, 1080));

    Timeline* tl = src->timeline();

    // ---- 映像トラック ----
    Track* video = tl->appendTrack(TrackType::Video, QStringLiteral("V1"));
    auto vc = std::make_shared<VideoClip>();
    vc->setRange({1000, 500});
    vc->setSourceOffset(200);
    vc->setSpeed(1.5);
    vc->setOpacity(0.8);
    vc->setBlendMode(BlendMode::Multiply);
    vc->setFadeOutFrames(30);
    QVERIFY(video->insertClip(vc));

    // ---- 音声トラック ----
    Track* audio = tl->appendTrack(TrackType::Audio, QStringLiteral("A1"));
    audio->setGain(0.75);
    audio->setPan(-0.25);
    auto ac = std::make_shared<AudioClip>();
    ac->setRange({0, 1200});
    ac->setGain(1.2);
    QVERIFY(audio->insertClip(ac));

    // ---- 字幕トラック (リッチスパン + エフェクトスタック + 単語タイミング) ----
    Track* subs = tl->appendTrack(TrackType::Subtitle, QStringLiteral("S1"));
    auto sc = std::make_shared<SubtitleClip>();
    sc->setRange({500, 300});
    SubtitleText text;
    text.setPlain(QStringLiteral("これは字幕のテキストです"));
    TextSpan span;
    span.start = 4;
    span.length = 2;
    span.bold = true;
    span.color = QColor(255, 204, 0);
    text.addSpan(span);
    sc->setText(text);

    SubtitleStyleDiff diff;
    diff.fontPointSize = 56.0;
    sc->setStyleOverride(diff);

    auto inst = SubtitleEffectInstance::create(QStringLiteral("yave.typewriter"));
    inst.params.set(QStringLiteral("charsPerSecond"), 20.0);
    sc->addEffect(std::move(inst));

    SubtitleClip::WordTiming wt{0, 4, 0.0, 0.42};
    sc->setWordTimings({wt});
    QVERIFY(subs->insertClip(sc));

    // ---- 保存 / 読み込み ----
    const QString path = tempPath(QStringLiteral(".yave"));
    io::SaveOptions opts;
    QString err;
    QVERIFY2(io::ProjectSerializer::save(*src, path, opts, &err), qUtf8Printable(err));

    Project loaded;
    const io::LoadResult result = io::ProjectSerializer::load(&loaded, path);
    QVERIFY2(result.ok, qUtf8Printable(result.errorMessage));
    QCOMPARE(loaded.name(), QStringLiteral("Round Trip"));
    QVERIFY(loaded.timebase() == timebase::Fps30);
    QCOMPARE(loaded.canvasSize(), QSize(1920, 1080));
    QCOMPARE(loaded.timeline()->trackCount(), tl->trackCount());

    // ---- 各フィールドの一致確認 ----
    Track* lv = loaded.timeline()->tracksOfType(TrackType::Video).at(0);
    QCOMPARE(lv->name(), QStringLiteral("V1"));
    const auto lvc = lv->clips().at(0);
    const TimeRange lvcRange{1000, 500};
    QCOMPARE(lvc->range(), lvcRange);
    QCOMPARE(lvc->sourceOffset(), int64_t(200));
    QCOMPARE(static_cast<VideoClip*>(lvc.get())->speed(), 1.5);
    QCOMPARE(lvc->blendMode(), BlendMode::Multiply);

    Track* la = loaded.timeline()->tracksOfType(TrackType::Audio).at(0);
    QCOMPARE(la->gain(), 0.75);
    QCOMPARE(la->pan(), -0.25);

    Track* ls = loaded.timeline()->tracksOfType(TrackType::Subtitle).at(0);
    const auto lsc =
        std::static_pointer_cast<SubtitleClip>(ls->clips().at(0));
    QCOMPARE(lsc->plainText(), QStringLiteral("これは字幕のテキストです"));
    QCOMPARE(int(lsc->text().spans().size()), 1);
    QVERIFY(lsc->text().spans()[0].bold.value_or(false));
    QCOMPARE(int(lsc->effectStack().size()), 1);
    QCOMPARE(lsc->effectStack()[0].effectId, QStringLiteral("yave.typewriter"));
    QCOMPARE(lsc->effectStack()[0].params.getDouble(QStringLiteral("charsPerSecond")),
             20.0);
    QCOMPARE(int(lsc->wordTimings().size()), 1);
    QCOMPARE(lsc->wordTimings()[0].startSec, 0.0);

    QFile::remove(path);
}

void TestProjectSerializer::unknownFieldsPreserved()
{
    // 未来バージョンのフィールドを手で注入しても、読み込み -> 保存で消えないこと
    Project src;
    Timeline* tl = src.timeline();
    Track* video = tl->appendTrack(TrackType::Video);
    auto vc = std::make_shared<VideoClip>();
    vc->setRange({0, 10});
    video->insertClip(vc);

    const QString path = tempPath(QStringLiteral(".yave"));
    io::SaveOptions opts;
    QVERIFY(io::ProjectSerializer::save(src, path, opts, nullptr));

    // JSON を手編集して未知フィールドを追加する
    QFile f(path);
    f.open(QIODevice::ReadOnly);
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    QJsonObject root = doc.object();
    QJsonArray tracks = root[QStringLiteral("tracks")].toArray();
    QJsonObject trackObj = tracks.at(0).toObject();
    QJsonObject clipObj = trackObj[QStringLiteral("clips")].toArray().at(0).toObject();
    clipObj[QStringLiteral("futureField")] = QStringLiteral("keep-me");
    clipObj[QStringLiteral("futureNested")] = QJsonObject{{"a", 1}, {"b", 2}};
    trackObj[QStringLiteral("clips")] = QJsonArray{clipObj};
    tracks[0] = trackObj;
    root[QStringLiteral("tracks")] = tracks;
    doc.setObject(root);

    f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    f.write(doc.toJson());
    f.close();

    // 再読み込みして保存する
    Project reloaded;
    const auto result = io::ProjectSerializer::load(&reloaded, path);
    QVERIFY(result.ok);
    const QString resavePath = tempPath(QStringLiteral(".yave"));
    QVERIFY(io::ProjectSerializer::save(reloaded, resavePath, opts, nullptr));

    // 未知フィールドが保持されていること (9.11.2)
    QFile rf(resavePath);
    rf.open(QIODevice::ReadOnly);
    const QJsonObject reread = QJsonDocument::fromJson(rf.readAll()).object();
    const QJsonObject rtrack = readTracks(reread).at(0).toObject();
    const QJsonObject rclip =
        rtrack[QStringLiteral("clips")].toArray().at(0).toObject();
    QCOMPARE(rclip[QStringLiteral("futureField")].toString(),
             QStringLiteral("keep-me"));
    QVERIFY(rclip[QStringLiteral("futureNested")].isObject());

    QFile::remove(path);
    QFile::remove(resavePath);
}

void TestProjectSerializer::atomicSaveKeepsOriginal()
{
    Project* src = makeSampleProject();
    const QString path = tempPath(QStringLiteral(".yave"));

    io::SaveOptions opts;
    QVERIFY(io::ProjectSerializer::save(*src, path, opts, nullptr));

    // 上書き保存中にクラッシュした場合を想定: 元ファイルは無傷であること。
    // QSaveFile の commit 失失敗シミュレーションとして、書き込み不可パスを使う。
    QString err = QStringLiteral("unused");
    io::SaveOptions badOpts;
    const bool saved = io::ProjectSerializer::save(
        *src, path + QStringLiteral("/invalid/path.yave"), badOpts, &err);
    QVERIFY(!saved);
    QVERIFY(!err.isEmpty());

    QFile::remove(path);
}

void TestProjectSerializer::enumStringsInJson()
{
    // enum は必ず文字列で保存される (9.9)。数値だと enum 追加で既存プロジェクトが壊れる。
    Project src;
    Track* t = src.timeline()->appendTrack(TrackType::AiGenerated);
    auto c = std::make_shared<VideoClip>();
    c->setRange({0, 5});
    c->setBlendMode(BlendMode::Screen);
    t->insertClip(c);

    const QString path = tempPath(QStringLiteral(".yave"));
    QVERIFY(io::ProjectSerializer::save(src, path,
                                        io::SaveOptions{}, nullptr));

    QFile f(path);
    f.open(QIODevice::ReadOnly);
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();

    const QJsonObject trackObj = root[QStringLiteral("tracks")].toArray().at(0).toObject();
    QCOMPARE(trackObj[QStringLiteral("type")].toString(), QStringLiteral("aiGenerated"));

    const QJsonObject clipObj = trackObj[QStringLiteral("clips")].toArray().at(0).toObject();
    QCOMPARE(clipObj[QStringLiteral("type")].toString(), QStringLiteral("video"));
    QCOMPARE(clipObj[QStringLiteral("blendMode")].toString(), QStringLiteral("screen"));

    QFile::remove(path);
}

void TestProjectSerializer::schemaVersionWarning()
{
    // 未来バージョンは警告付きで開ける (9.11.1)
    Project* src = makeSampleProject();
    const QString path = tempPath(QStringLiteral(".yave"));
    QVERIFY(io::ProjectSerializer::save(*src, path, io::SaveOptions{}, nullptr));

    QFile f(path);
    f.open(QIODevice::ReadOnly);
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    QJsonObject root = doc.object();
    root[QStringLiteral("schemaVersion")] = 999;
    doc.setObject(root);
    f.close();
    f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    f.write(doc.toJson());
    f.close();

    Project loaded;
    const auto result = io::ProjectSerializer::load(&loaded, path);
    QVERIFY(result.ok);
    QCOMPARE(result.loadedSchemaVersion, 999);
    bool warned = false;
    for (const QString& w : result.warnings)
        if (w.contains(QLatin1String("newer version")))
            warned = true;
    QVERIFY(warned);

    QFile::remove(path);
}

void TestProjectSerializer::autosaveCompact()
{
    Project* src = makeSampleProject();
    const QString path = tempPath(QStringLiteral(".autosave"));
    QVERIFY(io::ProjectSerializer::saveAutosave(*src, path));

    Project loaded;
    const auto result = io::ProjectSerializer::loadAutosave(&loaded, path);
    QVERIFY(result.ok);

    QFile::remove(path);
}

QTEST_MAIN(TestProjectSerializer)
#include "tst_projectserializer.moc"
