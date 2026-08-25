import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

// 右のアイテムビュー。viewMode で GridView / ListView を切り替える (1.7.5)。
Rectangle {
    id: root

    property var    itemsModel: null
    property string viewMode: "grid"
    property int    iconSize: 64

    property int currentRow: -1

    color: "#1c1c1c"
    border.color: "#1a1a1a"

    readonly property bool gridMode: viewMode === "grid"
    readonly property int  rowHeight: viewMode === "list" ? 18
                                    : (viewMode === "details" ? 22
                                                              : Math.max(20, iconSize + 4))

    // Ctrl + ホイールでアイコンサイズを変える (エクスプローラと同じ操作)
    WheelHandler {
        acceptedModifiers: Qt.ControlModifier
        onWheel: function(event) {
            var delta = event.angleDelta.y > 0 ? 8 : -8
            root.iconSize = Math.max(32, Math.min(160, root.iconSize + delta))
        }
    }

    // ---- 詳細ビューのヘッダ行 ----
    Rectangle {
        id: header
        visible: root.viewMode === "details"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: visible ? 20 : 0
        color: "#252525"
        border.color: "#1a1a1a"

        Row {
            anchors.fill: parent
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            spacing: 6

            Text {
                width: parent.width - 132
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Name"); color: "#888"; font.pixelSize: 10
            }
            Text {
                width: 60
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Type"); color: "#888"; font.pixelSize: 10
            }
            Text {
                width: 60
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Length"); color: "#888"; font.pixelSize: 10
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    // ---- グリッド (大アイコン) ----
    GridView {
        id: grid
        visible: root.gridMode
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        model: root.gridMode ? root.itemsModel : null
        cellWidth: root.iconSize + 24
        cellHeight: root.iconSize + 30
        boundsBehavior: Flickable.StopAtBounds

        delegate: LibraryItemDelegate {
            required property int index
            required property string name
            required property string kind
            required property string iconKey
            required property string iconSource
            required property string detailText
            required property string dragPayload
            required property string assetId
            required property bool   missing

            width: grid.cellWidth
            height: grid.cellHeight
            viewMode: root.viewMode
            iconSize: root.iconSize
            selected: root.currentRow === index

            itemName: name
            itemKind: kind
            itemIconKey: iconKey
            itemIconSource: iconSource
            itemDetail: detailText
            itemPayload: dragPayload
            itemAssetId: assetId
            itemMissing: missing

            onClicked: root.currentRow = index
            onContextMenuRequested: { root.currentRow = index; itemMenu.popup() }
        }
    }

    // ---- 行リスト (一覧 / 小アイコン / 詳細) ----
    ListView {
        id: list
        visible: !root.gridMode
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        model: root.gridMode ? null : root.itemsModel
        boundsBehavior: Flickable.StopAtBounds

        delegate: LibraryItemDelegate {
            required property int index
            required property string name
            required property string kind
            required property string iconKey
            required property string iconSource
            required property string detailText
            required property string dragPayload
            required property string assetId
            required property bool   missing

            width: list.width
            height: root.rowHeight
            viewMode: root.viewMode
            iconSize: root.iconSize
            selected: root.currentRow === index

            itemName: name
            itemKind: kind
            itemIconKey: iconKey
            itemIconSource: iconSource
            itemDetail: detailText
            itemPayload: dragPayload
            itemAssetId: assetId
            itemMissing: missing

            onClicked: root.currentRow = index
            onContextMenuRequested: { root.currentRow = index; itemMenu.popup() }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: !root.itemsModel || root.itemsModel.count === 0
        text: qsTr("Empty")
        color: "#555"
        font.pixelSize: 11
    }

    // ---- アイテムの右クリックメニュー (1.7.5 のアイコン差し替え) ----
    Menu {
        id: itemMenu
        MenuItem {
            text: qsTr("Set Icon...")
            onTriggered: iconDialog.open()
        }
        MenuItem {
            text: qsTr("Reset Icon")
            onTriggered: if (root.itemsModel) root.itemsModel.clearIcon(root.currentRow)
        }
    }

    FileDialog {
        id: iconDialog
        title: qsTr("Choose an icon image")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.svg *.bmp)")]
        onAccepted: if (root.itemsModel) root.itemsModel.setIcon(root.currentRow, selectedFile)
    }
}
