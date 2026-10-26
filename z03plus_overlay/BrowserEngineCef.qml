import QtQuick 2.0
import QCefView 1.0

// CEF engine backed by QCefView. The QCefView QML plugin ships as an optional
// component; when it is missing the Loader in BrowserPage.qml reports an error
// and the page offers to switch back to the built-in engine.
Item {
    id: root
    property string url: ""
    property string title: ""
    property bool loading: false
    property int loadProgress: 0
    property bool canGoBack: false
    property bool canGoForward: false
    property string lastError: ""

    signal loadSucceeded()
    signal loadFailed(string message)
    signal newWindowRequested(string target)

    function navigate(target) {
        root.url = target
        root.loading = true
        root.loadProgress = 0
        cef.url = target
    }

    function loadHtmlPage(html, baseUrl) {
        root.navigate("data:text/html;charset=utf-8," + encodeURIComponent(html))
    }

    function setUserAgent(ua) {
        root.userAgent = ua
    }

    function reload() {
        cef.reload()
    }

    function stop() {
        cef.stop()
    }

    function goBack() {
        if (cef.canGoBack)
            cef.back()
    }

    function goForward() {
        if (cef.canGoForward)
            cef.forward()
    }

    property string userAgent: ""

    QCefView {
        id: cef
        anchors.fill: parent

        onLoadingStateChanged: {
            root.loading = isLoading
            root.canGoBack = canGoBack
            root.canGoForward = canGoForward
            root.url = url
        }

        onLoadUrlFinished: {
            root.loading = false
            root.loadProgress = 100
            if (isSuccess) {
                root.title = cef.title
                root.loadSucceeded()
            } else {
                root.lastError = errorMsg
                root.loadFailed(errorMsg)
            }
        }

        onUrlChanged: root.url = url
        onTitleChanged: root.title = title
    }
}
