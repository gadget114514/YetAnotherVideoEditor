import QtQuick
import QtQuick.Controls
import Yave

// 左のフォルダツリー (1.7.5)。
// 右クリック: 新規フォルダ / 名前の変更 / 削除。F2 と Delete も同じ操作。
Rectangle {
    id: root

    property var  treeModel: null
    property int  selectedCategory: -1
    property string selectedFolderId: ""

    signal selectionChanged(int category, string folderId)

    color: "#1c1c1c"
    border.color: "#1a1a1a"

    function createFolderUnderSelection() {
        if (!treeModel)
            return
        var target = tree.currentIndex
        if (!target || !target.valid)
            target = treeModel.index(0, 0)
        var created = treeModel.createFolder(target, qsTr("New Folder"))
        if (created && created.valid) {
            tree.expand(tree.rowAtIndex(target))
            tree.selectionModel.setCurrentIndex(created, 0x0002 /* Select */)
        }
    }

    TreeView {
        id: tree
        anchors.fill: parent
        anchors.margins: 2
        clip: true
        model: root.treeModel
        selectionModel: ItemSelectionModel { model: root.treeModel }

        delegate: Item {
            id: node
            required property int row
            required property int depth
            required property bool expanded
            required property bool hasChildren
            required property bool current

            implicitWidth: tree.width
            implicitHeight: 22

            readonly property var nodeIndex: tree.index(row, 0)

            Rectangle {
                anchors.fill: parent
                color: node.current ? "#3c5a80" : (hover.hovered ? "#2d2d2d" : "transparent")
                radius: 2
            }

            HoverHandler { id: hover }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 4 + node.depth * 14
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                // 展開マーク
                Text {
                    width: 10
                    text: node.hasChildren ? (node.expanded ? "▾" : "▸") : ""
                    color: "#999"
                    font.pixelSize: 10
                    anchors.verticalCenter: parent.verticalCenter

                    TapHandler {
                        onTapped: tree.toggleExpanded(node.row)
                    }
                }

                LibraryIcon {
                    width: 14
                    height: 14
                    anchors.verticalCenter: parent.verticalCenter
                    iconKey: model.iconKey
                    label: model.name
                }

                Text {
                    text: model.name
                    color: "#ddd"
                    font.pixelSize: 11
                    font.bold: model.isCategoryRoot
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !renameField.visible
                }

                TextField {
                    id: renameField
                    visible: false
                    width: 140
                    height: 18
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter

                    function begin() {
                        text = model.name
                        visible = true
                        forceActiveFocus()
                        selectAll()
                    }
                    function commit() {
                        if (!visible)
                            return
                        visible = false
                        if (text.trim().length > 0 && text !== model.name)
                            root.treeModel.renameFolder(node.nodeIndex, text.trim())
                    }
                    onAccepted: commit()
                    onActiveFocusChanged: if (!activeFocus) commit()
                    Keys.onEscapePressed: { visible = false }
                }
            }

            // アイテムをフォルダへ落として移動する (1.7.5)
            DropArea {
                anchors.fill: parent
                keys: ["yave/library-item"]

                Rectangle {
                    anchors.fill: parent
                    visible: parent.containsDrag
                    color: "#3060a0"
                    opacity: 0.35
                    radius: 2
                }

                onDropped: function(drop) {
                    var payload = drop.getDataAsString("yave/library-item")
                    if (!payload)
                        return
                    try {
                        var obj = JSON.parse(payload)
                        if (root.treeModel.dropItems(node.nodeIndex, [obj.itemId]))
                            drop.acceptProposedAction()
                    } catch (e) {
                        console.warn("library: malformed drag payload", e)
                    }
                }
            }

            TapHandler {
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onTapped: function(point, button) {
                    tree.selectionModel.setCurrentIndex(node.nodeIndex, 0x0002 /* Select */)
                    root.selectedCategory = root.treeModel.categoryAt(node.nodeIndex)
                    root.selectedFolderId = root.treeModel.folderIdAt(node.nodeIndex)
                    root.selectionChanged(root.selectedCategory, root.selectedFolderId)

                    if (button === Qt.RightButton)
                        contextMenu.popup()
                }
                onDoubleTapped: tree.toggleExpanded(node.row)
            }

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_F2 && model.canRename) {
                    renameField.begin()
                    event.accepted = true
                } else if (event.key === Qt.Key_Delete && model.canRename) {
                    root.treeModel.removeFolder(node.nodeIndex)
                    event.accepted = true
                }
            }

            Menu {
                id: contextMenu
                MenuItem {
                    text: qsTr("New Folder")
                    onTriggered: {
                        var created = root.treeModel.createFolder(node.nodeIndex, qsTr("New Folder"))
                        if (created && created.valid)
                            tree.expand(node.row)
                    }
                }
                MenuItem {
                    text: qsTr("Rename")
                    enabled: model.canRename
                    onTriggered: renameField.begin()
                }
                MenuItem {
                    text: qsTr("Delete Folder")
                    enabled: model.canRename
                    onTriggered: root.treeModel.removeFolder(node.nodeIndex)
                }
            }
        }
    }
}
