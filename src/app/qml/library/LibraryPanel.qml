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
        anchors.fill: parent
        enabled: root.acceptsFileDrop

        onDropped: function(drop) {
            if (!drop.hasUrls)
                return
            for (var i = 0; i < drop.urls.length; ++i) {
                var assetId = projectController.registerAsset(drop.urls[i].toString())
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
