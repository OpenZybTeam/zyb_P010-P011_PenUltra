import QtQuick 2.0
import QtQuick.Controls 2.3
import Z03Ultra 1.0
import "../ZybControls/"
import "../Style"

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0F0F0F"
    property color cardColor: Qt.rgba(60 / 255, 61 / 255, 66 / 255, 0.6)
    readonly property bool portrait: width < height
    property string engine: "qtwebengine"
    property bool cefInstalled: false
    property string cefVersion: ""
    property bool cefActive: false
    property string note: ""
    signal closeRequested()

    BrowserService { id: browserSettings }
    CefService { id: cefService }

    function refresh() {
        root.engine = browserSettings.engine()
        var info = cefService.cefInfo()
        root.cefInstalled = !!info.installed
        root.cefVersion = String(info.version ? info.version : "")
        root.cefActive = !!info.active
    }

    function apply(name) {
        browserSettings.setEngine(name)
        root.engine = browserSettings.engine()
    }

    function activateCef() {
        root.note = qsTr("正在自检 CEF 运行环境…")
        var err = cefService.enableCefMode()
        if (err) {
            root.note = err
            return
        }
        root.apply("cef")
        root.note = qsTr("CEF 内核已启用，正在重启应用…")
        cefService.restartApp()
    }

    Component.onCompleted: refresh()

    Connections {
        target: cefService
        onChanged: {
            if (cefService.state === "done") {
                root.refresh()
                root.activateCef()
            }
        }
    }

    ZybBackButton {
        id: backButton
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 10
        onBackClicked: root.closeRequested()
    }

    ZybText {
        text: qsTr("浏览器内核")
        font.styleName: "Bold"
        font.pixelSize: root.portrait ? 16 : 24
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: backButton.verticalCenter
    }

    BasicButton {
        id: refreshButton
        text: qsTr("刷新")
        width: root.portrait ? 62 : 78
        height: root.portrait ? 32 : 36
        radius: 6
        checkable: false
        backgroundTheme: root.cardColor
        anchors.right: parent.right
        anchors.rightMargin: root.portrait ? 10 : 14
        anchors.verticalCenter: backButton.verticalCenter
        onClicked: root.refresh()
    }

    Flickable {
        id: listArea
        anchors.top: backButton.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: root.portrait ? 10 : 16
        anchors.right: parent.right
        anchors.rightMargin: root.portrait ? 10 : 16
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        contentHeight: column.height
        clip: true
        flickableDirection: Flickable.VerticalFlick

        Column {
            id: column
            width: parent.width
            spacing: 8

            ZybText {
                width: parent.width
                text: qsTr("当前内核：") + (root.engine === "cef" ? "CEF (QCefView)" : "内置 QtWebEngine")
                color: "#8E9099"
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            Rectangle {
                width: parent.width
                height: builtInRow.height + 20
                radius: 10
                color: root.engine === "qtwebengine" ? "#16302A" : root.cardColor
                border.width: 1
                border.color: root.engine === "qtwebengine" ? "#2FA98D" : "#3A3B42"

                Column {
                    id: builtInRow
                    width: parent.width - 24
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    spacing: 3
                    ZybText {
                        text: qsTr("内置 QtWebEngine")
                        color: "#F2F3F5"
                        font.pixelSize: 15
                        font.styleName: "Bold"
                    }
                    ZybText {
                        width: parent.width
                        text: qsTr("Chromium 61 · 随系统提供，占用小、启动快，部分新站点兼容性一般")
                        color: "#8E9099"
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.apply("qtwebengine")
                }
            }

            Rectangle {
                width: parent.width
                height: cefColumn.height + 20
                radius: 10
                color: root.engine === "cef" ? "#16302A" : root.cardColor
                border.width: 1
                border.color: root.engine === "cef" ? "#2FA98D" : "#3A3B42"

                Column {
                    id: cefColumn
                    width: parent.width - 24
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    spacing: 5

                    Row {
                        spacing: 6
                        ZybText {
                            text: qsTr("CEF (QCefView)")
                            color: root.cefInstalled ? "#F2F3F5" : "#B9BBC2"
                            font.pixelSize: 15
                            font.styleName: "Bold"
                        }
                        ZybText {
                            text: root.cefInstalled
                                  ? qsTr("已安装 ") + (root.cefVersion.length > 0 ? root.cefVersion : "")
                                  : qsTr("未安装")
                            color: root.cefInstalled ? "#2FA98D" : "#E0A24A"
                            font.pixelSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    ZybText {
                        width: parent.width
                        visible: !cefService.busy
                        text: root.cefInstalled
                              ? qsTr("Chromium 内核 · 现代网页兼容性好，占用约 200MB 内存")
                              : qsTr("下载后可使用 Chromium 内核浏览现代网页（约 99MB）")
                        color: "#8E9099"
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }

                    ZybText {
                        width: parent.width
                        visible: cefService.busy
                        text: cefService.status
                        color: "#C9CBD2"
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }

                    Rectangle {
                        width: parent.width
                        height: 8
                        radius: 4
                        color: "#26272C"
                        visible: cefService.busy
                        Rectangle {
                            height: parent.height
                            radius: 4
                            color: "#2FA98D"
                            width: parent.width * Math.max(0, Math.min(100, cefService.progress)) / 100
                        }
                    }

                    Row {
                        spacing: 8
                        visible: !cefService.busy || cefService.state === "error"

                        BasicButton {
                            width: 96
                            height: 32
                            radius: 6
                            checkable: false
                            backgroundTheme: "#2FA98D"
                            text: root.cefInstalled ? qsTr("重新安装") : qsTr("下载安装")
                            onClicked: cefService.install()
                        }

                        BasicButton {
                            width: 84
                            height: 32
                            radius: 6
                            checkable: false
                            backgroundTheme: root.cardColor
                            text: root.cefActive ? qsTr("使用") : qsTr("启用")
                            enabled: root.cefInstalled && (root.engine !== "cef" || !root.cefActive)
                            opacity: enabled ? 1 : 0.45
                            onClicked: {
                                if (root.cefActive)
                                    root.apply("cef")
                                else
                                    root.activateCef()
                            }
                        }

                        ZybText {
                            visible: cefService.state === "error"
                            text: cefService.error
                            color: "#E46B6B"
                            font.pixelSize: 11
                            width: Math.max(0, cefColumn.width - 190)
                            anchors.verticalCenter: parent.verticalCenter
                            wrapMode: Text.Wrap
                        }
                    }

                    BasicButton {
                        width: 84
                        height: 30
                        radius: 6
                        visible: cefService.busy
                        checkable: false
                        backgroundTheme: root.cardColor
                        text: qsTr("取消")
                        onClicked: cefService.cancel()
                    }
                }
            }

            ZybText {
                width: parent.width
                text: root.note.length > 0 ? root.note
                      : qsTr("首次启用会自动自检并重启应用，自检失败会保留内置内核。安装位置 /home/user/z03plus/cef")
                color: root.note.indexOf("失败") >= 0 ? "#E46B6B" : "#6F7178"
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }
    }
}
