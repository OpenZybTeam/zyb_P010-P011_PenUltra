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
    property var tracks: []
    property int currentIndex: -1
    property string status: ""
    signal closeRequested()

    MusicService { id: music }

    function human(bytes) {
        if (bytes >= 1048576)
            return (bytes / 1048576).toFixed(1) + " MB"
        if (bytes >= 1024)
            return (bytes / 1024).toFixed(0) + " KB"
        return bytes + " B"
    }

    function formatTime(ms) {
        if (!ms || ms < 0)
            ms = 0
        var total = Math.floor(ms / 1000)
        var minutes = Math.floor(total / 60)
        var seconds = total % 60
        return (minutes < 10 ? "0" : "") + minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    function refresh() {
        var list = music.scan()
        tracks = list
        if (currentIndex >= tracks.length)
            currentIndex = -1
        status = tracks.length === 0 ? qsTr("未找到音频文件（/mnt/data、/home/user）") : ""
    }

    function playIndex(index) {
        if (index < 0 || index >= tracks.length)
            return
        currentIndex = index
        player.source = tracks[index].url
        player.play()
        root.status = ""
    }

    function togglePlay() {
        if (player.playbackState === MediaPlayer.PlayingState) {
            player.pause()
        } else if (player.source.toString().length === 0) {
            root.playIndex(0)
        } else {
            player.play()
        }
    }

    function previous() {
        if (root.currentIndex > 0)
            root.playIndex(root.currentIndex - 1)
        else if (root.tracks.length > 0)
            root.playIndex(0)
    }

    function next() {
        if (root.currentIndex + 1 < root.tracks.length)
            root.playIndex(root.currentIndex + 1)
    }

    MediaPlayer {
        id: player
        onError: root.status = qsTr("播放错误：") + errorString
        onPlaybackStateChanged: {
            try {
                if (typeof engineSocket !== "undefined" && engineSocket.sendEnableSysAutoOffScreen)
                    engineSocket.sendEnableSysAutoOffScreen(player.playbackState !== MediaPlayer.PlayingState)
            } catch (e) {}
        }
    }

    onVisibleChanged: {
        if (!visible) {
            player.stop()
            try {
                if (typeof engineSocket !== "undefined" && engineSocket.sendEnableSysAutoOffScreen)
                    engineSocket.sendEnableSysAutoOffScreen(true)
            } catch (e) {}
        }
    }

    Connections {
        target: player
        onPositionChanged: if (!progress.pressed) progress.value = player.position
        onStopped: {
            if (player.position >= player.duration && root.currentIndex + 1 < root.tracks.length)
                root.playIndex(root.currentIndex + 1)
        }
    }

    ZybBackButton {
        id: backButton
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 10
        onBackClicked: {
            player.stop()
            root.closeRequested()
        }
    }
    ZybText {
        text: qsTr("音乐播放器")
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

    ListView {
        id: trackList
        anchors.top: backButton.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: playerBar.top
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.bottomMargin: 8
        clip: true
        model: root.tracks
        spacing: 6
        delegate: Rectangle {
            width: trackList.width
            height: 62
            radius: 8
            color: index === root.currentIndex ? Qt.rgba(0.19, 0.78, 0.65, 0.18) : root.cardColor
            border.width: index === root.currentIndex ? 1 : 0
            border.color: "#31C7A5"
            ZybText {
                text: modelData.name
                elide: Text.ElideMiddle
                width: parent.width - 28
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.top: parent.top
                anchors.topMargin: 8
            }
            ZybText {
                text: modelData.folder
                color: "#8E9099"
                font.pixelSize: 11
                elide: Text.ElideMiddle
                width: parent.width - 100
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
            }
            ZybText {
                text: modelData.size
                color: "#8E9099"
                font.pixelSize: 11
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.playIndex(index)
            }
        }
        footer: ZybText {
            width: trackList.width
            height: root.status.length > 0 ? 40 : 0
            text: root.status
            color: "#E0A24A"
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    Rectangle {
        id: playerBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 132
        color: "#1A1B1F"

        ZybText {
            id: nowPlaying
            text: root.currentIndex >= 0 && root.currentIndex < root.tracks.length
                  ? root.tracks[root.currentIndex].name : qsTr("未选择曲目")
            elide: Text.ElideMiddle
            width: parent.width - 150
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.top: parent.top
            anchors.topMargin: 8
        }
        ZybText {
            id: timeText
            text: root.formatTime(player.position) + " / " + root.formatTime(player.duration)
            color: "#8E9099"
            font.pixelSize: 11
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: nowPlaying.verticalCenter
        }

        BasicSlider {
            id: progress
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.top: nowPlaying.bottom
            anchors.topMargin: 2
            height: 22
            from: 0
            to: Math.max(1, player.duration)
            value: 0
            onMoved: player.seek(value)
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 12
            spacing: 10
            BasicButton {
                text: qsTr("上一首")
                width: 92
                height: 44
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                enabled: root.tracks.length > 0
                onClicked: root.previous()
            }
            BasicButton {
                text: player.playbackState === MediaPlayer.PlayingState ? qsTr("暂停") : qsTr("播放")
                width: 104
                height: 44
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                enabled: root.tracks.length > 0
                onClicked: root.togglePlay()
            }
            BasicButton {
                text: qsTr("下一首")
                width: 92
                height: 44
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                enabled: root.tracks.length > 0
                onClicked: root.next()
            }
        }
    }

    Component.onCompleted: root.refresh()
}
