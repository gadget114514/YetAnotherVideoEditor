import QtQuick
import QtQuick.Controls

// ライブラリのアイテム 1 個 (1.7.5)。
// 4 つの表示モードで見た目だけを変え、ドラッグ元としての振る舞いは共通にする。
Item {
    id: root

    property string viewMode: "grid"       // list | smallIcons | grid | details
    property int    iconSize: 64
    property bool   selected: false

    // モデルから渡される値 (Repeater/GridView の required property 経由で入る)
    property string itemName: ""
    property string itemKind: ""
    property string itemIconKey: ""
    property string itemIconSource: ""
    property string itemDetail: ""
    property string itemPayload: ""
    property string itemAssetId: ""
    property bool   itemMissing: false

    signal clicked()
    signal doubleClicked()
    signal contextMenuRequested()

    readonly property bool iconMode: viewMode === "grid"

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: 2
        color: root.selected ? "#3c5a80"
                             : (hover.hovered ? "#2d2d2d" : "transparent")
        border.color: root.selected ? "#4a7fbf" : "transparent"
    }

    HoverHandler { id: hover }

    // ---- 大アイコン (グリッド) ----
    Column {
        anchors.centerIn: parent
        visible: root.iconMode
        spacing: 3

        LibraryIcon {
            width: root.iconSize
            height: root.iconSize
            anchors.horizontalCenter: parent.horizontalCenter
            source: root.itemIconSource
            iconKey: root.itemIconKey
            label: root.itemName
            opacity: root.itemMissing ? 0.4 : 1.0
        }

        Text {
            width: root.width - 8
            horizontalAlignment: Text.AlignHCenter
            text: root.itemName
            color: root.itemMissing ? "#c06060" : "#ddd"
            font.pixelSize: 10
            elide: Text.ElideMiddle
            maximumLineCount: 2
            wrapMode: Text.Wrap
        }
    }

    // ---- 一覧 / 小アイコン / 詳細 (行) ----
    Row {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        visible: !root.iconMode
        spacing: 6

        LibraryIcon {
            width: root.viewMode === "list" ? 0 : Math.min(root.iconSize, root.height - 4)
            height: width
            visible: width > 0
            anchors.verticalCenter: parent.verticalCenter
            source: root.itemIconSource
            iconKey: root.itemIconKey
            label: root.itemName
            opacity: root.itemMissing ? 0.4 : 1.0
        }

        Text {
            width: root.viewMode === "details"
                       ? root.width - 140
                       : root.width - (root.viewMode === "list" ? 12 : root.height + 18)
            anchors.verticalCenter: parent.verticalCenter
            text: root.itemName
            color: root.itemMissing ? "#c06060" : "#ddd"
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        Text {
            visible: root.viewMode === "details"
            width: 60
            anchors.verticalCenter: parent.verticalCenter
            text: root.itemKind
            color: "#777"
            font.pixelSize: 10
            elide: Text.ElideRight
        }

        Text {
            visible: root.viewMode === "details"
            width: 60
            anchors.verticalCenter: parent.verticalCenter
            text: root.itemDetail
            color: "#777"
            font.pixelSize: 10
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
        }
    }

    // ---- ドラッグ元 (1.7.5 / 3.9 / 3.10) ----
    MouseArea {
        id: dragArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.OpenHandCursor
        drag.target: dragProxy

        property bool dragActive: drag.active

        onPressed: function(mouse) {
            root.clicked()
            if (mouse.button === Qt.RightButton) {
                root.contextMenuRequested()
                return
            }
            dragProxy.x = 0
            dragProxy.y = 0
        }
        onReleased: dragProxy.Drag.drop()
        onDoubleClicked: root.doubleClicked()
    }

    Item {
        id: dragProxy
        width: 120
        height: 26
        visible: dragArea.dragActive

        Drag.active: dragArea.dragActive
        Drag.dragType: Drag.Automatic
        Drag.supportedActions: Qt.CopyAction
        // メディアだけは後方互換のキーも載せる。タイムラインの既存 DropArea が
        // そのまま動く (1.7.5)。
        Drag.mimeData: root.itemAssetId !== ""
            ? { "yave/library-item": root.itemPayload,
                "yave/asset-id": root.itemAssetId,
                "text/plain": root.itemName }
            : { "yave/library-item": root.itemPayload,
                "text/plain": root.itemName }

        Rectangle {
            anchors.fill: parent
            color: "#cc3a5f8a"
            border.color: "#4a7fbf"
            radius: 3
            opacity: 0.85

            Text {
                anchors.centerIn: parent
                width: parent.width - 8
                text: root.itemName
                color: "white"
                font.pixelSize: 10
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
