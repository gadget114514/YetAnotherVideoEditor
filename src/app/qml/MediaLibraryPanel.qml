import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#202020" // Unity window background
    border.color: "#1a1a1a"
    border.width: 1

    // OS からのドロップエリア
    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls) {
                for (var i = 0; i < drop.urls.length; ++i) {
                    var url = drop.urls[i]
                    if (url) {
                        projectController.registerAsset(url.toString())
                    }
                }
                drop.acceptProposedAction()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- Unity風のタブヘッダー ----
        Rectangle {
            Layout.fillWidth: true
            height: 22
            color: "#1a1a1a"

            // アクティブタブ
            Rectangle {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                width: 80
                height: 21
                color: "#202020"
                border.color: "#1a1a1a"
                border.width: 1

                // 下部境界線を隠してタブらしく見せる
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: "#202020"
                    anchors.leftMargin: 1
                    anchors.rightMargin: 1
                }

                Label {
                    anchors.centerIn: parent
                    text: "Project"
                    color: "#ffffff"
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            // 全体の下部ボーダー
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: "#1a1a1a"
                z: -1
            }
        }

        // 説明ラベル
        Rectangle {
            Layout.fillWidth: true
            height: 24
            color: "#282828"
            border.color: "#1a1a1a"

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 8
                text: qsTr("Drag & drop files here from Explorer")
                color: "#888"
                font.pixelSize: 10
            }
        }

        // アセットリスト
        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: assetListModel
            clip: true
            spacing: 1
            boundsBehavior: Flickable.StopAtBounds

            // リスト自体の背景
            Rectangle {
                anchors.fill: parent
                color: "#1c1c1c"
                z: -2
            }

            delegate: Item {
                id: delegateItem
                width: listView.width
                height: 32

                Rectangle {
                    anchors.fill: parent
                    color: dragArea.pressed ? "#3c3c3c" : (dragArea.containsMouse ? "#2d2d2d" : "#222222")
                    border.color: "#1a1a1a"
                    radius: 2
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 6

                    // 種別アイコン
                    Rectangle {
                        width: 24
                        height: 24
                        color: model.kind === "video" ? "#3a5f8a" : (model.kind === "audio" ? "#3a6a4a" : "#555555")
                        radius: 2
                        Text {
                            anchors.centerIn: parent
                            text: model.kind === "video" ? "V" : (model.kind === "audio" ? "A" : "I")
                            color: "white"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        Label {
                            text: model.name
                            color: "#eee"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            font.pixelSize: 11
                        }
                        Label {
                            text: (model.duration / 60.0).toFixed(1) + "s"
                            color: "#666"
                            font.pixelSize: 9
                        }
                    }
                }

                MouseArea {
                    id: dragArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.OpenHandCursor

                    drag.target: dragDummy

                    property bool dragActive: drag.active

                    onPressed: {
                        dragDummy.x = delegateItem.x
                        dragDummy.y = delegateItem.y
                    }

                    onReleased: {
                        dragDummy.Drag.drop()
                    }
                }

                Item {
                    id: dragDummy
                    width: delegateItem.width
                    height: delegateItem.height
                    x: 0
                    y: 0
                    visible: dragArea.dragActive

                    Drag.active: dragArea.dragActive
                    Drag.dragType: Drag.Automatic
                    Drag.keys: ["yave/asset-id"]
                    Drag.hotSpot.x: width / 2
                    Drag.hotSpot.y: height / 2
                    Drag.supportedActions: Qt.CopyAction
                    Drag.mimeData: {
                        "yave/asset-id": model.assetId,
                        "yave/asset-type": model.kind,
                        "yave/asset-name": model.name,
                        "text/plain": model.assetId
                    }

                    Rectangle {
                        width: parent.width
                        height: parent.height
                        color: "#cc3a5f8a"
                        border.color: "#3a5f8a"
                        border.width: 1
                        radius: 3
                        opacity: 0.8
                        Text {
                            anchors.centerIn: parent
                            text: model.name
                            color: "white"
                            elide: Text.ElideRight
                            width: parent.width - 8
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }
    }
}
