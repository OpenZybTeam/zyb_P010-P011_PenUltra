import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.0

InputPanel {
    id: panel
    z: 300
    width: parent ? parent.width : 0
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottom: parent.bottom
}
