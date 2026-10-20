import QtQuick 2.0
import QtQuick.Controls 2.3
import "../ZybControls/"
import "../ZybVirturlKeyBoard"
import "../Style"
import Z03Ultra 1.0

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0F0F0F"
    property color cardColor: Qt.rgba(60 / 255, 61 / 255, 66 / 255, 0.6)
    property string activeField: ""
    property string typedUrl: ""
    signal closeRequested()

    BrowserService { id: web }

    function go() {
        var text = addressInput.text.trim()
        if (text.length === 0) {
            root.statusText = qsTr("请输入网址")
            return
        }
        typedUrl = text
        web.load(text)
    }

    property string statusText: ""

    ZybBackButton {
        id: backButton
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 10
        onBackClicked: {
            if (web.busy)
                web.stop()
            root.closeRequested()
        }
    }
    ZybText {
        id: titleLabel
        text: web.title.length > 0 ? web.title : qsTr("网页浏览")
        font.styleName: "Bold"
        font.pixelSize: 14
        elide: Text.ElideRight
        width: Math.min(implicitWidth, parent.width - 220)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: backButton.verticalCenter
    }
    ZybText {
        text: web.busy ? qsTr("加载中…")
                        : (web.status.length > 0 ? web.status : root.statusText)
        color: web.busy ? "#E0A24A" : "#8E9099"
        font.pixelSize: 12
        elide: Text.ElideRight
        width: 150
        horizontalAlignment: Text.AlignRight
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: backButton.verticalCenter
    }

    Rectangle {
        id: addressBox
        anchors.top: backButton.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: parent.right
        anchors.rightMargin: 14
        height: 42
        radius: 8
        color: "#292A2E"

        BasicButton {
            id: historyBack
            text: "←"
            width: 46
            height: 32
            radius: 6
            checkable: false
            enabled: web.canGoBack
            backgroundTheme: Qt.rgba(0.25, 0.26, 0.29, 0.9)
            anchors.left: parent.left
            anchors.leftMargin: 5
            anchors.verticalCenter: parent.verticalCenter
            onClicked: web.back()
        }

        TextInput {
            id: addressInput
            anchors.left: historyBack.right
            anchors.leftMargin: 8
            anchors.right: goButton.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            color: "#F2F2F2"
            font.family: "monospace"
            font.pixelSize: 13
            clip: true
            selectByMouse: true
            onAccepted: root.go()
        }
        ZybText {
            visible: addressInput.text.length === 0
            text: qsTr("输入网址或搜索词…")
            color: "#999999"
            font.pixelSize: 13
            anchors.left: addressInput.left
            anchors.verticalCenter: addressInput.verticalCenter
        }
        MouseArea {
            anchors.fill: addressInput
            onClicked: {
                addressInput.forceActiveFocus()
                root.activeField = "address"
                overlayBrowserKeyboard.showKeyboard()
            }
        }
        BasicButton {
            id: goButton
            text: web.busy ? qsTr("停止") : qsTr("打开")
            width: 66
            height: 32
            radius: 6
            checkable: false
            backgroundTheme: web.busy ? Qt.rgba(0.7, 0.18, 0.18, 0.8)
                                      : Qt.rgba(0.19, 0.62, 0.55, 0.8)
            anchors.right: refreshButton.left
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            onClicked: web.busy ? web.stop() : root.go()
        }
        BasicButton {
            id: refreshButton
            text: qsTr("刷新")
            width: 60
            height: 32
            radius: 6
            checkable: false
            backgroundTheme: root.cardColor
            anchors.right: parent.right
            anchors.rightMargin: 5
            anchors.verticalCenter: parent.verticalCenter
            onClicked: web.reload()
        }
    }

    Rectangle {
        id: quickBar
        anchors.top: addressBox.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: parent.right
        anchors.rightMargin: 14
        height: 28
        color: "transparent"
        Flickable {
            anchors.fill: parent
            contentWidth: quickRow.width
            clip: true
            flickableDirection: Flickable.HorizontalFlick
            Row {
                id: quickRow
                spacing: 6
                Repeater {
                    model: [
                        { "name": qsTr("必应"), "url": "https://cn.bing.com" },
                        { "name": qsTr("百度"), "url": "https://www.baidu.com" },
                        { "name": qsTr("哔哩哔哩"), "url": "https://m.bilibili.com" },
                        { "name": qsTr("知乎"), "url": "https://www.zhihu.com" },
                        { "name": qsTr("回到首页"), "url": "" }
                    ]
                    Rectangle {
                        width: quickLabel.implicitWidth + 18
                        height: 26
                        radius: 13
                        color: "#1E1F24"
                        border.color: "#3A3B42"
                        border.width: 1
                        Text {
                            id: quickLabel
                            anchors.centerIn: parent
                            text: modelData.name
                            color: "#CCCCCC"
                            font.pixelSize: 12
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (modelData.url.length === 0) {
                                    web.home()
                                } else {
                                    addressInput.text = modelData.url
                                    web.load(modelData.url)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: contentCard
        anchors.top: quickBar.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.bottom: linksCard.top
        anchors.bottomMargin: 8
        radius: 8
        color: "#151619"
        clip: true

        ScrollView {
            anchors.fill: parent
            anchors.margins: 8
            clip: true
            TextArea {
                id: contentArea
                readOnly: true
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.PlainText
                color: "#D8D8D8"
                font.pixelSize: 13
                selectByMouse: true
                text: web.content
                background: Rectangle { color: "transparent" }
                onTextChanged: cursorPosition = 0
            }
        }
        ZybText {
            visible: web.content.length === 0 && !web.busy
            text: qsTr("输入网址后打开。为节省内存，页面以纯文本渲染，图片与脚本会被忽略。")
            width: contentCard.width - 40
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            color: "#777777"
            font.pixelSize: 12
            anchors.centerIn: contentCard
        }
    }

    Rectangle {
        id: linksCard
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        height: 96
        radius: 8
        color: "#151619"
        clip: true

        ZybText {
            id: linksLabel
            text: qsTr("页面链接 · ") + web.links.length
            color: "#8E9099"
            font.pixelSize: 11
            anchors.top: parent.top
            anchors.topMargin: 4
            anchors.left: parent.left
            anchors.leftMargin: 8
        }
        ListView {
            anchors.top: linksLabel.bottom
            anchors.topMargin: 2
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 4
            clip: true
            model: web.links
            spacing: 3
            delegate: Rectangle {
                width: ListView.view.width
                height: 26
                radius: 5
                color: linkMouse.containsMouse ? "#2A2B30" : "transparent"
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 6
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.text
                    color: "#7FC7FF"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
                MouseArea {
                    id: linkMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        addressInput.text = modelData.url
                        web.openLink(modelData.url)
                    }
                }
            }
        }
        ZybText {
            visible: web.links.length === 0
            text: qsTr("当前页面没有可点击的链接")
            color: "#777777"
            font.pixelSize: 11
            anchors.centerIn: parent
        }
    }

    ZybKeyBoard {
        id: overlayBrowserKeyboard
        z: 300
        width: root.width
        height: root.height
        y: Style.gHeight
        visible: y < Style.gHeight
        onCommitInput: {
            if (root.activeField === "address") {
                addressInput.text = t1
                addressInput.cursorPosition = t1.length
            }
        }
    }

    Connections {
        target: web
        onPageChanged: {
            if (web.pageUrl.length > 0)
                addressInput.text = web.pageUrl
        }
    }

    Component.onCompleted: {
        if (addressInput.text.length === 0)
            web.home()
    }
}
