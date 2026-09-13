import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Yave

// ライブラリパネル (1.7.5)。
// mediaLibrary と effectLibrary は、担当カテゴリだけを変えた同じ実装を使う。
Rectangle {
    id: root

    // 担当カテゴリ。例: ["media"] / ["transition","title","subtitle","filter","effect"]
    property var categories: ["media"]
    property string title: qsTr("Library")

    // 設定の保存キーを 2 枚で分けるための識別子 (1.7.5: モードとサイズはパネルごと)
    property string panelId: "library"

    // OS からファイルをドロップして取り込めるか (メディア側のみ true)
    property bool acceptsFileDrop: false

    color: "#202020"
    border.color: "#1a1a1a"
    border.width: 1

    LibraryTreeModel {
        id: treeModel
        categories: root.categories
    }

    LibraryItemsModel {
        id: itemsModel
        filterText: toolbar.filterText
    }

    Settings {
        id: viewSettings
        category: "ui/library/" + root.panelId
        property alias viewMode: toolbar.viewMode
        property alias iconSize: toolbar.iconSize
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- タブ風ヘッダー (既存パネルと同じ見た目) ----
        Rectangle {
            Layout.fillWidth: true
            height: 22
            color: "#1a1a1a"

            Rectangle {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                width: Math.max(80, headerLabel.width + 20)
                height: 21
                color: "#202020"
                border.color: "#1a1a1a"
                border.width: 1

                Label {
                    id: headerLabel
                    anchors.centerIn: parent
                    text: root.title
                    color: "#ffffff"
                    font.pixelSize: 11
                    font.bold: true
                }
            }
        }

        LibraryToolbar {
            id: toolbar
            Layout.fillWidth: true
            canCreateFolder: true
            onNewFolderRequested: tree.createFolderUnderSelection()
        }

        // ---- 左: フォルダツリー / 右: アイテム ----
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            LibraryTree {
                id: tree
                SplitView.preferredWidth: 130
                SplitView.minimumWidth: 90
                treeModel: treeModel

                onSelectionChanged: function(category, folderId) {
                    itemsModel.setCurrentFolder(category, folderId)
                    itemView.currentRow = -1
                }
            }

            LibraryItemView {
                id: itemView
                SplitView.fillWidth: true
                itemsModel: itemsModel
                viewMode: toolbar.viewMode
                iconSize: toolbar.iconSize
                onIconSizeChanged: toolbar.iconSize = iconSize
            }
        }
    }

    // ---- OS からのファイルドロップ (メディア側のみ) ----
    DropArea {
        id: fileDropArea
        anchors.fill: parent
        enabled: root.acceptsFileDrop

        // 外部ファイルドロップだけを受ける。keys が無いと内部ドラッグ
        // (ライブラリアイテムの移動) まで奪ってしまうため。
        keys: ["text/uri-list"]

        Rectangle {
            anchors.fill: parent
            color: fileDropArea.containsDrag ? "#15ffffff" : "transparent"
            z: 100
        }

        onEntered: (drag) => {
            console.log("[libraryFileDrop] ENTERED hasUrls=" + drag.hasUrls
                        + " hasText=" + drag.hasText)
        }

        onDropped: function(drop) {
            let urls = drop.urls
            // QML の DragEvent は外部ドロップの urls を空で渡すことがある
            // (Qt の既知挙動)。その場合はウィンドウレベルで捕捉した
            // FileDropFilter の URL を使う。
            if ((!drop.hasUrls || !urls || urls.length === 0)
                    && fileDropFilter && fileDropFilter.lastUrls.length > 0)
                urls = fileDropFilter.lastUrls
            console.log("[libraryFileDrop] DROPPED urls=" + (urls ? urls.length : 0))
            if (!urls || urls.length === 0) {
                console.log("[libraryFileDrop] no urls, ignored")
                return
            }

            for (var i = 0; i < urls.length; ++i) {
                const urlStr = urls[i].toString()
                // SRT / VTT は字幕として取り込む (メディアアセットではない)
                if (/\.(srt|vtt)$/i.test(urlStr)) {
                    console.log("[libraryFileDrop] SRT url=" + urlStr)
                    editController.importSrt(urlStr, {
                        overlapPolicy: 0,
                        targetTrackIndex: -1,
                        fadeInFrames: 8,
                        fadeOutFrames: 8
                    })
                    continue
                }
                const assetId = projectController.registerAsset(urlStr)
                console.log("[libraryFileDrop] registerAsset url=" + urlStr
                            + " -> assetId='" + assetId + "'")
                // 取り込んだ素材は、いま開いているフォルダへ入れる
                if (assetId && tree.selectedCategory === 0)
                    projectController.assignAssetToFolder(assetId, tree.selectedFolderId)
            }
            drop.acceptProposedAction()
        }
    }

    Component.onCompleted: {
        // 既定では最初のカテゴリのルートを選んでおく
        var first = treeModel.index(0, 0)
        if (first && first.valid) {
            itemsModel.setCurrentFolder(treeModel.categoryAt(first), treeModel.folderIdAt(first))
        }
    }
}
