import QtQuick 2.0
import QtWebEngine 1.4

// Built-in engine: QtWebEngine 1.4 (Chromium 61 on this device).
// Exposes the same surface as BrowserEngineCef.qml so BrowserPage.qml can swap
// engines without branching on every property access.
WebEngineView {
    id: engine
    backgroundColor: "#FFFFFF"
    zoomFactor: 1.0

    signal loadSucceeded()
    signal loadFailed(string message)
    signal newWindowRequested(string target)

    function navigate(target) {
        engine.url = target
    }

    function loadHtmlPage(html, baseUrl) {
        engine.loadHtml(html, baseUrl)
    }

    function setUserAgent(ua) {
        try {
            if (engine.profile)
                engine.profile.httpUserAgent = ua
        } catch (e) {
            console.log("browser: cannot set user agent", e)
        }
    }

    onLoadingChanged: {
        if (loadRequest.status === WebEngineView.LoadFailedStatus)
            engine.loadFailed(loadRequest.errorString)
        else if (loadRequest.status === WebEngineView.LoadSucceededStatus)
            engine.loadSucceeded()
    }

    Component.onCompleted: {
        try {
            engine.newViewRequested.connect(function(request) {
                if (request && request.requestedUrl)
                    engine.newWindowRequested(request.requestedUrl)
            })
        } catch (e) {
            console.log("browser: newViewRequested unavailable", e)
        }
    }
}
