import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: container
    color: "#202020"
    border.color: "#1a1a1a"
    border.width: 1

    property alias trackModel: root.trackModel
    property int    playheadFrame: 0
    property double fps: 60.0
    property int    duration: 0

    property real zoomFactor: 0.2
    property int    trackHeaderWidth: 160
    property int    tickInterval: niceTick()

    // ユーザーがルーラをクリック / ドラッグしたとき。playheadFrame は
    // 呼び出し側 (MainWindow) が単一の書き込み元になるよう、ここでは
    // プロパティを書き換えずにシグナルのみ発行する。
    signal seekRequested(int frame)

    function niceTick() {
        const minPx = 42
        const candidates = [1, 2, 5, 10, 15, 30, 60, 120, 300, 600,
                            1200, 3000, 6000, 12000, 24000]
        for (var i = 0; i < candidates.length; ++i)
            if (candidates[i] * zoomFactor >= minPx)
                return candidates[i]
        return 24000
    }

    function visibleFrames() {
        return ruler.width / zoomFactor
    }

    function formatTime(frame) {
        const totalSec = frame / fps
        const min  = Math.floor(totalSec / 60)
        const sec  = Math.floor(totalSec % 60)
        const ff   = Math.round((totalSec - Math.floor(totalSec)) * fps)
        return min + ":" + (sec < 10 ? "0" : "") + sec
             + ":" + (ff < 10 ? "0" : "") + ff
    }

    function seekToFrame(frame) {
        const f = Math.max(0, Math.round(frame))
        seekRequested(f)
    }

    function zoomIn()  { zoomFactor = Math.min(4.0, zoomFactor * 1.5) }
    function zoomOut() { zoomFactor = Math.max(0.05, zoomFactor / 1.5) }

    function clipListModelForTrack(trackIndex) {
        return root.trackModel ? root.trackModel.clipModelProvider(trackIndex) : null
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- Unity風のタブヘッダー ----
        Rectangle {
            Layout.fillWidth: true
            height: 22
            color: "#1a1a1a"

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
                    text: "Timeline"
                    color: "#ffffff"
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: "#1a1a1a"
                z: -1
            }
        }

        // ---- ズームツールバー ----
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            spacing: 6

            Button {
                text: "-"
                implicitWidth: 26
                implicitHeight: 20
                font.bold: true
                onClicked: zoomOut()
            }
            Button {
                text: "+"
                implicitWidth: 26
                implicitHeight: 20
                font.bold: true
                onClicked: zoomIn()
            }
            Label {
                text: Math.round(zoomFactor * 100) + "%"
                color: "#aaa"
                font.pixelSize: 10
                width: 40
            }

            Item { Layout.fillWidth: true }

            Label {
                text: formatTime(playheadFrame)
                color: "#ff8c8c"
                font.pixelSize: 10
                font.bold: true
            }
        }

        // ---- ルーラ行 ----
        Row {
            id: rulerRow
            Layout.fillWidth: true
            Layout.preferredHeight: 20

            Rectangle {
                width: trackHeaderWidth
                height: parent.height
                color: "#1a1a1a"
                border.color: "#1a1a1a"
                border.width: 1
            }

            Item {
                id: ruler
                width: parent.width - trackHeaderWidth
                height: parent.height
                clip: true

                Repeater {
                    model: tickInterval > 2 ? Math.floor(Math.min(duration, visibleFrames()) / (tickInterval / 2)) : 0
                    delegate: Rectangle {
                        x: (index + 1) * (container.tickInterval / 2) * zoomFactor
                        y: 12
                        width: 1
                        height: 4
                        color: "#555"
                    }
                }

                Repeater {
                    model: Math.floor(Math.min(duration, visibleFrames()) / tickInterval) + 1
                    delegate: Rectangle {
                        x: index * container.tickInterval * zoomFactor
                        y: 4
                        width: 1
                        height: 12
                        color: "#888"

                        Label {
                            anchors.left: parent.left
                            anchors.leftMargin: 3
                            anchors.top: parent.top
                            text: container.tickInterval >= 120
                                      ? container.formatTime(index * container.tickInterval)
                                      : (index * container.tickInterval).toString()
                            color: "#bbb"
                            font.pixelSize: 8
                        }
                    }
                }

                Rectangle {
                    x: playheadFrame * zoomFactor - 5
                    y: 0
                    width: 10
                    height: 8
                    color: "#ff5c5c"
                    radius: 1
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: seekToFrame(mouse.x / zoomFactor)
                    onPositionChanged: if (pressed) seekToFrame(mouse.x / zoomFactor)
                }
            }
        }

        // ---- トラックリスト ----
        ListView {
            id: root

            property var trackModel

            Layout.fillWidth: true
            Layout.fillHeight: true

            model: trackModel
            spacing: 1
            clip: true
            orientation: ListView.Vertical
            boundsBehavior: Flickable.StopAtBounds

            Rectangle {
                anchors.fill: parent
                color: "#191919"
                z: -2
            }

            // プレイヘッド線 (ビューポート重ね合わせなのでスクロールしない)
            Rectangle {
                id: playheadLine
                x: container.trackHeaderWidth + playheadFrame * zoomFactor
                width: 1
                color: "#ff5c5c"
                z: 10
                visible: playheadFrame >= 0
                anchors.top: parent.top
                anchors.bottom: parent.bottom
            }

            delegate: Row {
                id: trackRow
                width: root.width
                height: model.height || 64

                property string trackId: model.trackId
                property int    trackIndex: index

                // ---- TrackHeader ----
                Rectangle {
                    width: container.trackHeaderWidth
                    height: parent.height
                    color: "#2d2d2d"
                    border.color: "#1a1a1a"
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 2

                        Label {
                            text: model.name
                            color: "#ddd"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            font.pixelSize: 11
                        }

                        Row {
                            spacing: 4
                            CheckBox {
                                text: qsTr("M")
                                checked: model.muted
                                scale: 0.7
                            }
                            CheckBox {
                                text: qsTr("S")
                                checked: model.solo
                                scale: 0.7
                            }
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        height: parent.height
                        width: 3
                        color: model.color
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        onClicked: {
                            trackMenu.trackId = model.trackId
                            trackMenu.popup()
                        }
                    }
                }

                // ---- クリップレーン ----
                Item {
                    width: root.width - container.trackHeaderWidth
                    height: parent.height
                    clip: true

                    Rectangle {
                        anchors.fill: parent
                        color: "#232323"
                        border.color: "#1a1a1a"
                        border.width: 1
                        z: -1
                    }

                    DropArea {
                        id: laneDrop
                        anchors.fill: parent
                        keys: ["yave/library-item", "yave/asset-id"]

                        // ドラッグ中の落とし先の表示 (1.7.5)。
                        // トランジションは境界へ吸着するので、近い境界を縦線で示す。
                        property real snappedBoundary: -1

                        function frameAt(x) {
                            return Math.max(0, Math.round(x / container.zoomFactor))
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: laneDrop.containsDrag ? "#20ffffff" : "transparent"
                        }

                        Rectangle {
                            visible: laneDrop.containsDrag && laneDrop.snappedBoundary >= 0
                            x: laneDrop.snappedBoundary * container.zoomFactor - 1
                            width: 3
                            height: parent.height
                            color: "#ffcc55"
                        }

                        onPositionChanged: (drag) => {
                            laneDrop.snappedBoundary =
                                editController.clipBoundaryNear(trackRow.trackId,
                                                                laneDrop.frameAt(drag.x), 30)
                        }
                        onExited: laneDrop.snappedBoundary = -1

                        onDropped: (drop) => {
                            const startFrame = laneDrop.frameAt(drop.x)
                            laneDrop.snappedBoundary = -1

                            if (drop.hasFormat("yave/library-item")) {
                                const payload = drop.getDataAsString("yave/library-item")
                                if (editController.dropLibraryItem(payload, trackRow.trackId,
                                                                   startFrame, ""))
                                    drop.acceptProposedAction()
                                return
                            }
                            // 後方互換: メディアのみの古い MIME
                            const assetId = drop.getDataAsString("yave/asset-id")
                            if (assetId.length === 0)
                                return
                            editController.addAssetClip(trackRow.trackIndex, trackRow.trackId,
                                                        assetId, startFrame, 300)
                            drop.acceptProposedAction()
                        }
                    }

                    // 空き領域の右クリック -> レーンコンテキストメニュー
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        onClicked: (mouse) => {
                            laneMenu.trackIndex = trackRow.trackIndex
                            laneMenu.trackId = trackRow.trackId
                            laneMenu.frame = Math.max(0, Math.round(mouse.x / container.zoomFactor))
                            laneMenu.popup()
                        }
                    }

                    Repeater {
                        model: clipListModelForTrack(index)

                        delegate: Rectangle {
                            x: start * container.zoomFactor
                            y: 4
                            width: Math.max(2, duration * container.zoomFactor)
                            height: parent.height - 8
                            radius: 2
                            color: generatedByAi ? "#5a4a7a" : model.type === "audio"
                                                     ? "#3a6a4a" : "#3a5f8a"
                            border.color: missingEffects ? "#cc4444" : Qt.lighter(color, 1.2)

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                text: textPreview || model.name
                                color: "white"
                                elide: Text.ElideRight
                                width: parent.width - 12
                                font.pixelSize: 10
                            }

                            Rectangle {
                                visible: generatedByAi && progress > 0 && progress < 1
                                anchors.bottom: parent.bottom
                                height: 3
                                width: parent.width * progress
                                color: "#88aa66"
                            }

                            // クリップの上へ落とすもの: フィルタ / エフェクト (1.7.5)
                            DropArea {
                                id: clipDrop
                                anchors.fill: parent
                                keys: ["yave/library-item"]

                                Rectangle {
                                    anchors.fill: parent
                                    visible: clipDrop.containsDrag
                                    color: "#5590ff"
                                    opacity: 0.35
                                    radius: 2
                                }

                                onDropped: (drop) => {
                                    const payload = drop.getDataAsString("yave/library-item")
                                    if (payload.length === 0)
                                        return
                                    if (editController.dropLibraryItem(payload, trackRow.trackId,
                                                                       start, model.clipId))
                                        drop.acceptProposedAction()
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onClicked: {
                                    clipMenu.clipId = model.clipId
                                    clipMenu.trackId = trackRow.trackId
                                    clipMenu.trackIndex = trackRow.trackIndex
                                    clipMenu.popup()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- コンテキストメニュー ----
    Menu {
        id: clipMenu
        property string clipId
        property string trackId
        property int    trackIndex

        MenuItem {
            text: qsTr("Split at Playhead")
            onTriggered: editController.splitClip(clipMenu.trackId, clipMenu.clipId, container.playheadFrame)
        }
        MenuItem {
            text: qsTr("Delete Clip")
            onTriggered: editController.removeClip(clipMenu.clipId)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Remove Track")
            onTriggered: editController.removeTrack(clipMenu.trackId)
        }
    }

    Menu {
        id: laneMenu
        property string trackId
        property int    trackIndex
        property int    frame

        MenuItem {
            text: qsTr("Insert Clip Here")
            onTriggered: editController.addClipToTrack(laneMenu.trackIndex, laneMenu.trackId, laneMenu.frame, 300)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Add Video Track")
            onTriggered: editController.addTrack("video", -1)
        }
        MenuItem {
            text: qsTr("Add Audio Track")
            onTriggered: editController.addTrack("audio", -1)
        }
        MenuItem {
            text: qsTr("Add Subtitle Track")
            onTriggered: editController.addTrack("subtitle", -1)
        }
    }

    Menu {
        id: trackMenu
        property string trackId

        MenuItem {
            text: qsTr("Remove Track")
            onTriggered: editController.removeTrack(trackMenu.trackId)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Add Video Track")
            onTriggered: editController.addTrack("video", -1)
        }
        MenuItem {
            text: qsTr("Add Audio Track")
            onTriggered: editController.addTrack("audio", -1)
        }
        MenuItem {
            text: qsTr("Add Subtitle Track")
            onTriggered: editController.addTrack("subtitle", -1)
        }
    }
}