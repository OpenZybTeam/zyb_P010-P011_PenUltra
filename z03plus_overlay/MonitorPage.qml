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
    property bool floating: false
    signal closeRequested()

    MonitorService { id: monitor; running: true }

    function barColor(value) {
        return value > 85 ? "#FF6B6B" : (value > 60 ? "#F2B34C" : "#2AC7A4")
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
        text: qsTr("资源监视")
        font.styleName: "Bold"
        font.pixelSize: 21
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: backButton.verticalCenter
    }

    Rectangle {
        id: floatingButton
        width: 110
        height: 34
        radius: 17
        anchors.verticalCenter: backButton.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 14
        color: floatingMouse.pressed ? "#3B3D45" : "#26272C"
        border.color: root.floating ? "#2AC7A4" : "#3C3E46"
        border.width: 1
        Text {
            anchors.centerIn: parent
            text: root.floating ? qsTr("浮窗：开") : qsTr("浮窗：关")
            color: root.floating ? "#2AC7A4" : "#EDEDED"
            font.pixelSize: 14
        }
        MouseArea {
            id: floatingMouse
            anchors.fill: parent
            onClicked: root.floating = !root.floating
        }
    }

    Grid {
        id: cards
        anchors.top: backButton.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.right: parent.right
        anchors.rightMargin: 16
        columns: width > 560 ? 4 : 2
        spacing: 10
        property real cellWidth: (width - spacing * (columns - 1)) / columns

        Rectangle {
            width: cards.cellWidth
            height: 74
            radius: 12
            color: "#1B1E24"
            border.color: "#2B2F38"
            border.width: 1
            ZybText { x: 12; y: 9; text: qsTr("CPU 占用"); color: "#8E9099"; font.pixelSize: 11 }
            ZybText {
                x: 12; y: 26; text: monitor.cpuUsage.toFixed(0) + "%"
                color: root.barColor(monitor.cpuUsage); font.pixelSize: 24; font.styleName: "Bold"
            }
            Rectangle {
                x: 12; y: 58; width: parent.width - 24; height: 5; radius: 3; color: "#2B2F38"
                Rectangle {
                    width: (parent.width) * Math.min(100, monitor.cpuUsage) / 100
                    height: parent.height; radius: 3; color: root.barColor(monitor.cpuUsage)
                }
            }
        }

        Rectangle {
            width: cards.cellWidth
            height: 74
            radius: 12
            color: "#1B1E24"
            border.color: "#2B2F38"
            border.width: 1
            ZybText { x: 12; y: 9; text: qsTr("内存占用"); color: "#8E9099"; font.pixelSize: 11 }
            ZybText {
                x: 12; y: 26; text: monitor.memUsage.toFixed(0) + "%"
                color: root.barColor(monitor.memUsage); font.pixelSize: 24; font.styleName: "Bold"
            }
            ZybText {
                anchors.right: parent.right; anchors.rightMargin: 12; y: 32
                text: monitor.memUsedMB + " / " + monitor.memTotalMB + " MB"
                color: "#8E9099"; font.pixelSize: 11
            }
            Rectangle {
                x: 12; y: 58; width: parent.width - 24; height: 5; radius: 3; color: "#2B2F38"
                Rectangle {
                    width: (parent.width) * Math.min(100, monitor.memUsage) / 100
                    height: parent.height; radius: 3; color: root.barColor(monitor.memUsage)
                }
            }
        }

        Rectangle {
            width: cards.cellWidth
            height: 74
            radius: 12
            color: "#1B1E24"
            border.color: "#2B2F38"
            border.width: 1
            ZybText { x: 12; y: 9; text: qsTr("系统负载"); color: "#8E9099"; font.pixelSize: 11 }
            ZybText {
                x: 12; y: 30; text: monitor.loadAverage.length > 0 ? monitor.loadAverage : "--"
                color: "#F2F3F5"; font.pixelSize: 17
            }
        }

        Rectangle {
            width: cards.cellWidth
            height: 74
            radius: 12
            color: "#1B1E24"
            border.color: "#2B2F38"
            border.width: 1
            ZybText { x: 12; y: 9; text: qsTr("运行时间"); color: "#8E9099"; font.pixelSize: 11 }
            ZybText {
                x: 12; y: 30; text: monitor.uptimeText.length > 0 ? monitor.uptimeText : "--"
                color: "#F2F3F5"; font.pixelSize: 17
            }
        }
    }

    Rectangle {
        id: listHeader
        anchors.top: cards.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.right: parent.right
        anchors.rightMargin: 16
        height: 26
        color: "transparent"
        ZybText { x: 4; text: qsTr("进程"); color: "#6F7178"; font.pixelSize: 11 }
        ZybText { x: parent.width - 152; text: qsTr("PID"); color: "#6F7178"; font.pixelSize: 11 }
        ZybText { x: parent.width - 96; text: qsTr("CPU"); color: "#6F7178"; font.pixelSize: 11 }
        ZybText { x: parent.width - 58; text: qsTr("内存"); color: "#6F7178"; font.pixelSize: 11 }
    }

    ListView {
        anchors.top: listHeader.bottom
        anchors.topMargin: 4
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        clip: true
        model: monitor.processes
        spacing: 2
        delegate: Rectangle {
            width: ListView.view.width
            height: 34
            radius: 7
            color: "#1B1E24"
            ZybText {
                x: 10
                width: parent.width - 210
                anchors.verticalCenter: parent.verticalCenter
                text: modelData.name
                color: "#F2F3F5"
                font.pixelSize: 13
                elide: Text.ElideRight
            }
            ZybText {
                x: parent.width - 152
                width: 52
                anchors.verticalCenter: parent.verticalCenter
                text: modelData.pid
                color: "#8E9099"
                font.pixelSize: 12
            }
            ZybText {
                x: parent.width - 96
                width: 34
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignRight
                text: modelData.cpu.toFixed(0) + "%"
                color: root.barColor(modelData.cpu)
                font.pixelSize: 12
            }
            ZybText {
                x: parent.width - 56
                width: 46
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignRight
                text: modelData.mem + "M"
                color: "#8E9099"
                font.pixelSize: 12
            }
        }
    }
}
