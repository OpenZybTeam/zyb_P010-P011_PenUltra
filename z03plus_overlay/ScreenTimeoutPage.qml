import QtQuick 2.0
import QtQuick.Controls 2.3
import com.zyb.PingDefine 1.0
import "../ZybControls/"
import "../Style"

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0F0F0F"

    property string currentText: "读取原厂设置…"
    property string statusText: ""
    signal closeRequested()

    function setFactoryTimeout(seconds, label) {
        try {
            var customSent = false
            // EngineSocket 是原厂设置页使用的底层通道，参数单位为秒。
            // 先走该通道，以支持原厂事件表之外的自定义时长。
            if (typeof engineSocket !== "undefined" && engineSocket.sendOffScreenTime) {
                engineSocket.sendOffScreenTime(seconds)
                customSent = true
            }

            // 三个固定值同时发送原厂统计/兼容事件。
            if (seconds === 30 || seconds === 60 || seconds === 180) {
                var eventId = seconds === 30 ? PingDefine.PING_EVENT_SET_SCREEN_TIME_30
                             : seconds === 60 ? PingDefine.PING_EVENT_SET_SCREEN_TIME_60
                             : PingDefine.PING_EVENT_SET_SCREEN_TIME_180
                if (typeof pingManager !== "undefined" && pingManager.addPingData)
                    pingManager.addPingData(eventId)
                if (typeof devManager !== "undefined" && devManager.setLightIndex)
                    devManager.setLightIndex(seconds === 30 ? 1 : seconds === 60 ? 2 : 3)
            }
            if (!customSent) {
                root.statusText = qsTr("原厂息屏接口不可用")
            } else {
                root.currentText = label
                root.statusText = qsTr("已请求设置为 ") + label
            }
            statusTimer.restart()
        } catch (e) {
            root.statusText = qsTr("原厂接口调用失败：") + e
            statusTimer.restart()
        }
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
        text: qsTr("息屏时间")
        font.styleName: "Bold"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: backButton.verticalCenter
    }

    Flickable {
        id: timeoutFlickable
        anchors.top: backButton.bottom
        anchors.topMargin: 20
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: status.top
        anchors.bottomMargin: 8
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        clip: true
        contentWidth: width
        contentHeight: timeoutColumn.height + 20
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        Column {
            id: timeoutColumn
            width: timeoutFlickable.width
            spacing: 14

        ZybText {
            width: parent.width
            text: qsTr("当前：") + root.currentText
            color: "#B8B8B8"
            horizontalAlignment: Text.AlignHCenter
        }

        BasicButton {
            width: parent.width
            height: 58
            text: qsTr("30 秒")
            checkable: false
            backgroundTheme: Qt.rgba(60/255, 61/255, 66/255, 0.6)
            onClicked: root.setFactoryTimeout(30, qsTr("30 秒"))
        }
        BasicButton {
            width: parent.width
            height: 58
            text: qsTr("1 分钟")
            checkable: false
            backgroundTheme: Qt.rgba(60/255, 61/255, 66/255, 0.6)
            onClicked: root.setFactoryTimeout(60, qsTr("1 分钟"))
        }
        BasicButton {
            width: parent.width
            height: 58
            text: qsTr("3 分钟")
            checkable: false
            backgroundTheme: Qt.rgba(60/255, 61/255, 66/255, 0.6)
            onClicked: root.setFactoryTimeout(180, qsTr("3 分钟"))
        }
        BasicButton {
            width: parent.width
            height: 58
            text: qsTr("5 分钟")
            checkable: false
            backgroundTheme: Qt.rgba(60/255, 61/255, 66/255, 0.6)
            onClicked: root.setFactoryTimeout(300, qsTr("5 分钟"))
        }
        BasicButton {
            width: parent.width
            height: 58
            text: qsTr("10 分钟")
            checkable: false
            backgroundTheme: Qt.rgba(60/255, 61/255, 66/255, 0.6)
            onClicked: root.setFactoryTimeout(600, qsTr("10 分钟"))
        }
        BasicButton {
            width: parent.width
            height: 58
            text: qsTr("30 分钟")
            checkable: false
            backgroundTheme: Qt.rgba(60/255, 61/255, 66/255, 0.6)
            onClicked: root.setFactoryTimeout(1800, qsTr("30 分钟"))
        }
        BasicButton {
            width: parent.width
            height: 58
            text: qsTr("60 分钟")
            checkable: false
            backgroundTheme: Qt.rgba(60/255, 61/255, 66/255, 0.6)
            onClicked: root.setFactoryTimeout(3600, qsTr("60 分钟"))
        }

        ZybText {
            width: parent.width
            text: qsTr("以上选项调用原厂设置通道，不修改开机脚本、不运行后台常亮程序。")
            color: "#777A82"
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }
    }

    ZybText {
        id: status
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        text: root.statusText
        color: "#31C7A5"
        horizontalAlignment: Text.AlignHCenter
    }
    Timer {
        id: statusTimer
        interval: 2200
        onTriggered: root.statusText = ""
    }
    Component.onCompleted: {
        try {
            if (typeof devManager !== "undefined" && devManager.getLightIndex) {
                var index = devManager.getLightIndex()
                root.currentText = index === 2 ? qsTr("1 分钟")
                    : index === 3 ? qsTr("3 分钟")
                    : qsTr("30 秒")
            } else {
                root.currentText = qsTr("原厂设置")
            }
        } catch (e) {
            root.currentText = qsTr("原厂设置")
        }
    }
}