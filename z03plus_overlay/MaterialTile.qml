import QtQuick 2.0

Rectangle {
    id: tile
    property string text: ""
    property color accent: "#4C8DFF"
    signal clicked()
    height: 70
    radius: 12
    color: mouse.pressed ? "#2A2E38" : "#1B1E24"
    border.width: 1
    border.color: mouse.pressed ? "#4A4F5C" : "#2B2F38"

    Behavior on color { ColorAnimation { duration: 120 } }

    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: "#FFFFFF"
        opacity: mouse.pressed ? 0.06 : 0
        Behavior on opacity { NumberAnimation { duration: 140 } }
    }

    Rectangle {
        width: 3
        radius: 2
        color: tile.accent
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.top: parent.top
        anchors.topMargin: 14
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        opacity: 0.95
    }

    Text {
        text: tile.text
        color: "#F2F3F5"
        font.pixelSize: 17
        font.weight: Font.Medium
        elide: Text.ElideRight
        anchors.left: parent.left
        anchors.leftMargin: 26
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        onClicked: tile.clicked()
    }
}
