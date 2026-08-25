import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "timeline"
import "library"
import Yave

// メインウィンドウ。プレビュー + タイムライン + インスペクタの 3 ペイン構成。
ApplicationWindow {
    id: root
    width: 1600
    height: 900
    visible: true
    title: qsTr("YetAnotherVideoEditor")

    menuBar: MenuBar {
        Menu {
            title: qsTr("File")
            Action { text: qsTr("New Project") }
            Action { text: qsTr("Open...") }
            MenuSeparator { }
            Action { text: qsTr("Save"); shortcut: "Ctrl+S" }
            Action { text: qsTr("Save As...") }
        }
        Menu {
            title: qsTr("Edit")
            Action {
                text: qsTr("Undo")
                shortcut: "Ctrl+Z"
                onTriggered: editController.undo()
            }
            Action {
                text: qsTr("Redo")
                shortcut: "Ctrl+Y"
                onTriggered: editController.redo()
            }
        }
        Menu {
            title: qsTr("Language")
            Action { text: qsTr("Japanese") }
            Action { text: qsTr("English") }
        }
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Vertical

        // 上半部 (左右分割)
        SplitView {
            SplitView.fillHeight: true
            orientation: Qt.Horizontal

            // 左ペイン: 上がユーザー素材、下が組み込み/プラグインのカタログ (1.7.4)
            SplitView {
                orientation: Qt.Vertical
                SplitView.preferredWidth: 340
                SplitView.minimumWidth: 220
                SplitView.maximumWidth: 640

                LibraryPanel {
                    SplitView.preferredHeight: 320
                    SplitView.minimumHeight: 120
                    panelId: "mediaLibrary"
                    title: qsTr("Media Library")
                    categories: ["media"]
                    acceptsFileDrop: true
                }

                LibraryPanel {
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 120
                    panelId: "effectLibrary"
                    title: qsTr("Effect Library")
                    categories: ["transition", "title", "subtitle", "filter", "effect"]
                }
            }

            ColumnLayout {
                SplitView.fillWidth: true
                spacing: 4

                // ---- プレビューコンテナ（元のドッキング場所） ----
                Rectangle {
                    id: previewDockArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#202020"
                    border.color: "#1a1a1a"
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        // ---- Unity風のタブヘッダー ----
                        Rectangle {
                            Layout.fillWidth: true
                            height: 22
                            color: "#1a1a1a"
                            visible: !isPreviewFloating

                            Rectangle {
                                anchors.left: parent.left
                                anchors.bottom: parent.bottom
                                width: 80
                                height: 21
                                color: "#202020"
                                border.color: "#1a1a1a"
                                border.width: 1

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
                                    text: "Game"
                                    color: "#ffffff"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }
                        }

                        // プレビュー表示エリア
                        Rectangle {
                            id: previewDockContent
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "#191919"

                            Label {
                                anchors.centerIn: parent
                                text: qsTr("Preview window is currently detached. Click 'Dock' or close the window to re-dock.")
                                color: "#666"
                                font.pixelSize: 11
                                visible: isPreviewFloating
                            }

                            Component.onCompleted: {
                                if (!isPreviewFloating) {
                                    previewWrapper.parent = previewDockContent
                                }
                            }
                        }
                    }
                }

                // ---- 再生コントロール ----
                Rectangle {
                    Layout.fillWidth: true
                    height: 32
                    color: "#282828"
                    border.color: "#1a1a1a"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        Button {
                            text: playbackController.isPlaying ? qsTr("Pause") : qsTr("Play")
                            onClicked: playbackController.isPlaying
                                           ? playbackController.pause()
                                           : playbackController.play()
                        }
                        Button {
                            text: qsTr("Stop")
                            onClicked: playbackController.stop()
                        }

                        // ---- ルーラ / シークバー ----
                        Slider {
                            id: seekBar
                            Layout.fillWidth: true
                            from: 0
                            to: playbackController.duration
                            value: playheadFrame
                            onMoved: {
                                root.playheadFrame = Math.round(seekBar.value)
                                playbackController.seek(Math.round(seekBar.value))
                            }
                        }

                        Label {
                            text: (playbackController.latencySamples / 48.0).toFixed(1) + " ms"
                            color: "#888"
                            font.pixelSize: 10
                        }
                    }
                }
            }
        }

        // 下半部 (ボトム全幅)
        TimelineView {
            id: timelineView
            SplitView.preferredHeight: parent.height * 0.4
            SplitView.minimumHeight: 150
            trackModel: trackListModel
            playheadFrame: root.playheadFrame
            fps: playbackController.fps
            duration: playbackController.duration
        }
    }

    // ---- コンテキストプロパティ相当の値 ----
    property int    playheadFrame: 0
    property bool   isPreviewFloating: false

    Timer {
        interval: 16
        running: playbackController.isPlaying
        repeat: true
        onTriggered: root.playheadFrame = playbackController.currentFrame()
    }

    // タイムラインのルーラクリック / ドラッグからのシーク
    Connections {
        target: timelineView
        function onSeekRequested(frame) {
            root.playheadFrame = frame
            playbackController.seek(frame)
        }
    }

    // 独立したプレビューウィンドウ（浮動窓）
    Window {
        id: floatWindow
        visible: isPreviewFloating
        title: qsTr("Game (Floating)")
        width: 800
        height: 480
        color: "#202020"
        onClosing: {
            isPreviewFloating = false
        }

        Rectangle {
            id: floatWindowContent
            anchors.fill: parent
            color: "#191919"
        }

        Binding {
            target: previewWrapper
            property: "parent"
            value: isPreviewFloating ? floatWindowContent : previewDockContent
        }
    }

    // プレビューラッパーの実体（reparentされる）
    Item {
        id: previewWrapper
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // 浮動中のみラッパー自身がヘッダーを表示する
            Rectangle {
                Layout.fillWidth: true
                height: 22
                color: "#1a1a1a"
                visible: isPreviewFloating

                Rectangle {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    width: 100
                    height: 21
                    color: "#202020"
                    border.color: "#1a1a1a"
                    border.width: 1

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
                        text: "Game (Floating)"
                        color: "#ffffff"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#191919"

                PreviewItem {
                    id: preview
                    objectName: "preview"
                    anchors.centerIn: parent
                    width: parent.width * 0.95
                    height: parent.height * 0.95
                    frameIndex: playheadFrame
                }

                Label {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 8
                    color: "#888"
                    font.pixelSize: 10
                    text: qsTr("Preview - frame %1").arg(playheadFrame)
                }

                Button {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 8
                    text: isPreviewFloating ? qsTr("Dock") : qsTr("Float")
                    onClicked: isPreviewFloating = !isPreviewFloating
                }
            }
        }
    }

}
