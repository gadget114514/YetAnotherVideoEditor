qt_add_executable(yave_app
    main.cpp
    controllers/ProjectController.h
    controllers/ProjectController.cpp
    controllers/EditController.h
    controllers/EditController.cpp
    controllers/PlaybackController.h
    controllers/PlaybackController.cpp
    controllers/AiController.h
    controllers/AiController.cpp
    controllers/PluginController.h
    controllers/PluginController.cpp

    models/TrackListModel.h
    models/TrackListModel.cpp
    models/AssetListModel.h
    models/AssetListModel.cpp

    library/LibraryStore.h
    library/LibraryStore.cpp

    items/PreviewItem.h
    items/PreviewItem.cpp
    items/ThumbnailImageProvider.h
    items/ThumbnailImageProvider.cpp
)

qt_add_qml_module(yave_app
    URI Yave
    VERSION 1.0
    QML_FILES
        qml/MainWindow.qml
        qml/timeline/TimelineView.qml
        qml/library/LibraryPanel.qml
        qml/library/LibraryTree.qml
        qml/library/LibraryItemView.qml
        qml/library/LibraryItemDelegate.qml
        qml/library/LibraryIcon.qml
        qml/library/LibraryToolbar.qml
    RESOURCES
        qml/icons/cat_media.svg
        qml/icons/cat_transition.svg
        qml/icons/cat_title.svg
        qml/icons/cat_subtitle.svg
        qml/icons/cat_filter.svg
        qml/icons/cat_effect.svg
        qml/icons/kind_video.svg
        qml/icons/kind_audio.svg
        qml/icons/kind_image.svg
        qml/icons/kind_generated.svg
        qml/icons/folder.svg
    SOURCES
        items/PreviewItem.h
        items/PreviewItem.cpp
        models/LibraryTreeModel.h
        models/LibraryTreeModel.cpp
        models/LibraryItemsModel.h
        models/LibraryItemsModel.cpp
)

# ---------------------------------------------------------------------------
# 翻訳: 日本語 / 英語
#
#   RESOURCE_PREFIX "/i18n" により、.qm が :/i18n/yave_ja.qm として
#   実行ファイルに埋め込まれる。外部ファイルの配布が不要になる。
#
#   LUPDATE_OPTIONS:
#     -no-obsolete   : 使われなくなった文字列を .ts から削除する
#     -locations none: 行番号情報を出力しない。
# ---------------------------------------------------------------------------
set(YAVE_TS_DIR ${CMAKE_SOURCE_DIR}/i18n)
if(EXISTS ${YAVE_TS_DIR}/yave_ja.ts AND EXISTS ${YAVE_TS_DIR}/yave_en.ts)
    qt_add_translations(yave_app
        TS_FILES
            ${YAVE_TS_DIR}/yave_ja.ts
            ${YAVE_TS_DIR}/yave_en.ts
        RESOURCE_PREFIX "/i18n"
        LUPDATE_OPTIONS -no-obsolete -locations none
    )
endif()

yave_apply_common_settings(yave_app)

target_link_libraries(yave_app PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Quick Qt6::QuickControls2 Qt6::Qml
    yave_core yave_media yave_render yave_audio yave_subtitle yave_ai yave_io yave_plugin yave_i18n)

# QML の PreviewItem (QQuickRhiItem) 用に Gui のプライベートヘッダが必要
find_package(Qt6 COMPONENTS GuiPrivate QUIET)
if(TARGET Qt6::GuiPrivate)
    target_link_libraries(yave_app PRIVATE Qt6::GuiPrivate)
endif()

# qmltyperegistration が生成する #include <PreviewItem.h> を解決できるようにする
target_include_directories(yave_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/items
    ${CMAKE_CURRENT_SOURCE_DIR}/models)

set_target_properties(yave_app PROPERTIES
    WIN32_EXECUTABLE FALSE
    MACOSX_BUNDLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

yave_deploy(yave_app)
