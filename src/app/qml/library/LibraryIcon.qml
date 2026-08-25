import QtQuick

// アイコン 1 個。解決順序は 1.7.5:
//   1. ユーザーが割り当てたアイコン / サムネイル (source が非空)
//   2. 組み込み SVG (iconKey から選ぶ)
//   3. 種別プレースホルダ (単色 + 頭文字)
Item {
    id: root

    property string source: ""      // 上書きアイコン or image://yave-thumb/...
    property string iconKey: ""     // "kind_video" / "cat_media" / "folder"
    property string label: ""       // プレースホルダ用の文字

    // 組み込み SVG が存在するキー。無いものはプレースホルダへ落とす。
    readonly property var builtinKeys: [
        "cat_media", "cat_transition", "cat_title", "cat_subtitle", "cat_filter", "cat_effect",
        "kind_video", "kind_audio", "kind_image", "kind_generated", "folder"
    ]

    // kind_* のうち SVG を持たないものはカテゴリアイコンで代用する
    readonly property var kindAliases: ({
        "kind_transition": "cat_transition",
        "kind_title":      "cat_title",
        "kind_subtitle":   "cat_subtitle",
        "kind_filter":     "cat_filter",
        "kind_effect":     "cat_effect"
    })

    readonly property string resolvedKey: {
        if (!iconKey)
            return ""
        if (kindAliases[iconKey] !== undefined)
            return kindAliases[iconKey]
        return builtinKeys.indexOf(iconKey) >= 0 ? iconKey : ""
    }

    Image {
        id: primary
        anchors.fill: parent
        source: root.source
        visible: root.source !== "" && status !== Image.Error
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: true
        smooth: true
    }

    Image {
        id: builtin
        anchors.fill: parent
        anchors.margins: Math.round(parent.width * 0.1)
        visible: !primary.visible && root.resolvedKey !== ""
        source: root.resolvedKey !== "" ? "qrc:/qt/qml/Yave/qml/icons/" + root.resolvedKey + ".svg" : ""
        fillMode: Image.PreserveAspectFit
        sourceSize.width: 64
        sourceSize.height: 64
        smooth: true
    }

    Rectangle {
        anchors.fill: parent
        visible: !primary.visible && !builtin.visible
        color: "#3a3f4a"
        radius: 3
        border.color: "#4a4f5a"

        Text {
            anchors.centerIn: parent
            text: root.label.length > 0 ? root.label.charAt(0).toUpperCase() : "?"
            color: "#cfd6e4"
            font.pixelSize: Math.max(9, parent.height * 0.5)
            font.bold: true
        }
    }
}
