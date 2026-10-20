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
    property var entries: []
    signal closeRequested()

    DeviceInfoService { id: deviceInfo }

    function refresh() {
        entries = deviceInfo.entries()
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
        text: qsTr("关于设备")
        font.styleName: "Bold"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: backButton.verticalCenter
    }
    BasicButton {
        text: qsTr("刷新")
        width: 78
        height: 36
        radius: 6
        checkable: false
        backgroundTheme: root.cardColor
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: backButton.verticalCenter
        onClicked: root.refresh()
    }

    Flickable {
        id: infoFlick
        anchors.top: backButton.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        clip: true
        contentWidth: width
        contentHeight: infoColumn.height
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: infoColumn
            width: infoFlick.width
            spacing: 8

            Repeater {
                model: root.entries
                Rectangle {
                    width: infoColumn.width
                    height: Math.max(56, valueText.height + 34)
                    radius: 8
                    color: root.cardColor
                    ZybText {
                        text: modelData.label
                        color: "#8E9099"
                        font.pixelSize: 12
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.top: parent.top
                        anchors.topMargin: 9
                    }
                    ZybText {
                        id: valueText
                        text: modelData.value
                        font.pixelSize: 14
                        wrapMode: Text.WrapAnywhere
                        width: parent.width - 28
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.top: parent.top
                        anchors.topMargin: 28
                    }
                }
            }

            ZybText {
                width: parent.width
                text: qsTr("以上信息读取自系统节点（/proc、uname、QStorageInfo），只读展示。")
                color: "#6F7178"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Component.onCompleted: root.refresh()
}
