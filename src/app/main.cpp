#include "../audio/AudioRenderEngine.h"
#include "../core/Project.h"
#include "../i18n/LanguageManager.h"
#include "../plugin/PluginManager.h"
#include "../render/RhiContext.h"
#include "../util/Log.h"

#include "controllers/AiController.h"
#include "controllers/EditController.h"
#include "controllers/PlaybackController.h"
#include "controllers/PluginController.h"
#include "controllers/ProjectController.h"
#include "models/TrackListModel.h"
#include "models/AssetListModel.h"
#include "library/LibraryStore.h"
#include "items/ThumbnailImageProvider.h"
#include "items/PreviewItem.h"
#include "../core/Timeline.h"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QFile>
#include <QTextStream>
#include <QQmlError>
#include <fstream>
#include <string>

void write_log(const std::string& msg) {
    std::ofstream f("d:/ws/YetAnotherVideoEditor/app_debug.log", std::ios::app);
    if (f.is_open()) {
        f << msg << std::endl;
    }
}

int main(int argc, char** argv)
{
    write_log("main started");
    // QWidget 系 (PluginWindow) を使うので QApplication を使う。
    // QGuiApplication では QWidget が動かない。
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("YAVE"));
    QCoreApplication::setApplicationName(QStringLiteral("YetAnotherVideoEditor"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    yave::diag::registerLoggingCategories();

    // ★ QML エンジンを作る前に翻訳をロードする (12.6.3)。
    //   後からだと、初回の qsTr() 評価が未翻訳のまま固定される要素が出る。
    yave::LanguageManager::instance().initialize();

    yave::render::RhiContext::selectBackend();
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    // プラグイン走査をバックグラウンドで開始 (1.5 起動シーケンス)
    yave::plugin::PluginManager::instance().scanAsync();
    yave::audio::AudioRenderEngine::instance().openDevice();

    yave::ProjectController projectController;
    yave::EditController editController;
    editController.setProject(projectController.project());

    yave::AiController aiController;
    aiController.attachProject(projectController.project());

    auto& playback = yave::PlaybackController::instance();
    playback.attachProject(projectController.project());

    yave::TrackListModel trackListModel;
    trackListModel.setProject(projectController.project());

    yave::AssetListModel assetListModel;
    assetListModel.setProject(projectController.project());

    QQmlApplicationEngine engine;

    // ライブラリ (1.7.5): メディアカテゴリの供給元と、サムネイル画像プロバイダ。
    yave::app::LibraryStore::instance().setProject(projectController.project());
    engine.addImageProvider(QStringLiteral("yave-thumb"),
                            new yave::app::ThumbnailImageProvider);

    auto updateProjectReferences = [&]() {
        auto* proj = projectController.project();
        editController.setProject(proj);
        aiController.attachProject(proj);
        playback.attachProject(proj);
        trackListModel.setProject(proj);
        assetListModel.setProject(proj);
        yave::app::LibraryStore::instance().setProject(proj);

        if (!engine.rootObjects().isEmpty()) {
            auto* rootObj = engine.rootObjects().first();
            auto* previewItem = rootObj->findChild<yave::PreviewItem*>(QStringLiteral("preview"));
            if (previewItem && proj) {
                previewItem->attachTimeline(proj->timeline());
            }
        }
    };

    QObject::connect(&projectController, &yave::ProjectController::projectOpened, &app, updateProjectReferences);
    QObject::connect(&projectController, &yave::ProjectController::projectClosed, &app, updateProjectReferences);

    // QMLのエラー警告をキャッチしてファイルに保存するデバッグコード
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, [](const QList<QQmlError>& warnings) {
        write_log("QML WARNING RECEIVED:");
        for (const auto& warning : warnings) {
            write_log(" - " + warning.toString().toStdString());
        }
    });

    // retranslate() を呼ぶためにエンジンを登録する
    yave::LanguageManager::instance().setQmlEngine(&engine);

    engine.rootContext()->setContextProperty(QStringLiteral("projectController"),
                                             &projectController);
    engine.rootContext()->setContextProperty(QStringLiteral("editController"),
                                             &editController);
    engine.rootContext()->setContextProperty(QStringLiteral("aiController"),
                                             &aiController);
    engine.rootContext()->setContextProperty(QStringLiteral("playbackController"),
                                             &playback);
    engine.rootContext()->setContextProperty(QStringLiteral("trackListModel"),
                                             &trackListModel);
    engine.rootContext()->setContextProperty(QStringLiteral("assetListModel"),
                                             &assetListModel);

    const QUrl url(QStringLiteral("qrc:/qt/qml/Yave/qml/MainWindow.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] {
                         write_log("QQmlApplicationEngine::objectCreationFailed triggered");
                         QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);
    write_log("Calling engine.load for MainWindow.qml...");
    engine.load(url);
    write_log("engine.load finished. Root objects count: " + std::to_string(engine.rootObjects().size()));
    if (engine.rootObjects().isEmpty()) {
        write_log("rootObjects is empty! Returning -1");
        return -1;
    }

    // Find the PreviewItem and attach the timeline to it in C++
    auto* rootObj = engine.rootObjects().first();
    auto* preview = rootObj->findChild<yave::PreviewItem*>(QStringLiteral("preview"));
    if (preview) {
        preview->attachTimeline(projectController.project()->timeline());
    }


    // コマンドライン引数にプロジェクトパスがあれば開く
    if (argc > 1)
        projectController.open(QString::fromLocal8Bit(argv[1]));

    write_log("Calling app.exec()...");
    const int rc = app.exec();
    write_log("app.exec() returned: " + std::to_string(rc));

    // シャットダウン順序は 1.6 を厳守する
    // 1. AudioRenderEngine を先に止める (RT スレッド最優先)
    yave::audio::AudioRenderEngine::instance().closeDevice();
    // 6. PluginManager::unloadAll … VST3 -> terminate(), AviUtl -> FreeLibrary
    yave::plugin::PluginManager::instance().unloadAll();
    return rc;
}
