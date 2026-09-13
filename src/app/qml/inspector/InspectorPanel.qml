import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// インスペクタパネル (1.7.1 inspector)。
// 選択中のトラック / クリップの情報表示とプロパティ編集を担う。
// 編集はすべて editController 経由で Undo コマンドとして積む (3.2)。
Rectangle {
    id: root
    color: "#202020"
    border.color: "#1a1a1a"
    border.width: 1

    readonly property var blendNames: [
        qsTr("Normal"), qsTr("Add"), qsTr("Multiply"), qsTr("Screen"),
        qsTr("Overlay"), qsTr("Darken"), qsTr("Lighten"), qsTr("Color Dodge"),
        qsTr("Color Burn"), qsTr("Difference"), qsTr("Exclusion"), qsTr("Alpha Mask")
    ]

    // 字幕スタイル編集で最後にどの色フィールドを開いたか
    property string colorTarget: ""

    function openColor(field, current) {
        root.colorTarget = field
        colorDialog.selectedColor = current
        colorDialog.open()
    }

    function commitColor(color) {
        if (selectionModel && selectionModel.hasClip) {
            editController.setSubtitleStyle(selectionModel.clipId, root.colorTarget, color)
            root.refreshAll()
        }
    }

    ColorDialog {
        id: colorDialog
        onAccepted: root.commitColor(selectedColor)
    }

    // 色を選ぶ小さなスウォッチ + テキスト入力
    component ColorRow : RowLayout {
        id: colorRow
        property string field: ""
        property var    colorValue: "#000000"
        Layout.fillWidth: true

        Rectangle {
            id: colorSwatch
            implicitWidth: 26
            implicitHeight: 20
            radius: 2
            border.color: "#555"
            border.width: 1
            color: colorRow.colorValue

            MouseArea {
                anchors.fill: parent
                onClicked: root.openColor(colorRow.field, colorRow.colorValue)
            }
        }

        TextField {
            Layout.fillWidth: true
            text: colorRow.colorValue
            onEditingFinished: {
                if (selectionModel.hasClip)
                    editController.setSubtitleStyle(selectionModel.clipId,
                                                    colorRow.field, text)
            }
        }
    }

    function refreshAll() {
        if (!selectionModel)
            return

        if (selectionModel.hasClip) {
            const info = selectionModel.clipInfo
            clipNameField.text      = info.name
            clipTypeLabel.text      = info.type
            clipEnabledBox.checked  = info.enabled
            clipLockedBox.checked   = info.locked
            startBox.value          = info.start
            durationBox.value       = info.duration
            endLabel.text           = String(info.end)
            offsetBox.value         = info.sourceOffset
            fadeInBox.value         = info.fadeIn
            fadeOutBox.value        = info.fadeOut
            opacitySlider.value     = info.opacity
            blendCombo.currentIndex = info.blendMode

            const isAudio = info.isAudio
            gainRow.visible  = isAudio
            panRow.visible   = isAudio
            gainSlider.value = isAudio ? info.gain : 0
            panSlider.value  = isAudio ? info.pan : 0

            const isSub = info.isSubtitle === true
            subtitleFields.visible = isSub
            if (isSub) {
                subtitleTextEdit.text          = info.text !== undefined ? info.text : ""
                fontFamilyField.text           = info.fontFamily !== undefined ? info.fontFamily : ""
                fontPointSizeBox.value         = info.fontPointSize !== undefined ? Math.round(info.fontPointSize) : 0
                fontWeightBox.value            = info.fontWeight !== undefined ? info.fontWeight : 0
                italicBox.checked              = info.italic === true
                fillColorRow.colorValue        = info.fillColor !== undefined ? info.fillColor : "#000000"
                outlineColorRow.colorValue     = info.outlineColor !== undefined ? info.outlineColor : "#000000"
                outlineWidthBox.value          = info.outlineWidth !== undefined ? info.outlineWidth : 0
                shadowColorRow.colorValue      = info.shadowColor !== undefined ? info.shadowColor : "#000000"
                shadowBlurBox.value            = info.shadowBlur !== undefined ? info.shadowBlur : 0
                boxEnabledBox.checked          = info.boxEnabled === true
                boxColorRow.colorValue         = info.boxColor !== undefined ? info.boxColor : "#000000"
                hAlignCombo.currentIndex       = info.hAlign !== undefined ? info.hAlign : 1
                vAlignCombo.currentIndex       = info.vAlign !== undefined ? info.vAlign : 2
                anchorXSlider.value            = info.anchorX !== undefined ? info.anchorX : 0.5
                anchorYSlider.value            = info.anchorY !== undefined ? info.anchorY : 0.92
                rotationBox.value              = info.rotationDeg !== undefined ? info.rotationDeg : 0
                textOpacitySlider.value        = info.styleOpacity !== undefined ? info.styleOpacity : 1
                verticalBox.checked            = info.vertical === true
            }
        } else if (selectionModel.hasTrack) {
            const t = selectionModel.trackInfo
            trackNameField.text     = t.name
            trackTypeLabel.text     = t.type
            trackClipCount.text     = String(t.clipCount)
            trackMutedBox.checked   = t.muted
            trackSoloBox.checked    = t.solo
            trackVisibleBox.checked = t.visible
            trackLockedBox.checked  = t.locked
            trackHeightBox.value    = t.height
            trackGainSlider.value   = t.gain
            trackPanSlider.value    = t.pan
            trackOpacitySlider.value= t.opacity
            trackBlendCombo.currentIndex = t.blendMode

            trackAudioRow.visible  = t.isAudio
            trackVideoRow.visible  = t.isVideo
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- タブ風ヘッダー ----
        Rectangle {
            Layout.fillWidth: true
            height: 22
            color: "#1a1a1a"

            Rectangle {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                width: Math.max(80, headerLabel.width + 20)
                height: 21
                color: "#202020"
                border.color: "#1a1a1a"
                border.width: 1

                Label {
                    id: headerLabel
                    anchors.centerIn: parent
                    text: qsTr("Inspector")
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

        // ---- 内容 ----
        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: scroll.availableWidth
                spacing: 8

                // ---- 未選択 ----
                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    text: qsTr("Select a clip or track in the timeline to inspect and edit its properties.")
                    color: "#777"
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    visible: !selectionModel || (!selectionModel.hasClip && !selectionModel.hasTrack)
                }

                // ================= トラック =================
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    spacing: 6
                    visible: selectionModel && selectionModel.hasTrack && !selectionModel.hasClip

                    Rectangle {
                        Layout.fillWidth: true
                        height: 20
                        color: "#2a2a2a"
                        radius: 2

                        Label {
                            anchors.centerIn: parent
                            text: qsTr("Track")
                            color: "#ccc"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 6

                        Label { text: qsTr("Name"); color: "#aaa"; font.pixelSize: 11 }
                        TextField {
                            id: trackNameField
                            Layout.fillWidth: true
                            onEditingFinished: {
                                if (selectionModel.hasTrack)
                                    editController.setTrackProperty(selectionModel.trackId, "name", text)
                            }
                        }

                        Label { text: qsTr("Type"); color: "#aaa"; font.pixelSize: 11 }
                        Label {
                            id: trackTypeLabel
                            Layout.fillWidth: true
                            color: "#ddd"
                            font.pixelSize: 11
                        }

                        Label { text: qsTr("Clips"); color: "#aaa"; font.pixelSize: 11 }
                        Label {
                            id: trackClipCount
                            Layout.fillWidth: true
                            color: "#ddd"
                            font.pixelSize: 11
                        }

                        Label { text: qsTr("Height"); color: "#aaa"; font.pixelSize: 11 }
                        SpinBox {
                            id: trackHeightBox
                            Layout.fillWidth: true
                            from: 24
                            to: 400
                            editable: true
                            onValueModified: {
                                if (selectionModel.hasTrack)
                                    editController.setTrackProperty(selectionModel.trackId, "height", value)
                            }
                        }
                    }

                    // ---- 音声トラック ----
                    GridLayout {
                        id: trackAudioRow
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 6
                        visible: false

                        Label { text: qsTr("Gain"); color: "#aaa"; font.pixelSize: 11 }
                        RowLayout {
                            Layout.fillWidth: true
                            Slider {
                                id: trackGainSlider
                                Layout.fillWidth: true
                                from: 0
                                to: 2
                                stepSize: 0.01
                                onPressedChanged: {
                                    if (!pressed && selectionModel.hasTrack)
                                        editController.setTrackProperty(selectionModel.trackId, "gain", value)
                                }
                            }
                            Label {
                                text: trackGainSlider.value.toFixed(2)
                                color: "#ddd"
                                font.pixelSize: 10
                                width: 34
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        Label { text: qsTr("Pan"); color: "#aaa"; font.pixelSize: 11 }
                        RowLayout {
                            Layout.fillWidth: true
                            Slider {
                                id: trackPanSlider
                                Layout.fillWidth: true
                                from: -1
                                to: 1
                                stepSize: 0.01
                                onPressedChanged: {
                                    if (!pressed && selectionModel.hasTrack)
                                        editController.setTrackProperty(selectionModel.trackId, "pan", value)
                                }
                            }
                            Label {
                                text: trackPanSlider.value.toFixed(2)
                                color: "#ddd"
                                font.pixelSize: 10
                                width: 34
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }

                    // ---- 映像トラック ----
                    GridLayout {
                        id: trackVideoRow
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 6
                        visible: false

                        Label { text: qsTr("Opacity"); color: "#aaa"; font.pixelSize: 11 }
                        RowLayout {
                            Layout.fillWidth: true
                            Slider {
                                id: trackOpacitySlider
                                Layout.fillWidth: true
                                from: 0
                                to: 1
                                stepSize: 0.01
                                onPressedChanged: {
                                    if (!pressed && selectionModel.hasTrack)
                                        editController.setTrackProperty(selectionModel.trackId, "opacity", value)
                                }
                            }
                            Label {
                                text: trackOpacitySlider.value.toFixed(2)
                                color: "#ddd"
                                font.pixelSize: 10
                                width: 34
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        Label { text: qsTr("Blend"); color: "#aaa"; font.pixelSize: 11 }
                        ComboBox {
                            id: trackBlendCombo
                            Layout.fillWidth: true
                            model: root.blendNames
                            onActivated: {
                                if (selectionModel.hasTrack)
                                    editController.setTrackProperty(selectionModel.trackId, "blendMode", currentIndex)
                            }
                        }
                    }

                    // ---- 状態 ----
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 4

                        CheckBox {
                            id: trackMutedBox
                            text: qsTr("Muted")
                            onToggled: {
                                if (selectionModel.hasTrack)
                                    editController.setTrackProperty(selectionModel.trackId, "muted", checked)
                            }
                        }
                        CheckBox {
                            id: trackSoloBox
                            text: qsTr("Solo")
                            onToggled: {
                                if (selectionModel.hasTrack)
                                    editController.setTrackProperty(selectionModel.trackId, "solo", checked)
                            }
                        }
                        CheckBox {
                            id: trackVisibleBox
                            text: qsTr("Visible")
                            onToggled: {
                                if (selectionModel.hasTrack)
                                    editController.setTrackProperty(selectionModel.trackId, "visible", checked)
                            }
                        }
                        CheckBox {
                            id: trackLockedBox
                            text: qsTr("Locked")
                            onToggled: {
                                if (selectionModel.hasTrack)
                                    editController.setTrackProperty(selectionModel.trackId, "locked", checked)
                            }
                        }
                    }
                }

                // ================= クリップ =================
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    spacing: 6
                    visible: selectionModel && selectionModel.hasClip

                    Rectangle {
                        Layout.fillWidth: true
                        height: 20
                        color: "#2a2a2a"
                        radius: 2

                        Label {
                            anchors.centerIn: parent
                            text: qsTr("Clip")
                            color: "#ccc"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 6

                        Label { text: qsTr("Name"); color: "#aaa"; font.pixelSize: 11 }
                        TextField {
                            id: clipNameField
                            Layout.fillWidth: true
                            onEditingFinished: {
                                if (selectionModel.hasClip)
                                    editController.setClipProperty(selectionModel.clipId, "name", text)
                            }
                        }

                        Label { text: qsTr("Type"); color: "#aaa"; font.pixelSize: 11 }
                        Label {
                            id: clipTypeLabel
                            Layout.fillWidth: true
                            color: "#ddd"
                            font.pixelSize: 11
                        }

                        Label { text: qsTr("Start"); color: "#aaa"; font.pixelSize: 11 }
                        SpinBox {
                            id: startBox
                            Layout.fillWidth: true
                            from: 0
                            to: 999999999
                            editable: true
                            onValueModified: {
                                if (selectionModel.hasClip)
                                    editController.setClipProperty(selectionModel.clipId, "start", value)
                            }
                        }

                        Label { text: qsTr("Duration"); color: "#aaa"; font.pixelSize: 11 }
                        SpinBox {
                            id: durationBox
                            Layout.fillWidth: true
                            from: 1
                            to: 999999999
                            editable: true
                            onValueModified: {
                                if (selectionModel.hasClip)
                                    editController.setClipProperty(selectionModel.clipId, "duration", value)
                            }
                        }

                        Label { text: qsTr("End"); color: "#aaa"; font.pixelSize: 11 }
                        Label {
                            id: endLabel
                            Layout.fillWidth: true
                            color: "#ddd"
                            font.pixelSize: 11
                        }

                        Label { text: qsTr("Source offset"); color: "#aaa"; font.pixelSize: 11 }
                        SpinBox {
                            id: offsetBox
                            Layout.fillWidth: true
                            from: 0
                            to: 999999999
                            editable: true
                            onValueModified: {
                                if (selectionModel.hasClip)
                                    editController.setClipProperty(selectionModel.clipId, "sourceOffset", value)
                            }
                        }

                        Label { text: qsTr("Fade in"); color: "#aaa"; font.pixelSize: 11 }
                        SpinBox {
                            id: fadeInBox
                            Layout.fillWidth: true
                            from: 0
                            to: 999999
                            editable: true
                            onValueModified: {
                                if (selectionModel.hasClip)
                                    editController.setClipProperty(selectionModel.clipId, "fadeIn", value)
                            }
                        }

                        Label { text: qsTr("Fade out"); color: "#aaa"; font.pixelSize: 11 }
                        SpinBox {
                            id: fadeOutBox
                            Layout.fillWidth: true
                            from: 0
                            to: 999999
                            editable: true
                            onValueModified: {
                                if (selectionModel.hasClip)
                                    editController.setClipProperty(selectionModel.clipId, "fadeOut", value)
                            }
                        }
                    }

                    // ---- 映像プロパティ ----
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 6

                        Label { text: qsTr("Opacity"); color: "#aaa"; font.pixelSize: 11 }
                        RowLayout {
                            Layout.fillWidth: true
                            Slider {
                                id: opacitySlider
                                Layout.fillWidth: true
                                from: 0
                                to: 1
                                stepSize: 0.01
                                onPressedChanged: {
                                    if (!pressed && selectionModel.hasClip)
                                        editController.setClipProperty(selectionModel.clipId, "opacity", value)
                                }
                            }
                            Label {
                                text: opacitySlider.value.toFixed(2)
                                color: "#ddd"
                                font.pixelSize: 10
                                width: 34
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        Label { text: qsTr("Blend"); color: "#aaa"; font.pixelSize: 11 }
                        ComboBox {
                            id: blendCombo
                            Layout.fillWidth: true
                            model: root.blendNames
                            onActivated: {
                                if (selectionModel.hasClip)
                                    editController.setClipProperty(selectionModel.clipId, "blendMode", currentIndex)
                            }
                        }
                    }

                    // ---- 音声プロパティ ----
                    GridLayout {
                        id: gainRow
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 6
                        visible: false

                        Label { text: qsTr("Gain"); color: "#aaa"; font.pixelSize: 11 }
                        RowLayout {
                            Layout.fillWidth: true
                            Slider {
                                id: gainSlider
                                Layout.fillWidth: true
                                from: 0
                                to: 2
                                stepSize: 0.01
                                onPressedChanged: {
                                    if (!pressed && selectionModel.hasClip)
                                        editController.setClipProperty(selectionModel.clipId, "gain", value)
                                }
                            }
                            Label {
                                text: gainSlider.value.toFixed(2)
                                color: "#ddd"
                                font.pixelSize: 10
                                width: 34
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }

                    GridLayout {
                        id: panRow
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 6
                        visible: false

                        Label { text: qsTr("Pan"); color: "#aaa"; font.pixelSize: 11 }
                        RowLayout {
                            Layout.fillWidth: true
                            Slider {
                                id: panSlider
                                Layout.fillWidth: true
                                from: -1
                                to: 1
                                stepSize: 0.01
                                onPressedChanged: {
                                    if (!pressed && selectionModel.hasClip)
                                        editController.setClipProperty(selectionModel.clipId, "pan", value)
                                }
                            }
                            Label {
                                text: panSlider.value.toFixed(2)
                                color: "#ddd"
                                font.pixelSize: 10
                                width: 34
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }

                    // ---- 字幕プロパティ (文字列 / フォント / 色 / 形 / 大きさ / 位置) ----
                    ColumnLayout {
                        id: subtitleFields
                        Layout.fillWidth: true
                        spacing: 6
                        visible: false

                        Rectangle {
                            Layout.fillWidth: true
                            height: 20
                            color: "#2a2a2a"
                            radius: 2
                            Label {
                                anchors.centerIn: parent
                                text: qsTr("Subtitle")
                                color: "#ccc"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        // ---- 文字列 ----
                        Label { text: qsTr("Text"); color: "#aaa"; font.pixelSize: 11 }
                        TextArea {
                            id: subtitleTextEdit
                            Layout.fillWidth: true
                            Layout.preferredHeight: 64
                            wrapMode: TextEdit.Wrap
                            color: "#ddd"
                            background: Rectangle { color: "#1a1a1a"; border.color: "#333" }
                            onEditingFinished: {
                                if (selectionModel.hasClip)
                                    editController.setSubtitleText(selectionModel.clipId, text)
                            }
                        }

                        // ---- フォント ----
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 6

                            Label { text: qsTr("Font"); color: "#aaa"; font.pixelSize: 11 }
                            TextField {
                                id: fontFamilyField
                                Layout.fillWidth: true
                                onEditingFinished: {
                                    if (selectionModel.hasClip)
                                        editController.setSubtitleStyle(selectionModel.clipId, "fontFamily", text)
                                }
                            }

                            Label { text: qsTr("Size"); color: "#aaa"; font.pixelSize: 11 }
                            SpinBox {
                                id: fontPointSizeBox
                                Layout.fillWidth: true
                                from: 8
                                to: 400
                                editable: true
                                onValueModified: {
                                    if (selectionModel.hasClip)
                                        editController.setSubtitleStyle(selectionModel.clipId, "fontPointSize", value)
                                }
                            }

                            Label { text: qsTr("Weight"); color: "#aaa"; font.pixelSize: 11 }
                            SpinBox {
                                id: fontWeightBox
                                Layout.fillWidth: true
                                from: 100
                                to: 900
                                stepSize: 100
                                editable: true
                                onValueModified: {
                                    if (selectionModel.hasClip)
                                        editController.setSubtitleStyle(selectionModel.clipId, "fontWeight", value)
                                }
                            }

                            Label { text: qsTr("Italic"); color: "#aaa"; font.pixelSize: 11 }
                            CheckBox {
                                id: italicBox
                                Layout.fillWidth: true
                                onToggled: {
                                    if (selectionModel.hasClip)
                                        editController.setSubtitleStyle(selectionModel.clipId, "italic", checked)
                                }
                            }
                        }

                        // ---- 色 / 形 (縁取り・影・背景ボックス) ----
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 6

                            Label { text: qsTr("Fill"); color: "#aaa"; font.pixelSize: 11 }
                            ColorRow { id: fillColorRow; field: "fillColor" }

                            Label { text: qsTr("Outline"); color: "#aaa"; font.pixelSize: 11 }
                            ColorRow { id: outlineColorRow; field: "outlineColor" }

                            Label { text: qsTr("Outline width"); color: "#aaa"; font.pixelSize: 11 }
                            SpinBox {
                                id: outlineWidthBox
                                Layout.fillWidth: true
                                from: 0
                                to: 60
                                editable: true
                                onValueModified: {
                                    if (selectionModel.hasClip)
                                        editController.setSubtitleStyle(selectionModel.clipId, "outlineWidth", value)
                                }
                            }

                            Label { text: qsTr("Shadow"); color: "#aaa"; font.pixelSize: 11 }
                            ColorRow { id: shadowColorRow; field: "shadowColor" }

                            Label { text: qsTr("Shadow blur"); color: "#aaa"; font.pixelSize: 11 }
                            SpinBox {
                                id: shadowBlurBox
                                Layout.fillWidth: true
                                from: 0
                                to: 60
                                editable: true
                                onValueModified: {
                                    if (selectionModel.hasClip)
                                        editController.setSubtitleStyle(selectionModel.clipId, "shadowBlur", value)
                                }
                            }

                            Label { text: qsTr("Background box"); color: "#aaa"; font.pixelSize: 11 }
                            CheckBox {
                                id: boxEnabledBox
                                Layout.fillWidth: true
                                onToggled: {
                                    if (selectionModel.hasClip)
                                        editController.setSubtitleStyle(selectionModel.clipId, "boxEnabled", checked)
                                }
                            }

                            Label { text: qsTr("Box color"); color: "#aaa"; font.pixelSize: 11 }
                            ColorRow { id: boxColorRow; field: "boxColor" }
                        }

                        // ---- 位置 ----
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 6

                            Label { text: qsTr("H align"); color: "#aaa"; font.pixelSize: 11 }
                            ComboBox {
                                id: hAlignCombo
                                Layout.fillWidth: true
                                model: [qsTr("Left"), qsTr("Center"), qsTr("Right")]
                                onActivated: {
                                    if (selectionModel.hasClip)
                                        editController.setSubtitleStyle(selectionModel.clipId, "hAlign", currentIndex)
                                }
                            }

                            Label { text: qsTr("V align"); color: "#aaa"; font.pixelSize: 11 }
                            ComboBox {
                                id: vAlignCombo
                                Layout.fillWidth: true
                                model: [qsTr("Top"), qsTr("Middle"), qsTr("Bottom")]
                                onActivated: {
                                    if (selectionModel.hasClip)
                                        editController.setSubtitleStyle(selectionModel.clipId, "vAlign", currentIndex)
                                }
                            }

                            Label { text: qsTr("Anchor X"); color: "#aaa"; font.pixelSize: 11 }
                            RowLayout {
                                Layout.fillWidth: true
                                Slider {
                                    id: anchorXSlider
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 1
                                    stepSize: 0.01
                                    onPressedChanged: {
                                        if (!pressed && selectionModel.hasClip)
                                            editController.setSubtitleStyle(selectionModel.clipId,
                                                                            "anchor", Qt.point(anchorXSlider.value, anchorYSlider.value))
                                    }
                                }
                                Label {
                                    text: anchorXSlider.value.toFixed(2)
                                    color: "#ddd"
                                    font.pixelSize: 10
                                    width: 34
                                    horizontalAlignment: Text.AlignRight
                                }
                            }

                            Label { text: qsTr("Anchor Y"); color: "#aaa"; font.pixelSize: 11 }
                            RowLayout {
                                Layout.fillWidth: true
                                Slider {
                                    id: anchorYSlider
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 1
                                    stepSize: 0.01
                                    onPressedChanged: {
                                        if (!pressed && selectionModel.hasClip)
                                            editController.setSubtitleStyle(selectionModel.clipId,
                                                                            "anchor", Qt.point(anchorXSlider.value, anchorYSlider.value))
                                    }
                                }
                                Label {
                                    text: anchorYSlider.value.toFixed(2)
                                    color: "#ddd"
                                    font.pixelSize: 10
                                    width: 34
                                    horizontalAlignment: Text.AlignRight
                                }
                            }

                            Label { text: qsTr("Rotation"); color: "#aaa"; font.pixelSize: 11 }
                            SpinBox {
                                id: rotationBox
                                Layout.fillWidth: true
                                from: -180
                                to: 180
                                editable: true
                                onValueModified: {
                                    if (selectionModel.hasClip)
                                        editController.setSubtitleStyle(selectionModel.clipId, "rotationDeg", value)
                                }
                            }

                            Label { text: qsTr("Text opacity"); color: "#aaa"; font.pixelSize: 11 }
                            RowLayout {
                                Layout.fillWidth: true
                                Slider {
                                    id: textOpacitySlider
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 1
                                    stepSize: 0.01
                                    onPressedChanged: {
                                        if (!pressed && selectionModel.hasClip)
                                            editController.setSubtitleStyle(selectionModel.clipId, "opacity", value)
                                    }
                                }
                                Label {
                                    text: textOpacitySlider.value.toFixed(2)
                                    color: "#ddd"
                                    font.pixelSize: 10
                                    width: 34
                                    horizontalAlignment: Text.AlignRight
                                }
                            }

                            Label { text: qsTr("Vertical"); color: "#aaa"; font.pixelSize: 11 }
                            CheckBox {
                                id: verticalBox
                                Layout.fillWidth: true
                                onToggled: {
                                    if (selectionModel.hasClip)
                                        editController.setSubtitleStyle(selectionModel.clipId, "vertical", checked)
                                }
                            }
                        }
                    }

                    // ---- 状態 ----
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 4

                        CheckBox {
                            id: clipEnabledBox
                            text: qsTr("Enabled")
                            onToggled: {
                                if (selectionModel.hasClip)
                                    editController.setClipProperty(selectionModel.clipId, "enabled", checked)
                            }
                        }
                        CheckBox {
                            id: clipLockedBox
                            text: qsTr("Locked")
                            onToggled: {
                                if (selectionModel.hasClip)
                                    editController.setClipProperty(selectionModel.clipId, "locked", checked)
                            }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: selectionModel
        function onSelectionChanged() {
            root.refreshAll()
        }
    }

    Component.onCompleted: root.refreshAll()
}