import QtQuick 2.0
import QtQuick.Controls 2.3
import "../ZybControls/"
import "../Style"
import Z03Ultra 1.0

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0F0F0F"
    property var apps: []
    property string status: ""
    signal closeRequested()

    AppService { id: appService }

    Component.onCompleted: { apps = appService.apps(); status = apps.length > 0 ? "" : qsTr("未找到应用，可先在应用商店安装") }

    ZybBackButton {
        id: backButton
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 10
        onBackClicked: root.closeRequested()
    }
    ZybText {
        text: qsTr("应用启动器")
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
        contentHeight: list.height + 8
        clip: true
        Column {
            id: list
            width: parent.width
            spacing: 10
            ZybText {
                width: list.width
                text: root.status.length > 0 ? root.status : qsTr("共 %1 个应用，点击启动").arg(root.apps.length)
                color: "#8E9099"
                font.pixelSize: 12
            }
            Repeater {
                model: root.apps
                Rectangle {
                    width: list.width
                    height: 62
                    radius: 12
                    color: mouse.pressed ? "#2A2E38" : "#1B1E24"
                    border.width: 1
                    border.color: "#2B2F38"
                    Image {
                        x: 14
                        width: 34
                        height: 34
                        anchors.verticalCenter: parent.verticalCenter
                        source: modelData.icon
                        fillMode: Image.PreserveAspectFit
                        visible: modelData.icon.length > 0
                    }
                    Rectangle {
                        x: 14
                        width: 34
                        height: 34
                        radius: 8
                        color: "#2FA98D"
                        visible: modelData.icon.length === 0
                        anchors.verticalCenter: parent.verticalCenter
                        ZybText {
                            anchors.centerIn: parent
                            text: modelData.name.length > 0 ? modelData.name.charAt(0) : "?"
                            color: "#0F0F0F"
                            font.pixelSize: 17
                            font.styleName: "Bold"
                        }
                    }
                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 60
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        ZybText {
                            width: parent.width
                            text: modelData.name
                            color: "#F2F3F5"
                            font.pixelSize: 15
                            elide: Text.ElideRight
                        }
                        ZybText {
                            width: parent.width
                            text: modelData.comment.length > 0 ? modelData.comment : modelData.exec
                            color: "#8E9099"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                    MouseArea {
                        id: mouse
                        anchors.fill: parent
                        onClicked: {
                            var result = appService.launch(modelData.exec)
                            root.status = result.ok ? qsTr("已启动 %1").arg(modelData.name) : result.message
                        }
                    }
                }
            }
        }
    }
}
