import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.0

InputPanel {
    id: panel
    anchors.left: parent.left
    anchors.right: parent.right
    y: Qt.inputMethod.visible ? parent.height - height : parent.height
    Behavior on y { NumberAnimation { duration: 150 } }
}
