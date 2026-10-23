import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.0

Item {
    id: holder
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    height: column.height

    function closeKeyboard() {
        var focused = holder.Window.activeFocusItem
        if (focused && focused !== holder && focused.hasOwnProperty("focus"))
            focused.focus = false
        Qt.inputMethod.hide()
        if (holder.parent)
            holder.parent.forceActiveFocus()
    }

    Column {
        id: column
        width: holder.width

        Rectangle {
            id: bar
            width: column.width
            height: Qt.inputMethod.visible ? 34 : 0
            visible: height > 0
            color: "#1B1E24"
            border.color: "#2B2F38"
            border.width: 1
            Text {
                anchors.centerIn: parent
                text: qsTr("收起键盘")
                color: "#D8D9DD"
                font.pixelSize: 13
            }
            MouseArea {
                anchors.fill: parent
                onClicked: holder.closeKeyboard()
            }
        }

        InputPanel {
            id: panel
            width: column.width
        }
    }
}
