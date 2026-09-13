import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ログパネル。Inspector とタブ切替で表示する (D&D 不具合調査用に追加)。
// 全ログを 1 つの TextEdit に集約して表示するため、複数行にまたがって
// 選択 → Ctrl+C でコピーできる。
Rectangle {
    id: root
    color: "#181818"
    border.color: "#1a1a1a"
    border.width: 1

    // ログ行を HTML に変換する。ERROR / WARN 行だけ赤くする。
    function buildHtml() {
        const lines = logModel.allText().split("\n")
        var html = ""
        for (var i = 0; i < lines.length; ++i) {
            const l = lines[i]
            if (i > 0)
                html += "\n"
            const esc = l.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
            if (l.indexOf("[ERROR]") >= 0 || l.indexOf("[WARN]") >= 0)
                html += '<font color="#ff8a80">' + esc + "</font>"
            else
                html += esc
        }
        return html
    }

    function refresh() {
        logText.text = root.buildHtml()
        if (autoScrollBox.checked)
            flick.contentY = flick.contentHeight
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("Log")
                color: "#bbb"
                font.pixelSize: 11
                Layout.fillWidth: true
            }

            CheckBox {
                id: autoScrollBox
                text: qsTr("Auto-scroll")
                checked: true
                scale: 0.9
            }

            Button {
                text: qsTr("Copy Selected")
                onClicked: logModel.copyToClipboard(logText.selectedText)
            }

            Button {
                text: qsTr("Copy All")
                onClicked: logModel.copyAllToClipboard()
            }

            Button {
                text: qsTr("Clear")
                onClicked: logModel.clear()
            }
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: Math.max(logText.implicitHeight, height)

            TextEdit {
                id: logText
                width: flick.width
                textFormat: TextEdit.RichText
                readOnly: true
                wrapMode: TextEdit.WrapAtWordBoundaryOrAnywhere
                font.family: "Consolas"
                font.pixelSize: 10
                color: "#ccc"
                selectByMouse: true
                selectByKeyboard: true
                selectionColor: "#3355aa"
                selectedTextColor: "#ffffff"

                // 右クリック: 選択範囲 / 全行をコピー
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: logMenu.popup()
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }
    }

    Menu {
        id: logMenu
        MenuItem {
            text: qsTr("Copy selected")
            onTriggered: logModel.copyToClipboard(logText.selectedText)
        }
        MenuItem {
            text: qsTr("Copy all")
            onTriggered: logModel.copyAllToClipboard()
        }
    }

    Connections {
        target: logModel
        function onRowsInserted() { root.refresh() }
        function onRowsRemoved()  { root.refresh() }
    }

    Component.onCompleted: root.refresh()
}