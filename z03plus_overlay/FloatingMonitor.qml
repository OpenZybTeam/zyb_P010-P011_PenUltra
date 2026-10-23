import QtQuick 2.0
import Z03Ultra 1.0

Rectangle {
    id: root
    width: 182
    height: 82
    radius: 12
    color: "#F21B1E24"
    border.color: "#3A3D46"
    border.width: 1
    x: parent ? parent.width - width - 14 : 0
    y: 78
    signal closed()
    signal openRequested()

    MonitorService { id: monitor; running: root.visible }

    function barColor(value) {
        return value > 85 ? "#FF6B6B" : (value > 60 ? "#F2B34C" : "#2AC7A4")
    }

    Column {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6

        Row {
            spacing: 6
            Text { text: "CPU"; color: "#8E9099"; font.pixelSize: 10; width: 30 }
            Text {
                text: monitor.cpuUsage.toFixed(0) + "%"
                color: root.barColor(monitor.cpuUsage)
                font.pixelSize: 13
                font.bold: true
            }
        }
        Rectangle {
            width: parent.width
            height: 4
            radius: 2
            color: "#2B2F38"
            Rectangle {
                width: parent.width * Math.min(100, monitor.cpuUsage) / 100
                height: parent.height
                radius: 2
                color: root.barColor(monitor.cpuUsage)
            }
        }
        Row {
            spacing: 6
            Text { text: qsTr("内存"); color: "#8E9099"; font.pixelSize: 10; width: 30 }
            Text {
                text: monitor.memUsage.toFixed(0) + "%"
                color: root.barColor(monitor.memUsage)
                font.pixelSize: 13
                font.bold: true
            }
        }
        Rectangle {
            width: parent.width
            height: 4
            radius: 2
            color: "#2B2F38"
            Rectangle {
                width: parent.width * Math.min(100, monitor.memUsage) / 100
                height: parent.height
                radius: 2
                color: root.barColor(monitor.memUsage)
            }
        }
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent
        drag.target: root
        drag.minimumX: 0
        drag.maximumX: parent ? parent.width - root.width : 0
        drag.minimumY: 0
        drag.maximumY: parent ? parent.height - root.height : 0
        onClicked: if (!drag.active) root.openRequested()
    }

    Rectangle {
        width: 22
        height: 22
        radius: 11
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 4
        color: closeMouse.pressed ? "#4A4D57" : "#2B2F38"
        Text { anchors.centerIn: parent; text: "×"; color: "#C8CAD1"; font.pixelSize: 13 }
        MouseArea {
            id: closeMouse
            anchors.fill: parent
            onClicked: root.closed()
        }
    }
}
