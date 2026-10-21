import QtQuick 2.0
import QtQuick.Controls 2.3
import QtWebEngine 1.4
import "../ZybControls/"
import "../ZybVirturlKeyBoard"
import "../Style"

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0F0F0F"
    property color cardColor: Qt.rgba(60 / 255, 61 / 255, 66 / 255, 0.6)
    property string activeField: ""
    property string typedUrl: ""
    property bool pageError: false
    property string errorText: ""
    signal closeRequested()

    function navigateTo(text) {
        var target = String(text || "").trim()
        if (target.length === 0)
            return
        if (target.indexOf("://") < 0) {
            if (target.charAt(0) === "/")
                target = "file://" + target
            else if (target.indexOf(" ") >= 0 || target.indexOf(".") < 0)
                target = "https://cn.bing.com/search?q=" + encodeURIComponent(target)
            else
                target = "https://" + target
        }
        root.pageError = false
        if (String(web.url) === target) {
            web.reload()
        } else {
            web.url = target
        }
    }

    function go() {
        var text = addressInput.text.trim()
        if (text.length === 0) {
            addressInput.forceActiveFocus()
            overlayBrowserKeyboard.showKeyboard()
            return
        }
        typedUrl = text
        addressInput.focus = false
        overlayBrowserKeyboard.hideKeyboard()
        navigateTo(text)
    }

    function openUrl(url) {
        var text = String(url || "").trim()
        if (text.length === 0) return
        addressInput.text = text
        root.typedUrl = text
        navigateTo(text)
    }

    function home() {
        root.pageError = false
        addressInput.text = ""
        web.loadHtml(
            "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
            + "<style>body{font-family:sans-serif;background:#fff;color:#111;padding:22px}"
            + "h1{font-size:22px;margin:0 0 6px}p{color:#666;font-size:13px;line-height:1.5}"
            + "a{display:block;color:#1565c0;font-size:17px;text-decoration:none;padding:12px 0;"
            + "border-bottom:1px solid #e6e6e6}</style></head><body>"
            + "<title>起始页</title><h1>Z03Ultra 浏览器</h1>"
            + "<p>完整网页渲染，支持 JavaScript、图片和视频。输入网址或搜索词开始浏览。</p>"
            + "<a href='https://cn.bing.com'>必应搜索</a>"
            + "<a href='https://www.baidu.com'>百度</a>"
            + "<a href='https://m.bilibili.com'>哔哩哔哩</a>"
            + "<a href='https://www.zhihu.com'>知乎</a>"
            + "<a href='https://news.qq.com'>腾讯新闻</a>"
            + "<a href='https://github.com'>GitHub</a>"
            + "</body></html>",
            "about:blank")
    }

    property string statusText: ""

    ZybBackButton {
        id: backButton
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 10
        onBackClicked: {
            if (web.loading)
                web.stop()
            overlayBrowserKeyboard.hideKeyboard()
            root.closeRequested()
        }
    }
    ZybText {
        id: titleLabel
        text: web.title.length > 0 ? web.title : qsTr("网页浏览")
        font.styleName: "Bold"
        font.pixelSize: 14
        elide: Text.ElideRight
        width: Math.min(implicitWidth, parent.width - 320)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: backButton.verticalCenter
    }
    ZybText {
        text: web.loading ? (web.loadProgress + "%")
                           : (root.pageError ? qsTr("加载失败")
                                             : (root.statusText.length > 0 ? root.statusText : ""))
        color: web.loading ? "#E0A24A" : (root.pageError ? "#E05A5A" : "#8E9099")
        font.pixelSize: 12
        elide: Text.ElideRight
        width: 220
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
            width: 42
            height: 32
            radius: 6
            checkable: false
            enabled: web.canGoBack
            backgroundTheme: Qt.rgba(0.25, 0.26, 0.29, 0.9)
            anchors.left: parent.left
            anchors.leftMargin: 5
            anchors.verticalCenter: parent.verticalCenter
            onClicked: web.goBack()
        }

        BasicButton {
            id: historyForward
            text: "→"
            width: 42
            height: 32
            radius: 6
            checkable: false
            enabled: web.canGoForward
            backgroundTheme: Qt.rgba(0.25, 0.26, 0.29, 0.9)
            anchors.left: historyBack.right
            anchors.leftMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            onClicked: web.goForward()
        }

        TextInput {
            id: addressInput
            anchors.left: historyForward.right
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
            text: web.loading ? qsTr("停止") : qsTr("打开")
            width: 66
            height: 32
            radius: 6
            checkable: false
            backgroundTheme: web.loading ? Qt.rgba(0.7, 0.18, 0.18, 0.8)
                                         : Qt.rgba(0.19, 0.62, 0.55, 0.8)
            anchors.right: refreshButton.left
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            onClicked: {
                if (web.loading)
                    web.stop()
                else
                    root.go()
            }
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
            onClicked: {
                if (root.pageError)
                    root.navigateTo(addressInput.text)
                else
                    web.reload()
            }
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
                        { "name": qsTr("起始页"), "url": "" },
                        { "name": qsTr("必应"), "url": "https://cn.bing.com" },
                        { "name": qsTr("百度"), "url": "https://www.baidu.com" },
                        { "name": qsTr("哔哩哔哩"), "url": "https://m.bilibili.com" },
                        { "name": qsTr("知乎"), "url": "https://www.zhihu.com" }
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
                                    root.home()
                                } else {
                                    addressInput.text = modelData.url
                                    root.navigateTo(modelData.url)
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
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        radius: 8
        color: "#FFFFFF"
        clip: true

        WebEngineView {
            id: web
            anchors.fill: parent
            anchors.margins: 1
            backgroundColor: "#FFFFFF"
            zoomFactor: 1.0

            onLoadingChanged: {
                if (loadRequest.status === WebEngineView.LoadFailedStatus) {
                    root.pageError = true
                    root.errorText = loadRequest.errorString
                } else if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
                    root.pageError = false
                }
            }
            onUrlChanged: {
                if (String(web.url).length > 0 && String(web.url) !== "about:blank")
                    addressInput.text = web.url
            }
            Component.onCompleted: {
                try {
                    if (web.profile)
                        web.profile.httpUserAgent = "Mozilla/5.0 (Linux; Android 9; Z03Ultra) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/61.0.3163.140 Mobile Safari/537.36"
                } catch (e) {
                    console.log("browser: cannot set user agent", e)
                }
                try {
                    web.newViewRequested.connect(function(request) {
                        if (request && request.requestedUrl)
                            web.url = request.requestedUrl
                    })
                } catch (e) {
                    console.log("browser: newViewRequested unavailable", e)
                }
                root.home()
            }
        }

        Rectangle {
            visible: web.loading
            height: 3
            radius: 2
            color: "#2FA98D"
            y: 1
            width: Math.max(6, contentCard.width * web.loadProgress / 100)
            x: 1
        }

        Rectangle {
            id: errorPanel
            visible: root.pageError && !web.loading
            anchors.centerIn: parent
            width: Math.min(parent.width - 60, 460)
            height: errorColumn.height + 40
            radius: 10
            color: "#FAFAFA"
            border.color: "#E0D6D6"
            border.width: 1

            Column {
                id: errorColumn
                width: parent.width - 40
                anchors.centerIn: parent
                spacing: 10
                ZybText {
                    width: parent.width
                    text: qsTr("网页加载失败")
                    color: "#B3261E"
                    font.pixelSize: 17
                    font.styleName: "Bold"
                    horizontalAlignment: Text.AlignHCenter
                }
                ZybText {
                    width: parent.width
                    text: root.errorText.length > 0 ? root.errorText : qsTr("请检查网络后重试")
                    color: "#555555"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                }
                BasicButton {
                    width: 120
                    height: 34
                    x: (parent.width - width) / 2
                    text: qsTr("重试")
                    checkable: false
                    backgroundTheme: Qt.rgba(0.19, 0.62, 0.55, 0.85)
                    onClicked: root.navigateTo(addressInput.text)
                }
            }
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
}
