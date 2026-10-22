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
    property string status: ""
    property bool confirmPower: false
    property string powerAction: ""
    signal closeRequested()

    SystemService { id: sys }

    ZybBackButton {
        id: backButton
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 10
        onBackClicked: root.closeRequested()
    }
    ZybText {
        text: qsTr("系统控制")
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
                height: 92
                radius: 12
                color: "#171A21"
                border.width: 1
                border.color: "#2B2F38"
                Row {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 26
                    Column {
                        spacing: 4
                        ZybText { text: qsTr("电量"); color: "#8E9099"; font.pixelSize: 11 }
                        ZybText {
                            text: sys.battery + "%"
                            color: sys.battery > 30 ? "#7FE0C8" : "#FFB454"
                            font.pixelSize: 26
                            font.styleName: "Bold"
                        }
                        ZybText { text: sys.batteryStatus; color: "#8E9099"; font.pixelSize: 11 }
                    }
                    Column {
                        spacing: 4
                        ZybText { text: qsTr("温度"); color: "#8E9099"; font.pixelSize: 11 }
                        ZybText { text: sys.cpuTemp; color: "#F2F3F5"; font.pixelSize: 20 }
                        ZybText { text: sys.batteryDetail; color: "#8E9099"; font.pixelSize: 11 }
                    }
                    Column {
                        spacing: 4
                        ZybText { text: qsTr("内存"); color: "#8E9099"; font.pixelSize: 11 }
                        ZybText { text: sys.memoryText; color: "#F2F3F5"; font.pixelSize: 20 }
                        ZybText { text: sys.governor.length > 0 ? sys.governor : qsTr("调频未知"); color: "#8E9099"; font.pixelSize: 11 }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 78
                radius: 12
                color: "#1B1E24"
                border.width: 1
                border.color: "#2B2F38"
                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 8
                    Row {
                        width: parent.width
                        ZybText { text: qsTr("屏幕亮度"); color: "#F2F3F5"; font.pixelSize: 13 }
                        Item { width: parent.width - 130; height: 1 }
                        ZybText {
                            text: sys.maxBrightness > 0 ? Math.round(sys.brightness * 100 / sys.maxBrightness) + "%" : "--"
                            color: "#8E9099"
                            font.pixelSize: 12
                        }
                    }
                    Rectangle {
                        id: brightTrack
                        width: parent.width
                        height: 8
                        radius: 4
                        color: "#2A2E38"
                        Rectangle {
                            width: parent.width * (sys.maxBrightness > 0 ? sys.brightness / sys.maxBrightness : 0)
                            height: parent.height
                            radius: 4
                            color: "#2FA98D"
                        }
                        MouseArea {
                            anchors.fill: parent
                            onPressed: root.setBrightnessAt(mouse.x)
                            onPositionChanged: if (pressed) root.setBrightnessAt(mouse.x)
                        }
                    }
                }
            }

            Rectangle {
                visible: sys.volumeAvailable
                width: parent.width
                height: 78
                radius: 12
                color: "#1B1E24"
                border.width: 1
                border.color: "#2B2F38"
                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 8
                    Row {
                        width: parent.width
                        ZybText { text: qsTr("媒体音量"); color: "#F2F3F5"; font.pixelSize: 13 }
                        Item { width: parent.width - 130; height: 1 }
                        ZybText { text: sys.volume + "%"; color: "#8E9099"; font.pixelSize: 12 }
                    }
                    Rectangle {
                        id: volTrack
                        width: parent.width
                        height: 8
                        radius: 4
                        color: "#2A2E38"
                        Rectangle {
                            width: parent.width * sys.volume / 100
                            height: parent.height
                            radius: 4
                            color: "#4C8DFF"
                        }
                        MouseArea {
                            anchors.fill: parent
                            onPressed: root.setVolumeAt(mouse.x)
                            onPositionChanged: if (pressed) root.setVolumeAt(mouse.x)
                        }
                    }
                }
            }

            Column {
                width: parent.width
                spacing: 8
                ZybText { text: qsTr("性能模式"); color: "#8E9099"; font.pixelSize: 12 }
                Row {
                    spacing: 8
                    Repeater {
                        model: sys.governors
                        Rectangle {
                            width: Math.max(66, label.implicitWidth + 22)
                            height: 34
                            radius: 9
                            color: sys.governor === modelData ? "#2FA98D" : "#1B1E24"
                            border.width: 1
                            border.color: "#2B2F38"
                            ZybText {
                                id: label
                                anchors.centerIn: parent
                                text: modelData
                                color: sys.governor === modelData ? "#0F0F0F" : "#CCCCCC"
                                font.pixelSize: 12
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (sys.setGovernor(modelData))
                                        root.status = qsTr("已切换到 ") + modelData
                                    else
                                        root.status = qsTr("无法切换，可能缺少权限")
                                }
                            }
                        }
                    }
                    ZybText {
                        visible: sys.governors.length === 0
                        text: qsTr("未检测到调频接口")
                        color: "#8E9099"
                        font.pixelSize: 12
                    }
                }
            }

            Row {
                width: parent.width
                spacing: 10
                BasicButton {
                    width: (parent.width - 10) / 2
                    height: 42
                    text: qsTr("重启设备")
                    checkable: false
                    backgroundTheme: Qt.rgba(0.7, 0.45, 0.18, 0.85)
                    onClicked: { root.powerAction = "reboot"; root.confirmPower = true }
                }
                BasicButton {
                    width: (parent.width - 10) / 2
                    height: 42
                    text: qsTr("关机")
                    checkable: false
                    backgroundTheme: Qt.rgba(0.7, 0.18, 0.18, 0.85)
                    onClicked: { root.powerAction = "poweroff"; root.confirmPower = true }
                }
            }

            ZybText {
                width: parent.width
                text: root.status.length > 0 ? root.status : qsTr("拖动滑块即可调整，修改立即生效")
                color: "#8E9099"
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }
    }

    function setBrightnessAt(x) {
        var ratio = Math.max(0, Math.min(1, x / brightTrack.width))
        sys.setBrightness(Math.max(1, Math.round(sys.maxBrightness * ratio)))
    }
    function setVolumeAt(x) {
        var ratio = Math.max(0, Math.min(1, x / volTrack.width))
        sys.setVolume(Math.round(ratio * 100))
    }

    Rectangle {
        visible: root.confirmPower
        anchors.fill: parent
        color: "#CC000000"
        z: 50
        MouseArea { anchors.fill: parent }
        Rectangle {
            anchors.centerIn: parent
            width: 300
            height: 150
            radius: 14
            color: "#171A21"
            border.width: 1
            border.color: "#2B2F38"
            Column {
                anchors.centerIn: parent
                spacing: 14
                ZybText {
                    width: 260
                    horizontalAlignment: Text.AlignHCenter
                    text: root.powerAction === "reboot" ? qsTr("确定重启设备？") : qsTr("确定关机？")
                    color: "#F2F3F5"
                    font.pixelSize: 16
                }
                Row {
                    spacing: 10
                    BasicButton {
                        width: 110
                        height: 36
                        text: qsTr("取消")
                        checkable: false
                        backgroundTheme: root.cardColor
                        onClicked: root.confirmPower = false
                    }
                    BasicButton {
                        width: 110
                        height: 36
                        text: qsTr("确定")
                        checkable: false
                        backgroundTheme: Qt.rgba(0.7, 0.18, 0.18, 0.85)
                        onClicked: {
                            root.confirmPower = false
                            if (root.powerAction === "reboot")
                                sys.reboot()
                            else
                                sys.poweroff()
                        }
                    }
                }
            }
        }
    }
}
