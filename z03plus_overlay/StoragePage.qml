import QtQuick 2.0
import QtQuick.Controls 2.3
import "../ZybControls/"
import "../Style"
import Z03Ultra 1.0

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0F0F0F"
    property color cardColor: Qt.rgba(60 / 255, 61 / 255, 66 / 255, 0.6)
    property var partitions: []
    property string cacheSummary: ""
    property string status: ""
    signal closeRequested()

    StorageService { id: storage }

    function human(bytes) {
        if (bytes >= 1073741824)
            return (bytes / 1073741824).toFixed(2) + " GB"
        if (bytes >= 1048576)
            return (bytes / 1048576).toFixed(1) + " MB"
        if (bytes >= 1024)
            return (bytes / 1024).toFixed(1) + " KB"
        return bytes + " B"
    }

    function refresh() {
        partitions = storage.partitions()
        var info = storage.cacheInfo()
        var total = 0
        var files = 0
        for (var i = 0; i < info.length; i++) {
            total += info[i].bytes
            files += info[i].files
        }
        cacheSummary = files === 0 ? qsTr("暂无可清理缓存")
                                  : qsTr("%1 个文件 · %2").arg(files).arg(human(total))
    }

    function clearCache() {
        var result = storage.clearCache()
        status = result.ok ? qsTr("已清理缓存，释放 %1").arg(result.freed)
                           : qsTr("部分文件清理失败，已释放 %1").arg(result.freed)
        refresh()
        statusTimer.restart()
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
        text: qsTr("存储空间")
        font.styleName: "Bold"
        font.pixelSize: parent.width < parent.height ? 16 : 28
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: backButton.verticalCenter
    }
    BasicButton {
        id: refreshButton
        text: qsTr("刷新")
        width: 62
        height: 32
        radius: 6
        checkable: false
        backgroundTheme: root.cardColor
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: backButton.verticalCenter
        onClicked: root.refresh()
    }

    Flickable {
        id: storageFlick
        anchors.top: backButton.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusText.top
        anchors.bottomMargin: 6
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        clip: true
        contentWidth: width
        contentHeight: storageColumn.height + 12
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: storageColumn
            width: storageFlick.width
            spacing: 10

            Rectangle {
                width: parent.width
                height: 104
                radius: 8
                color: root.cardColor
                ZybText {
                    text: qsTr("缓存")
                    font.styleName: "Bold"
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.top: parent.top
                    anchors.topMargin: 12
                }
                ZybText {
                    text: root.cacheSummary
                    color: "#B8B8B8"
                    font.pixelSize: 14
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.top: parent.top
                    anchors.topMargin: 42
                }
                BasicButton {
                    text: qsTr("清理缓存")
                    width: 118
                    height: 42
                    radius: 7
                    checkable: false
                    backgroundTheme: Qt.rgba(0.7, 0.18, 0.18, 0.7)
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 14
                    onClicked: root.clearCache()
                }
            }

            Repeater {
                model: root.partitions
                Rectangle {
                    width: storageColumn.width
                    height: 100
                    radius: 8
                    color: root.cardColor
                    ZybText {
                        id: mountText
                        text: modelData.mount
                        font.styleName: "Bold"
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.top: parent.top
                        anchors.topMargin: 10
                    }
                    ZybText {
                        text: modelData.fs + (modelData.readOnly ? qsTr(" · 只读") : "")
                        color: "#8E9099"
                        font.pixelSize: 12
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: mountText.verticalCenter
                    }
                    Rectangle {
                        id: usageBar
                        width: parent.width - 28
                        height: 9
                        radius: 4
                        color: "#151619"
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.top: parent.top
                        anchors.topMargin: 42
                        Rectangle {
                            width: parent.width * Math.min(100, modelData.percent) / 100
                            height: parent.height
                            radius: 4
                            color: modelData.percent > 90 ? "#E05A5A" : "#31C7A5"
                        }
                    }
                    ZybText {
                        text: qsTr("已用 ") + modelData.used + " / " + modelData.total +
                              qsTr(" · 剩余 ") + modelData.free + " · " + modelData.percent + "%"
                        color: "#B8B8B8"
                        font.pixelSize: 13
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.top: parent.top
                        anchors.topMargin: 62
                    }
                    ZybText {
                        text: modelData.device
                        visible: modelData.device.length > 0
                        color: "#6F7178"
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.top: parent.top
                        anchors.topMargin: 84
                    }
                }
            }
        }
    }

    ZybText {
        id: statusText
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        text: root.status
        color: "#31C7A5"
        horizontalAlignment: Text.AlignHCenter
    }

    Timer {
        id: statusTimer
        interval: 3000
        onTriggered: root.status = ""
    }

    Component.onCompleted: root.refresh()
}
