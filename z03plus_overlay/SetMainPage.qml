import QtQuick 2.0
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import "../Funcs/UILogics.js" as UILogic
import "../ZybControls/"
import "../../"
import "../Style"
import BtManager.BtStatusDefine 1.0

Item {
    id: storeUI
    anchors.fill: parent
    property color thisbgColor: Qt.rgba(60/255, 61/255, 66/255, 0.6)
    property var pageType: "setpage"
    signal backClicked()

    ListModel {
        id: setModel
        ListElement { name: qsTr("WLAN"); icon: "qrc:/images/set_main_wifi.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/SetWifiPage.qml" }
        ListElement { name: qsTr("蜂窝"); icon: "qrc:/images/mobile42x42.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/Set4GPage.qml"}
        ListElement { name: qsTr("蓝牙"); icon: "qrc:/images/bt42x42.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/SetBtPage.qml"}
        ListElement { name: qsTr("连接打印机"); icon: "qrc:/images/print42x42.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/SetMMPrintPage.qml" }
        ListElement { name: qsTr("会员中心"); icon: "qrc:/images/vip.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/SetVipPage.qml"}
        ListElement { name: qsTr("词典"); icon: "qrc:/images/dict42x42.png" ; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/SetDictPage.qml"}
        ListElement { name: qsTr("显示"); icon: "qrc:/images/light42x42.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/SetLightPage.qml"}
        ListElement { name: qsTr("音量"); icon: "qrc:/images/sound42x42.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/SetVolumnPage.qml"}
        ListElement { name: qsTr("发音"); icon: "qrc:/images/speak42x42.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/SetSpeekPage.qml"}
        ListElement { name: qsTr("语速"); icon: "qrc:/images/set_tts_speed.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/SetTtsSpeedPage.qml"}
        ListElement { name: qsTr("多行扫描"); icon: "qrc:/images/mutiline42x42.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/SetMutiLinePage.qml" }
        ListElement { name: qsTr("左右手切换"); icon: "qrc:/images/handsel42x42.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/SetHandDirectPage.qml" }
        ListElement { name: qsTr("升级"); icon: "qrc:/images/update42x42.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/SetUpdatePage.qml" }
        ListElement { name: qsTr("使用帮助"); icon: "qrc:/images/icon_help.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/SetHelpPage.qml" }
        ListElement { name: qsTr("用户反馈"); icon: "qrc:/images/icon_user_report.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/SetUserReport.qml" }
        ListElement { name: qsTr("关于设备"); icon: "qrc:/images/info42x42.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/SetDevInfo.qml" }
        ListElement { name: qsTr("Z03Ultra 扩展"); icon: "qrc:/images/info42x42.png"; circle: false; source: "qrc:/ZybQmlFiles/ZybSet/Z03UltraPage.qml" }
    }

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: Pane {
            background: Rectangle { anchors.fill: parent; color: "#0F0F0F" }
            GridView {
                id: view
                anchors.fill: parent
                cellWidth: Style.aviItemWidth; cellHeight: 80
                clip: true
                header: Item {
                    width: parent.width; height: 65
                    ZybBackButton { id: backButton; anchors.top: parent.top; anchors.left: parent.left; onBackClicked: UILogic.backToHome() }
                    ZybText { text: qsTr("设置"); font.styleName: "Bold"; anchors.horizontalCenter: parent.horizontalCenter; anchors.verticalCenter: backButton.verticalCenter }
                }
                Component {
                    id: setDelegate
                    MButtonPic {
                        radius: 6; width: Style.aviItemWidth-10; height: 70; displayMode: 2
                        textColor: Style.fontColor; fontfamily: Style.fontFamily; fontsize: Style.normalFontPixel
                        bgColor: thisbgColor; text: name; pic: icon; haveCircle: circle
                        onClicked: stackView.push(model.source)
                    }
                }
                model: setModel; delegate: setDelegate; focus: true
            }
        }
    }
    Component.onDestruction: {
        if (btManager.getStatus() !== BtConnectStatus.Bt_Close) engineSocket.sendBtScan(false)
        devManager.saveConfigFile(); naviPanel.updateSetIcon()
    }
}
