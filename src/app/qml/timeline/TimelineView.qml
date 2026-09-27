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

    // 選択状態 (インスペクタと連動)。MainWindow が SelectionModel から供給する。
    property string selectedClipId: ""
    property string selectedTrackId: ""

    // クリップをドラッグ中の移動先トラック行 (ハイライト表示用)
    property string dragClipId: ""
    property int    dragTargetRow: -1

    property real zoomFactor: 0.2
    property int    trackHeaderWidth: 160
    readonly property int tickInterval: {
        const minPx = 45
        const candidates = [1, 2, 5, 10, 15, 30, 60, 120, 300, 600,
                            1200, 3000, 6000, 12000, 24000]
        for (var i = 0; i < candidates.length; ++i) {
            if (candidates[i] * zoomFactor >= minPx)
                return candidates[i]
        }
        return 24000
    }

    // ユーザーがルーラをクリック / ドラッグしたとき。playheadFrame は
    // 呼び出し側 (MainWindow) が単一の書き込み元になるよう、ここでは
    // プロパティを書き換えずにシグナルのみ発行する。
    signal seekRequested(int frame)

    // クリックによる選択。呼び出し側 (MainWindow) が SelectionModel へ反映する。
    signal clipSelected(string trackId, string clipId)
    signal trackSelected(string trackId)

    function visibleFrames() {
        const w = (typeof ruler !== "undefined" && ruler && ruler.width > 0)
                      ? ruler.width
                      : (container.width - trackHeaderWidth)
        return Math.max(1, w) / zoomFactor
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

    // トラック追加後、新しい行が画面外に隠れないようスクロールする。
    function addTrackAndReveal(type, index) {
        const row = editController.addTrack(type, index)
        if (row >= 0)
            root.positionViewAtIndex(row, ListView.Contain)
    }

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

            // ---- トラック追加 ----
            // トラックが 0 本だと右クリックメニューを出す場所が無いので、
            // ツールバーからも追加できるようにする。
            Rectangle {
                implicitWidth: 1
                implicitHeight: 16
                color: "#3a3a3a"
            }
            Button {
                text: qsTr("+ Video Track")
                implicitHeight: 20
                font.pixelSize: 10
                onClicked: container.addTrackAndReveal("video", -1)
            }
            Button {
                text: qsTr("+ Audio Track")
                implicitHeight: 20
                font.pixelSize: 10
                onClicked: container.addTrackAndReveal("audio", -1)
            }
            Button {
                text: qsTr("+ Subtitle Track")
                implicitHeight: 20
                font.pixelSize: 10
                onClicked: container.addTrackAndReveal("subtitle", -1)
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
        RowLayout {
            id: rulerRow
            Layout.fillWidth: true
            Layout.preferredHeight: 22
            // 子が fillHeight を持つと入れ子 Layout も既定で fillHeight になり、
            // トラックリストの高さを奪ってしまうので明示的に固定する。
            Layout.fillHeight: false
            spacing: 0

            Rectangle {
                Layout.preferredWidth: container.trackHeaderWidth
                Layout.fillHeight: true
                color: "#1a1a1a"
                border.color: "#282828"
                border.width: 1

                Label {
                    anchors.centerIn: parent
                    text: qsTr("Time")
                    color: "#777777"
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            Rectangle {
                id: ruler
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#222222"
                clip: true

                // 下部境界線
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: "#1a1a1a"
                }

                // 副目盛り
                Repeater {
                    model: container.tickInterval > 2 ? Math.floor(container.visibleFrames() / (container.tickInterval / 2)) : 0
                    delegate: Rectangle {
                        x: (index + 1) * (container.tickInterval / 2) * container.zoomFactor
                        y: ruler.height - 5
                        width: 1
                        height: 5
                        color: "#444444"
                    }
                }

                // 主目盛りと時間ラベル
                Repeater {
                    model: Math.floor(container.visibleFrames() / container.tickInterval) + 1
                    delegate: Item {
                        x: index * container.tickInterval * container.zoomFactor
                        y: 0
                        width: 1
                        height: ruler.height

                        Rectangle {
                            anchors.left: parent.left
                            anchors.bottom: parent.bottom
                            width: 1
                            height: 10
                            color: "#777777"
                        }

                        Label {
                            anchors.left: parent.left
                            anchors.leftMargin: 3
                            anchors.top: parent.top
                            anchors.topMargin: 2
                            text: container.tickInterval >= 120
                                      ? container.formatTime(index * container.tickInterval)
                                      : (index * container.tickInterval).toString()
                            color: "#bbbbbb"
                            font.pixelSize: 9
                        }
                    }
                }

                Rectangle {
                    x: container.playheadFrame * container.zoomFactor - 5
                    y: 0
                    width: 11
                    height: 10
                    color: "#ff5c5c"
                    radius: 1
                    z: 10
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: container.seekToFrame(mouse.x / container.zoomFactor)
                    onPositionChanged: if (pressed) container.seekToFrame(mouse.x / container.zoomFactor)
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

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

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

            // プレイヘッドのドラッグハンドル (トラック領域内でもカーソルを掴んで動かせるように、
            // 細い線そのものより広い当たり判定を持たせる)
            MouseArea {
                id: playheadHandle
                x: playheadLine.x - 5
                width: 11
                z: 11
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                cursorShape: Qt.SizeHorCursor
                preventStealing: true

                function frameFromMouseX(mx) {
                    return (playheadHandle.x + mx - container.trackHeaderWidth) / container.zoomFactor
                }

                onPressed: (mouse) => container.seekToFrame(frameFromMouseX(mouse.x))
                onPositionChanged: (mouse) => {
                    if (pressed)
                        container.seekToFrame(frameFromMouseX(mouse.x))
                }
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
                    color: model.trackId === container.selectedTrackId ? "#3a3f4a" : "#2d2d2d"
                    border.color: model.trackId === container.selectedTrackId ? "#ffb54a" : "#1a1a1a"
                    border.width: model.trackId === container.selectedTrackId ? 2 : 1

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
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: (mouse) => {
                            if (mouse.button === Qt.RightButton) {
                                trackMenu.trackId = model.trackId
                                trackMenu.popup()
                            } else {
                                container.trackSelected(model.trackId)
                            }
                        }
                    }
                }

                // ---- クリップレーン ----
                Item {
                    id: laneItem
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

                    // タイムスケールと連動したグリッド線
                    Repeater {
                        model: Math.floor(container.visibleFrames() / container.tickInterval) + 1
                        delegate: Rectangle {
                            x: index * container.tickInterval * container.zoomFactor
                            width: 1
                            height: parent.height
                            color: "#2c2c2c"
                            z: -1
                        }
                    }

                    // クリップ移動時の移動先ハイライト (互換トラックのみ)
                    Rectangle {
                        anchors.fill: parent
                        color: index === container.dragTargetRow && container.dragClipId.length > 0
                                   ? "#2affcc55" : "transparent"
                        z: 0
                    }

                    DropArea {
                        id: laneDrop
                        anchors.fill: parent
                        // 内部ドラッグ (ライブラリから) のみ。OS ファイルドロップは
                        // ListView 直下の fileDropArea が受け持つ (デリゲート内では
                        // 外部ドロップの urls が空になる Qt の既知挙動があるため)。
                        keys: ["yave/library-item", "yave/asset-id"]

                        // ドラッグ中の落とし先の表示 (1.7.5)。
                        // トランジションは境界へ吸着するので、近い境界を縦線で示す。
                        property real snappedBoundary: -1
                        // このトラックへ落とせる内容かどうか (ハイライト制御)
                        property bool dropAllowed: true
                        // onPositionChanged のログ間引き用 (毎フレーム出すと読めない)
                        property int lastLoggedFrame: -1

                        function frameAt(x) {
                            return Math.max(0, Math.round(x / container.zoomFactor))
                        }

                        // 内部ドラッグでは mimeData が配送されないため drag.source を優先し、
                        // 外部/ネイティブドラッグ用に getDataAsString をフォールバックにする。
                        function payloadOf(ev) {
                            if (ev.source && ev.source.payload)
                                return ev.source.payload
                            return ev.getDataAsString("yave/library-item")
                        }

                        function assetIdOf(ev) {
                            if (ev.source && ev.source.assetId)
                                return ev.source.assetId
                            return ev.getDataAsString("yave/asset-id")
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: laneDrop.containsDrag && laneDrop.dropAllowed
                                       ? "#20ffffff" : "transparent"
                        }

                        Rectangle {
                            visible: laneDrop.containsDrag && laneDrop.snappedBoundary >= 0
                            x: laneDrop.snappedBoundary * container.zoomFactor - 1
                            width: 3
                            height: parent.height
                            color: "#ffcc55"
                        }

                        onEntered: (drag) => {
                            laneDrop.dropAllowed = true
                            const payload = laneDrop.payloadOf(drag)
                            console.log("[drag] ENTERED track=" + trackRow.trackId
                                        + " trackIndex=" + trackRow.trackIndex
                                        + " hasSource=" + (drag.source ? "yes" : "no")
                                        + " payloadLen=" + payload.length)
                            if (payload.length > 0) {
                                try {
                                    const obj = JSON.parse(payload)
                                    if (obj.category === "media" && obj.assetId)
                                        laneDrop.dropAllowed =
                                            editController.canDropAssetOnTrack(
                                                editController.assetKind(obj.assetId),
                                                trackRow.trackId)
                                    else if (obj.category)
                                        laneDrop.dropAllowed =
                                            editController.canDropOnTrack(obj.category,
                                                                          trackRow.trackId)
                                    console.log("[laneDrop] payload category=" + obj.category
                                                + " assetId=" + obj.assetId
                                                + " dropAllowed=" + laneDrop.dropAllowed)
                                } catch (e) { /* 不正なペイロードはそのまま受ける */ }
                            }
                        }

                        onPositionChanged: (drag) => {
                            const frame = laneDrop.frameAt(drag.x)
                            laneDrop.snappedBoundary =
                                editController.clipBoundaryNear(trackRow.trackId, frame, 30)
                            // フレームが動いたときだけ出す (間引かないと数十行/秒になる)
                            if (frame !== laneDrop.lastLoggedFrame) {
                                laneDrop.lastLoggedFrame = frame
                                console.log("[drag] MOVE track=" + trackRow.trackId
                                            + " frame=" + frame
                                            + " snap=" + laneDrop.snappedBoundary
                                            + " allowed=" + laneDrop.dropAllowed)
                            }
                        }
                        onExited: {
                            laneDrop.snappedBoundary = -1
                            laneDrop.lastLoggedFrame = -1
                            console.log("[drag] EXITED track=" + trackRow.trackId)
                        }

                        onDropped: (drop) => {
                            const startFrame = laneDrop.frameAt(drop.x)
                            laneDrop.snappedBoundary = -1
                            laneDrop.lastLoggedFrame = -1

                            const payload = laneDrop.payloadOf(drop)
                            console.log("[drag] DROPPED track=" + trackRow.trackId
                                        + " frame=" + startFrame
                                        + " via=" + (drop.source && drop.source.payload
                                                         ? "source" : "mime")
                                        + " payloadLen=" + payload.length)

                            if (payload.length > 0) {
                                if (editController.dropLibraryItem(payload, trackRow.trackId,
                                                                   startFrame, "")) {
                                    drop.acceptProposedAction()
                                } else {
                                    console.warn("[drag] dropLibraryItem -> REJECTED: "
                                                + editController.lastDropError())
                                }
                                return
                            }
                            // 後方互換: メディアのみの古い MIME
                            const assetId = laneDrop.assetIdOf(drop)
                            if (assetId.length > 0) {
                                editController.addAssetClip(trackRow.trackIndex, trackRow.trackId,
                                                            assetId, startFrame, 0)
                                drop.acceptProposedAction()
                                return
                            }
                            console.log("[drag] DROPPED but no payload/assetId -> ignored")
                        }
                    }

                    // 空き領域のクリック/ドラッグ -> トラック選択 + プレイヘッド移動
                    // (右クリックはレーンコンテキストメニュー)
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onPressed: (mouse) => {
                            if (mouse.button === Qt.RightButton)
                                return
                            container.trackSelected(trackRow.trackId)
                            container.seekToFrame(mouse.x / container.zoomFactor)
                        }
                        onPositionChanged: (mouse) => {
                            if (pressed)
                                container.seekToFrame(mouse.x / container.zoomFactor)
                        }
                        onClicked: (mouse) => {
                            if (mouse.button === Qt.RightButton) {
                                laneMenu.trackIndex = trackRow.trackIndex
                                laneMenu.trackId = trackRow.trackId
                                laneMenu.frame = Math.max(0, Math.round(mouse.x / container.zoomFactor))
                                laneMenu.popup()
                            }
                        }
                    }

                    Repeater {
                        model: clipListModelForTrack(index)

                        delegate: Rectangle {
                            id: clipVisual
                            x: start * container.zoomFactor
                            y: 4
                            width: Math.max(2, duration * container.zoomFactor)
                            height: parent.height - 8
                            radius: 2
                            color: generatedByAi ? "#5a4a7a" : model.type === "audio"
                                                     ? "#3a6a4a" : "#3a5f8a"
                            border.color: model.clipId === container.selectedClipId
                                              ? "#ffb54a"
                                              : (missingEffects ? "#cc4444" : Qt.lighter(color, 1.2))
                            border.width: model.clipId === container.selectedClipId ? 2 : 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 6
                                spacing: 4

                                Text {
                                    text: textPreview || model.name
                                    color: "white"
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                    font.pixelSize: 10
                                }

                                Text {
                                    text: container.formatTime(duration)
                                    color: "#b0ffffff"
                                    font.pixelSize: 9
                                    visible: clipVisual.width > 70
                                }
                            }

                            ToolTip.visible: bodyMouse.containsMouse && !bodyMouse.drag.active
                            ToolTip.delay: 400
                            ToolTip.text: (textPreview || model.name) + "\n"
                                        + qsTr("Start: ") + container.formatTime(start) + " (" + start + "f)\n"
                                        + qsTr("Duration: ") + container.formatTime(duration) + " (" + duration + "f)"

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
                                    // 内部ドラッグは drag.source 経由でしかデータが来ない
                                    const payload = (drop.source && drop.source.payload)
                                                        ? drop.source.payload
                                                        : drop.getDataAsString("yave/library-item")
                                    console.log("[clipDrop] DROPPED clip=" + model.clipId
                                                + " via=" + (drop.source && drop.source.payload
                                                                 ? "source" : "mime")
                                                + " payloadLen=" + payload.length)
                                    if (payload.length === 0) {
                                        console.warn("[clipDrop] DROPPED with empty payload -> ignored")
                                        return
                                    }
                                    if (editController.dropLibraryItem(payload, trackRow.trackId,
                                                                       start, model.clipId)) {
                                        drop.acceptProposedAction()
                                    } else {
                                        console.warn("[clipDrop] dropLibraryItem -> REJECTED: "
                                                     + editController.lastDropError())
                                    }
                                }
                            }

                            MouseArea {
                                id: bodyMouse
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                hoverEnabled: true
                                drag.target: clipVisual
                                drag.axis: Drag.XAxis
                                drag.threshold: 4

                                property real dragStartX: 0

                                onPressed: (mouse) => {
                                    if (mouse.button === Qt.RightButton) {
                                        clipMenu.clipId = model.clipId
                                        clipMenu.trackId = trackRow.trackId
                                        clipMenu.trackIndex = trackRow.trackIndex
                                        clipMenu.popup()
                                        return
                                    }
                                    container.clipSelected(trackRow.trackId, model.clipId)
                                    container.dragClipId = model.clipId
                                    container.dragTargetRow = trackRow.trackIndex
                                    dragStartX = clipVisual.x
                                }
                                onPositionChanged: (mouse) => {
                                    // drag.target が x を動かす。0 より左へは行かない
                                    if (clipVisual.x < 0)
                                        clipVisual.x = 0
                                    // 移動先トラック行を追跡 (別トラック移動用)
                                    const p = root.mapFromItem(bodyMouse, mouse.x, mouse.y)
                                    const row = root.indexAt(p.x, p.y + root.contentY)
                                    if (row >= 0) {
                                        const tid = root.model.trackIdAt(row)
                                        if (editController.canDropClipOnTrack(model.clipId, tid))
                                            container.dragTargetRow = row
                                        else
                                            container.dragTargetRow = -1
                                    } else {
                                        container.dragTargetRow = -1
                                    }
                                }
                                onReleased: {
                                    const targetRow = container.dragTargetRow
                                    container.dragClipId = ""
                                    container.dragTargetRow = -1
                                    const newStart = Math.max(0, Math.round(clipVisual.x / container.zoomFactor))
                                    if (Math.abs(clipVisual.x - dragStartX) >= 1
                                            || targetRow !== trackRow.trackIndex) {
                                        const toTrackId = targetRow >= 0
                                                              ? root.model.trackIdAt(targetRow)
                                                              : trackRow.trackId
                                        if (toTrackId.length > 0) {
                                            editController.moveClip(trackRow.trackId, toTrackId,
                                                                    model.clipId, newStart, duration)
                                        }
                                    }
                                }
                            }

                            // ---- 左トリムハンドル (In を変更 = 開始位置と長さ) ----
                            Rectangle {
                                id: leftHandle
                                width: 6
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                color: model.clipId === container.selectedClipId ? "#ffb54a" : "#666666"
                                opacity: 0.8
                                z: 2

                                MouseArea {
                                    id: leftTrim
                                    anchors.fill: parent
                                    cursorShape: Qt.SplitHCursor

                                    property point pressLane: Qt.point(0, 0)
                                    property int   startF: 0
                                    property int   endF: 0
                                    property bool  moved: false

                                    onPressed: (mouse) => {
                                        container.clipSelected(trackRow.trackId, model.clipId)
                                        pressLane = laneItem.mapFromItem(leftTrim, mouse.x, mouse.y)
                                        startF = start
                                        endF = start + duration
                                        moved = false
                                        mouse.accepted = true
                                    }
                                    onPositionChanged: (mouse) => {
                                        const p = laneItem.mapFromItem(leftTrim, mouse.x, mouse.y)
                                        const deltaF = Math.round((p.x - pressLane.x) / container.zoomFactor)
                                        if (deltaF !== 0)
                                            moved = true
                                        const ns = Math.min(Math.max(0, startF + deltaF), endF - 1)
                                        clipVisual.x = ns * container.zoomFactor
                                        clipVisual.width = Math.max(1, (endF - ns) * container.zoomFactor)
                                    }
                                    onReleased: {
                                        if (moved) {
                                            const ns = Math.max(0, Math.round(clipVisual.x / container.zoomFactor))
                                            editController.trimClip(trackRow.trackId, model.clipId,
                                                                    0, ns, endF - ns)
                                        }
                                    }
                                }
                            }

                            // ---- 右トリムハンドル (Out を変更 = 長さ) ----
                            Rectangle {
                                id: rightHandle
                                width: 6
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                color: model.clipId === container.selectedClipId ? "#ffb54a" : "#666666"
                                opacity: 0.8
                                z: 2

                                MouseArea {
                                    id: rightTrim
                                    anchors.fill: parent
                                    cursorShape: Qt.SplitHCursor

                                    property point pressLane: Qt.point(0, 0)
                                    property int   startF: 0
                                    property int   endF: 0
                                    property bool  moved: false

                                    onPressed: (mouse) => {
                                        container.clipSelected(trackRow.trackId, model.clipId)
                                        pressLane = laneItem.mapFromItem(rightTrim, mouse.x, mouse.y)
                                        startF = start
                                        endF = start + duration
                                        moved = false
                                        mouse.accepted = true
                                    }
                                    onPositionChanged: (mouse) => {
                                        const p = laneItem.mapFromItem(rightTrim, mouse.x, mouse.y)
                                        const deltaF = Math.round((p.x - pressLane.x) / container.zoomFactor)
                                        if (deltaF !== 0)
                                            moved = true
                                        const ne = Math.max(startF + 1, endF + deltaF)
                                        clipVisual.width = Math.max(1, (ne - startF) * container.zoomFactor)
                                    }
                                    onReleased: {
                                        if (moved) {
                                            const ne = startF + Math.max(1, Math.round(clipVisual.width / container.zoomFactor))
                                            editController.trimClip(trackRow.trackId, model.clipId,
                                                                    0, startF, ne - startF)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- OS ファイルドロップ ----
    // Flickable (ListView) の内側では外部ドラッグの urls が空になる Qt の既知挙動が
    // あるため、ルート (コンテナ直下) で受ける。内部ドラッグ (ライブラリ) は
    // デリゲート側の laneDrop が受け持つ。
    DropArea {
        id: fileDropArea
        anchors.fill: parent

        // 外部ファイルドロップ (text/uri-list) だけを受ける。keys が無いと
        // 最前面の DropArea が内部ドラッグ (ライブラリ) まで奪ってしまい、
        // デリゲート側の laneDrop へ届かなくなる。
        keys: ["text/uri-list"]

        Rectangle {
            anchors.fill: parent
            color: fileDropArea.containsDrag ? "#15ffffff" : "transparent"
            z: 100
        }

        onEntered: (drag) => {
            console.log("[fileDrop] ENTERED hasUrls=" + drag.hasUrls
                        + " hasText=" + drag.hasText)
        }

        onDropped: (drop) => {
            let urls = drop.urls
            let dropPos = Qt.point(drop.x, drop.y)

            // QML の DragEvent は外部ファイルドロップの urls / text/uri-list を
            // 空で渡すことがある (Qt の既知挙動)。その場合はウィンドウレベルで
            // 捕捉した FileDropFilter がネイティブ QDropEvent から取り出した
            // URL と座標 (ウィンドウ座標) をフォールバックとして使う。
            if ((!drop.hasUrls || !urls || urls.length === 0)
                    && fileDropFilter && fileDropFilter.lastUrls.length > 0) {
                urls = fileDropFilter.lastUrls
                dropPos = fileDropArea.mapFromItem(Window.window.contentItem,
                                                   fileDropFilter.lastPos)
            }

            if (!urls || urls.length === 0) {
                console.warn("[fileDrop] no urls, ignored")
                return
            }

            // 落下位置を ListView のコンテンツ座標へ変換してトラック行を求める
            const p = root.mapFromItem(fileDropArea, dropPos.x, dropPos.y)
            const row = root.indexAt(p.x, p.y + root.contentY)
            const trackId   = row >= 0 ? root.model.trackIdAt(row) : ""
            const trackType = row >= 0 ? root.model.trackTypeAt(row) : ""
            const frame = Math.max(0, Math.round(
                (p.x - container.trackHeaderWidth) / container.zoomFactor))
            console.log("[fileDrop] DROPPED row=" + row + " track=" + trackId
                        + " type=" + trackType + " frame=" + frame
                        + " urls=" + urls.length)

            if (row < 0 || trackId.length === 0) {
                console.warn("[fileDrop] no valid track row, ignored")
                return
            }

            for (let i = 0; i < urls.length; ++i) {
                const urlStr = String(urls[i])
                const isSub = /\.srt$/i.test(urlStr) || /\.vtt$/i.test(urlStr)
if (isSub) {
                            // 字幕ファイルは importSrt がトラックを決定する:
                            //   初回      -> 既存の字幕トラックへ (無ければ新規)
                            //   同一ファイル -> 無視 (skipped)
                            //   別のSRT   -> 新しい字幕トラックへ
                            console.log("[fileDrop] SRT url=" + urlStr)
                            const res = editController.importSrt(urlStr, {
                                overlapPolicy: 1,
                                targetTrackIndex: -1,
                                fadeInFrames: 8,
                                fadeOutFrames: 8
                            })
                            console.log("[fileDrop] importSrt ok=" + res.ok
                                        + " skipped=" + (res.skipped === true)
                                        + " imported=" + res.importedCount
                                        + " trackIndex=" + res.trackIndex)
                            if (!res.ok)
                                console.warn("[fileDrop] SRT import failed: " + urlStr)
                } else {
                    // メディアファイル: アセット登録して、落下先トラックへ置く。
                    // duration 0 = アセットの実尺を使う。
                    console.log("[fileDrop] media url=" + urlStr)
                    const assetId = projectController.registerAsset(urlStr)
                    if (assetId) {
                        console.log("[fileDrop] registerAsset returned: '" + assetId + "'")
                        editController.addAssetClip(row, trackId, assetId, frame, 0)
                    } else {
                        console.warn("[fileDrop] registerAsset FAILED for: " + urlStr)
                    }
                }
            }
            drop.acceptProposedAction()
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
            onTriggered: container.addTrackAndReveal("video", -1)
        }
        MenuItem {
            text: qsTr("Add Audio Track")
            onTriggered: container.addTrackAndReveal("audio", -1)
        }
        MenuItem {
            text: qsTr("Add Subtitle Track")
            onTriggered: container.addTrackAndReveal("subtitle", -1)
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
            onTriggered: container.addTrackAndReveal("video", -1)
        }
        MenuItem {
            text: qsTr("Add Audio Track")
            onTriggered: container.addTrackAndReveal("audio", -1)
        }
        MenuItem {
            text: qsTr("Add Subtitle Track")
            onTriggered: container.addTrackAndReveal("subtitle", -1)
        }
    }
}