import QtQuick 2.0
import QtQuick.Controls 2.3
import "../ZybControls/"
import "../Style"
import Z03Ultra 1.0

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0F0F0F"
    property color cardColor: Qt.rgba(60 / 255, 61, 66 / 255, 0.6)
    property var files: []
    property string status: ""
    signal closeRequested()

    TransferService { id: transfer }
    Timer {
        interval: 3000
        running: transfer.running
        repeat: true
        onTriggered: root.files = transfer.listFiles()
    }

    function refresh() { files = transfer.listFiles() }
    function toggle() {
        if (transfer.running) {
            transfer.stop()
            status = qsTr("互传已关闭")
        } else {
            transfer.start()
            status = transfer.running ? qsTr("已开启，手机与设备需在同一 Wi-Fi") : transfer.status
            refresh()
        }
    }
    function human(bytes) {
        if (bytes >= 1048576)
            return (bytes / 1048576).toFixed(1) + " MB"
        if (bytes >= 1024)
            return (bytes / 1024).toFixed(1) + " KB"
        return bytes + " B"
    }

    Component.onCompleted: { refresh(); if (transfer.running) transfer.stop() }

    ZybBackButton {
        id: backButton
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 10
        onBackClicked: { transfer.stop(); root.closeRequested() }
    }
    ZybText {
        text: qsTr("手机互传")
        font.styleName: "Bold"
        font.pixelSize: 17
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: backButton.verticalCenter
    }

    Flickable {
        anchors.top: backButton.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        contentWidth: width
        contentHeight: body.height
        clip: true
        Column {
            id: body
            width: parent.width
            spacing: 12

            Rectangle {
                width: parent.width
                height: 132
                radius: 12
                color: "#171A21"
                border.width: 1
                border.color: transfer.running ? "#2FA98D" : "#2B2F38"
                Column {
                    anchors.centerIn: parent
                    spacing: 8
                    ZybText {
                        width: body.width - 60
                        horizontalAlignment: Text.AlignHCenter
                        text: transfer.running ? transfer.url : qsTr("开启后在手机浏览器访问")
                        color: transfer.running ? "#7FE0C8" : "#8E9099"
                        font.pixelSize: transfer.running ? 20 : 13
                        font.styleName: transfer.running ? "Bold" : "Regular"
                    }
                    ZybText {
                        width: body.width - 60
                        horizontalAlignment: Text.AlignHCenter
                        text: transfer.running ? qsTr("上传目录 /home/user/z03plus/transfer")
                                                     : qsTr("同一 Wi-Fi 下即可上传和下载文件")
                        color: "#8E9099"
                        font.pixelSize: 11
                    }
                }
            }

            BasicButton {
                width: parent.width
                height: 42
                text: transfer.running ? qsTr("停止互传") : qsTr("开启互传")
                checkable: false
                backgroundTheme: transfer.running ? Qt.rgba(0.7, 0.18, 0.18, 0.85) : Qt.rgba(0.19, 0.62, 0.55, 0.85)
                onClicked: root.toggle()
            }

            ZybText {
                width: parent.width
                text: status.length > 0 ? status : qsTr("共 %1 个文件").arg(files.length)
                color: "#8E9099"
                font.pixelSize: 12
            }

            Repeater {
                model: files
                Rectangle {
                    width: body.width
                    height: 56
                    radius: 10
                    color: "#1B1E24"
                    border.width: 1
                    border.color: "#2B2F38"
                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        ZybText { text: modelData.name; color: "#F2F3F5"; font.pixelSize: 14 }
                        ZybText { text: root.human(modelData.size) + " · " + modelData.modified; color: "#8E9099"; font.pixelSize: 11 }
                    }
                    Rectangle {
                        width: 54
                        height: 30
                        radius: 8
                        color: "#2A2E38"
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        ZybText { anchors.centerIn: parent; text: qsTr("删除"); color: "#FF9A9A"; font.pixelSize: 12 }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: { transfer.removeFile(modelData.name); root.refresh() }
                        }
                    }
                }
            }
        }
    }
}
