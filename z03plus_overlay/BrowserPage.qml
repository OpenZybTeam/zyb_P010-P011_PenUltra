import QtQuick 2.0
import QtQuick.Controls 2.3
import Z03Ultra 1.0
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
    property bool desktopUA: false
    readonly property bool portrait: width < height
    property string engine: "qtwebengine"
    property var view: null
    property bool engineError: false
    property string engineErrorText: ""
    property bool engineReady: false
    property string pendingEngineUrl: ""
    signal closeRequested()

    BrowserService { id: browserSettings }

    Component.onCompleted: {
        root.engine = browserSettings.engine()
        if (root.engine === "cef" && !browserSettings.ensureCefPlugin())
            root.engineErrorText = browserSettings.cefPluginError()
        root.engineReady = true
    }

    function currentUrl() { return (root.view && root.view.url) ? String(root.view.url) : "" }
    function pageTitle() { return (root.view && root.view.title) ? String(root.view.title) : "" }
    function isLoading() { return root.view ? !!root.view.loading : false }
    function loadPercent() { return root.view ? Number(root.view.loadProgress || 0) : 0 }
    function reloadPage() { if (root.view && root.view.reload) root.view.reload() }
    function stopLoading() { if (root.view && root.view.stop) root.view.stop() }
    function commandLoadHtml(html, base) {
        if (root.view && root.view.loadHtmlPage) root.view.loadHtmlPage(html, base)
    }
    function syncAddress() {
        var u = root.currentUrl()
        if (u.length > 0 && u !== "about:blank")
            addressInput.text = u
    }
    function setEngine(name) {
        if (root.engine === name) return
        var keep = root.currentUrl()
        root.pendingEngineUrl = (keep.length > 0 && keep !== "about:blank") ? keep : ""
        root.view = null
        root.engineError = false
        root.pageError = false
        root.engine = name
        addressInput.text = ""
        if (browserSettings) browserSettings.setEngine(name)
    }

    function showKeyboard() {
        Qt.inputMethod.show()
    }

    function hideKeyboard() {
        addressInput.focus = false
        Qt.inputMethod.hide()
    }

    function applyUserAgent() {
        var ua = desktopUA
            ? "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/61.0.3163.140 Safari/537.36"
            : "Mozilla/5.0 (Linux; Android 9; Z03Ultra) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/61.0.3163.140 Mobile Safari/537.36"
        if (root.view && root.view.setUserAgent)
            root.view.setUserAgent(ua)
    }

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
        if (root.currentUrl() === target) {
            root.reloadPage()
        } else if (root.view && root.view.navigate) {
            root.view.navigate(target)
        }
    }

    function go() {
        var text = addressInput.text.trim()
        if (text.length === 0) {
            addressInput.forceActiveFocus()
            root.showKeyboard()
            return
        }
        typedUrl = text
        root.hideKeyboard()
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
        root.commandLoadHtml(
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
            if (root.isLoading())
                root.stopLoading()
            root.hideKeyboard()
            root.closeRequested()
        }
    }
    ZybText {
        id: titleLabel
        text: root.pageTitle().length > 0 ? root.pageTitle() : qsTr("网页浏览")
        font.styleName: "Bold"
        font.pixelSize: 14
        elide: Text.ElideRight
        width: Math.max(40, Math.min(implicitWidth, parent.width - (root.portrait ? 150 : 320)))
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: backButton.verticalCenter
    }
    ZybText {
        text: root.isLoading() ? (root.loadPercent() + "%")
                               : (root.pageError ? qsTr("加载失败")
                                                 : (root.statusText.length > 0 ? root.statusText : ""))
        color: root.isLoading() ? "#E0A24A" : (root.pageError ? "#E05A5A" : "#8E9099")
        font.pixelSize: 12
        elide: Text.ElideRight
        width: root.portrait ? 70 : 220
        visible: !root.portrait
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
            width: root.portrait ? 34 : 42
            height: 32
            radius: 6
            checkable: false
            enabled: root.view ? !!root.view.canGoBack : false
            backgroundTheme: Qt.rgba(0.25, 0.26, 0.29, 0.9)
            anchors.left: parent.left
            anchors.leftMargin: 5
            anchors.verticalCenter: parent.verticalCenter
            onClicked: { if (root.view && root.view.goBack) root.view.goBack() }
        }

        BasicButton {
            id: historyForward
            text: "→"
            visible: !root.portrait
            width: root.portrait ? 34 : 42
            height: 32
            radius: 6
            checkable: false
            enabled: root.view ? !!root.view.canGoForward : false
            backgroundTheme: Qt.rgba(0.25, 0.26, 0.29, 0.9)
            anchors.left: historyBack.right
            anchors.leftMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            onClicked: { if (root.view && root.view.goForward) root.view.goForward() }
        }

        TextInput {
            id: addressInput
            anchors.left: root.portrait ? historyBack.right : historyForward.right
            anchors.leftMargin: root.portrait ? 6 : 8
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
                root.showKeyboard()
            }
        }
        BasicButton {
            id: goButton
            text: root.isLoading() ? qsTr("停止") : qsTr("打开")
            width: root.portrait ? 52 : 66
            height: 32
            radius: 6
            checkable: false
            backgroundTheme: root.isLoading() ? Qt.rgba(0.7, 0.18, 0.18, 0.8)
                                         : Qt.rgba(0.19, 0.62, 0.55, 0.8)
            anchors.right: root.portrait ? parent.right : refreshButton.left
            anchors.rightMargin: root.portrait ? 5 : 6
            anchors.verticalCenter: parent.verticalCenter
            onClicked: {
                if (root.isLoading())
                    root.stopLoading()
                else
                    root.go()
            }
        }
        BasicButton {
            id: refreshButton
            text: qsTr("刷新")
            visible: !root.portrait
            width: root.portrait ? 44 : 60
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
                    root.reloadPage()
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
                Rectangle {
                    width: uaLabel.implicitWidth + 18
                    height: 26
                    radius: 13
                    color: root.desktopUA ? "#1B2E28" : "#1E1F24"
                    border.color: root.desktopUA ? "#2FA98D" : "#3A3B42"
                    border.width: 1
                    Text {
                        id: uaLabel
                        anchors.centerIn: parent
                        text: root.desktopUA ? qsTr("UA: 桌面") : qsTr("UA: 移动")
                        color: root.desktopUA ? "#7FE0C8" : "#CCCCCC"
                        font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            root.desktopUA = !root.desktopUA
                            root.applyUserAgent()
                            if (root.currentUrl().length > 0 && root.currentUrl() !== "about:blank")
                                root.reloadPage()
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

        Loader {
            id: engineLoader
            anchors.fill: parent
            anchors.margins: 1
            visible: status === Loader.Ready && !root.engineError
            source: root.engineReady ? (root.engine === "cef" ? "BrowserEngineCef.qml" : "BrowserEngineWeb.qml") : ""
            onLoaded: {
                root.view = item
                if (!item) return
                item.loadSucceeded.connect(function() {
                    root.pageError = false
                    root.syncAddress()
                })
                item.loadFailed.connect(function(message) {
                    root.pageError = true
                    root.errorText = message
                })
                item.urlChanged.connect(function() { root.syncAddress() })
                item.newWindowRequested.connect(function(target) {
                    if (String(target).length > 0) root.navigateTo(target)
                })
                root.applyUserAgent()
                if (root.pendingEngineUrl.length > 0) {
                    var target = root.pendingEngineUrl
                    root.pendingEngineUrl = ""
                    root.navigateTo(target)
                } else {
                    root.home()
                }
            }
            onStatusChanged: {
                if (status === Loader.Loading) {
                    root.view = null
                } else if (status === Loader.Error) {
                    root.view = null
                    root.engineError = true
                    root.pageError = false
                    if (root.engineErrorText.length === 0)
                        root.engineErrorText = browserSettings.cefPluginError()
                } else if (status === Loader.Ready) {
                    root.engineError = false
                }
            }
        }

        Rectangle {
            id: engineMissingPanel
            visible: root.engineError
            anchors.fill: parent
            color: "#FAFAFA"
            Column {
                width: Math.min(parent.width - 40, 420)
                anchors.centerIn: parent
                spacing: 12
                ZybText {
                    width: parent.width
                    text: qsTr("CEF 引擎不可用")
                    color: "#B3261E"
                    font.pixelSize: 17
                    font.styleName: "Bold"
                    horizontalAlignment: Text.AlignHCenter
                }
                ZybText {
                    width: parent.width
                    text: root.engineErrorText.length > 0
                          ? root.engineErrorText
                          : qsTr("未找到 QCefView 组件（libcef.so 与 QML 插件）。请安装 CEF 组件，或切回内置引擎。")
                    color: "#555555"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                }
                BasicButton {
                    width: 150
                    height: 34
                    x: (parent.width - width) / 2
                    text: qsTr("切回内置引擎")
                    checkable: false
                    backgroundTheme: Qt.rgba(0.19, 0.62, 0.55, 0.85)
                    onClicked: root.setEngine("qtwebengine")
                }
            }
        }

        Rectangle {
            visible: root.isLoading()
            height: 3
            radius: 2
            color: "#2FA98D"
            y: 1
            width: Math.max(6, contentCard.width * root.loadPercent() / 100)
            x: 1
        }

        Rectangle {
            id: errorPanel
            visible: root.pageError && !root.isLoading()
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

}
