import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 表示モード切替 / アイコンサイズ / 検索 / 新規フォルダ (1.7.5)
Rectangle {
    id: root

    // "list" | "smallIcons" | "grid" | "details"
    property string viewMode: "grid"
    property int    iconSize: 64
    property alias  filterText: search.text
    property bool   canCreateFolder: true

    signal newFolderRequested()

    implicitHeight: 28
    color: "#282828"
    border.color: "#1a1a1a"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        spacing: 4

        ToolButton {
            text: "＋"
            enabled: root.canCreateFolder
            implicitWidth: 24
            implicitHeight: 22
            ToolTip.visible: hovered
            ToolTip.text: qsTr("New Folder")
            onClicked: root.newFolderRequested()
        }

        TextField {
            id: search
            Layout.fillWidth: true
            Layout.preferredHeight: 22
            placeholderText: qsTr("Search")
            font.pixelSize: 11
            color: "#ddd"
            background: Rectangle {
                color: "#1c1c1c"
                border.color: search.activeFocus ? "#4a7fbf" : "#333"
                radius: 2
            }
        }

        // ---- 表示モード ----
        Row {
            spacing: 1
            Repeater {
                model: [
                    { key: "list",       glyph: "≡",  tip: qsTr("List") },
                    { key: "smallIcons", glyph: "▤",  tip: qsTr("Small Icons") },
                    { key: "grid",       glyph: "▦",  tip: qsTr("Large Icons") },
                    { key: "details",    glyph: "☰",  tip: qsTr("Details") }
                ]
                delegate: Rectangle {
                    required property var modelData
                    width: 24
                    height: 22
                    color: root.viewMode === modelData.key ? "#3c5a80" : "transparent"
                    border.color: root.viewMode === modelData.key ? "#4a7fbf" : "transparent"
                    radius: 2

                    Text {
                        anchors.centerIn: parent
                        text: modelData.glyph
                        color: "#ddd"
                        font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        ToolTip.visible: containsMouse
                        ToolTip.text: modelData.tip
                        onClicked: root.viewMode = modelData.key
                    }
                }
            }
        }

        // ---- アイコンサイズ (グリッド / 小アイコンに効く) ----
        Slider {
            id: sizeSlider
            Layout.preferredWidth: 80
            from: 32
            to: 160
            stepSize: 8
            value: root.iconSize
            enabled: root.viewMode === "grid" || root.viewMode === "smallIcons"
            opacity: enabled ? 1.0 : 0.35
            onMoved: root.iconSize = Math.round(value)

            ToolTip.visible: hovered
            ToolTip.text: qsTr("Icon size: %1 px").arg(root.iconSize)
        }
    }
}
