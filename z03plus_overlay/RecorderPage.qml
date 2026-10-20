import QtQuick 2.0
import QtQuick.Controls 2.3
import QtMultimedia 5.0
import "../ZybControls/"
import "../Style"
import Z03Ultra 1.0

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0F0F0F"
    property color cardColor: Qt.rgba(60 / 255, 61 / 255, 66 / 255, 0.6)
    property var recordings: []
    property string status: ""
    property string pendingDelete: ""
    signal closeRequested()

    RecorderService { id: recorder }

    function refresh() {
        recordings = recorder.recordings()
    }

    function formatTime(seconds) {
        var minutes = Math.floor(seconds / 60)
        var rest = seconds % 60
        return (minutes < 10 ? "0" : "") + minutes + ":" + (rest < 10 ? "0" : "") + rest
    }

    function toggleRecord() {
        if (recorder.recording) {
            recorder.stop()
            return
        }
        if (recorder.start())
            root.status = qsTr("录音中…")
    }

    function playRecording(path) {
        preview.stop()
        preview.source = path
        preview.play()
    }

    MediaPlayer {
        id: preview
        onError: root.status = qsTr("播放错误：") + errorString
    }

    ZybBackButton {
        id: backButton
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 10
        onBackClicked: {
            if (recorder.recording)
                recorder.stop()
            preview.stop()
            root.closeRequested()
        }
    }
    ZybText {
        text: qsTr("录音")
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

    Rectangle {
        id: recordCard
        anchors.top: backButton.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        height: 132
        radius: 10
        color: root.cardColor

        ZybText {
            text: recorder.recording ? qsTr("正在录音") : qsTr("准备就绪")
            font.styleName: "Bold"
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.top: parent.top
            anchors.topMargin: 14
        }
        ZybText {
            text: recorder.recording ? recorder.elapsed.toString() + " 秒" : qsTr("后端：") + recorder.backendName()
            color: "#B8B8B8"
            font.pixelSize: 13
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.top: parent.top
            anchors.topMargin: 44
        }
        ZybText {
            text: qsTr("保存目录：/home/user/z03plus/recordings")
            color: "#6F7178"
            font.pixelSize: 11
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 12
        }
        BasicButton {
            text: recorder.recording ? qsTr("停止") : qsTr("开始录音")
            width: 132
            height: 56
            radius: 10
            checkable: false
            backgroundTheme: recorder.recording ? Qt.rgba(0.7, 0.18, 0.18, 0.8)
                                               : Qt.rgba(0.19, 0.78, 0.65, 0.7)
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.toggleRecord()
        }
    }

    ZybText {
        id: listTitle
        text: qsTr("录音文件")
        color: "#8E9099"
        font.pixelSize: 12
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.top: recordCard.bottom
        anchors.topMargin: 10
    }

    ListView {
        id: recordingList
        anchors.top: listTitle.bottom
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: statusText.top
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: 6
        clip: true
        model: root.recordings
        spacing: 6
        delegate: Rectangle {
            width: recordingList.width
            height: 62
            radius: 8
            color: root.cardColor
            ZybText {
                text: modelData.name
                elide: Text.ElideMiddle
                width: parent.width - 190
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.top: parent.top
                anchors.topMargin: 8
            }
            ZybText {
                text: modelData.modified + " · " + modelData.size
                color: "#8E9099"
                font.pixelSize: 11
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
            }
            BasicButton {
                text: qsTr("播放")
                width: 72
                height: 38
                radius: 7
                checkable: false
                backgroundTheme: root.cardColor
                anchors.right: deleteButton.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                onClicked: root.playRecording(modelData.path)
            }
            BasicButton {
                id: deleteButton
                text: qsTr("删除")
                width: 72
                height: 38
                radius: 7
                checkable: false
                backgroundTheme: Qt.rgba(0.7, 0.18, 0.18, 0.7)
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                onClicked: root.pendingDelete = modelData.path
            }
        }
        footer: ZybText {
            width: recordingList.width
            height: root.recordings.length === 0 ? 48 : 0
            text: qsTr("暂无录音文件")
            color: "#6F7178"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    ZybText {
        id: statusText
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        text: root.status
        color: "#31C7A5"
        horizontalAlignment: Text.AlignHCenter
    }

    Rectangle {
        visible: root.pendingDelete.length > 0
        anchors.fill: parent
        color: "#99000000"
        z: 100
        Rectangle {
            width: parent.width - 56
            height: 168
            radius: 10
            color: "#292A2E"
            anchors.centerIn: parent
            ZybText {
                text: qsTr("确认删除该录音？")
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 28
            }
            BasicButton {
                text: qsTr("取消")
                width: 118
                height: 46
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 18
                onClicked: root.pendingDelete = ""
            }
            BasicButton {
                text: qsTr("删除")
                width: 118
                height: 46
                radius: 8
                checkable: false
                backgroundTheme: Qt.rgba(0.7, 0.18, 0.18, 0.8)
                anchors.right: parent.right
                anchors.rightMargin: 18
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 18
                onClicked: {
                    preview.stop()
                    root.status = recorder.remove(root.pendingDelete) ? qsTr("已删除") : qsTr("删除失败")
                    root.pendingDelete = ""
                    root.refresh()
                }
            }
        }
    }

    Connections {
        target: recorder
        onStarted: root.status = qsTr("开始录音：") + path
        onStopped: {
            root.status = qsTr("录音完成：") + path
            root.refresh()
        }
        onFailed: root.status = reason
        onRecordingChanged: if (!recorder.recording) root.refresh()
    }

    Component.onCompleted: root.refresh()
}
