import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ログパネル。Inspector とタブ切替で表示する (D&D 不具合調査用に追加)。
// qInstallMessageHandler で捕まえた qDebug/qInfo/qWarning が logModel に流れてくる。
Rectangle {
    id: root
    color: "#181818"
    border.color: "#1a1a1a"
    border.width: 1

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
                text: qsTr("Clear")
                onClicked: logModel.clear()
            }
        }

        ListView {
            id: logList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: logModel
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {}

            delegate: Text {
                width: logList.width
                text: model.text
                color: text.indexOf("[ERROR]") >= 0 || text.indexOf("[WARN]") >= 0
                           ? "#ff8a80" : "#ccc"
                font.family: "Consolas"
                font.pixelSize: 10
                wrapMode: Text.Wrap
            }

            onCountChanged: if (autoScrollBox.checked) logList.positionViewAtEnd()
        }
    }
}
