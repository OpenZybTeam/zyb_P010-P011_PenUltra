import QtQuick 2.0
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
import QtQuick.Window 2.0
import QtQuick.VirtualKeyboard 2.0
import "../Funcs/UILogics.js" as UILogic
import "../ZybControls/"
import "../Style"
import Z03Plus 1.0

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0F0F0F"
    property color cardColor: Qt.rgba(60/255, 61/255, 66/255, 0.6)
    property bool sshPageVisible: false
    property bool fileManagerVisible: false
    property bool readerVisible: false
    property bool screenTimeoutVisible: false
    property bool aiPageVisible: false
    property string aiInput: ""
    property bool aiBusy: false
    property bool aiScanArmed: false
    property bool aiVoiceRecording: false
    property int aiScanPollCount: 0
    property int aiScanCursorPosition: 0
    property int aiScanSelectionStart: 0
    property int aiScanSelectionEnd: 0
    property bool aiScanHadSelection: false
    property string aiAnswer: ""
    property string aiStatus: ""
    property bool aiServiceOnline: false
    property var aiMessages: []
property bool aiHistoryVisible: false
    property string currentPath: ""
    property var fileEntries: []
    property string fileStatus: ""
    property int fileOrder: 0
    property bool fileReversed: false
    property bool fileActionsVisible: false
    property var selectedFile: null
    property bool renameDialogVisible: false
    property string renameValue: ""
    property string readerPath: ""
    property string readerContent: ""
    property string readerError: ""
    SshService { id: sshService }
    FileService { id: fileService }
    TextFileService { id: textFileService }
    VoiceService { id: voiceService }
    // 语音输入走本地桥接（bridge.mjs /api/voice/*），录音后由宿主机 Vosk 离线识别。

    Timer {
        id: aiVoiceResultTimer
        interval: 600; running: false; repeat: true
        property int ticks: 0
        onTriggered: {
            ticks++
            var xhr = new XMLHttpRequest()
            xhr.open("GET", "http://127.0.0.1:8080/api/voice/result")
            xhr.timeout = 2000
            xhr.onreadystatechange = function() {
                if (xhr.readyState !== 4 || xhr.status !== 200) return
                try {
                    var obj = JSON.parse(xhr.responseText)
                    if (!obj || !obj.ok) return
                    if (obj.busy) return  // 识别中，继续轮询
                    aiVoiceResultTimer.stop(); aiVoiceResultTimer.ticks = 0
                    if (typeof obj.text === "string" && obj.text.length > 0) {
                        root.aiInsertVoiceText(obj.text)
                    } else {
                        root.aiStatus = qsTr("未识别到语音内容")
                    }
                } catch (e) {}
            }
            try { xhr.send() } catch (e) {}
            if (ticks > 60) { aiVoiceResultTimer.stop(); ticks = 0; root.aiStatus = qsTr("识别超时") }
        }
    }

    ZybBackButton {
        id: backButton
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 10
        onBackClicked: {
            root.visible = false
            UILogic.backToSetPage()
        }
    }

    ZybText {
        text: qsTr("Z03Plus 扩展 · FM3")
        font.styleName: "Bold"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: backButton.verticalCenter
    }

    Flickable {
        id: featureList
        anchors.top: backButton.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: 20
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        clip: true
        contentWidth: width
        contentHeight: featureGrid.height + 10
        flickableDirection: Flickable.VerticalFlick

        Grid {
            id: featureGrid
            width: featureList.width
            columns: 2
            rowSpacing: 12
            columnSpacing: 12

            BasicButton {
                text: qsTr("文件管理")
                implicitWidth: (featureList.width - 12) / 2
                implicitHeight: 62
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                onClicked: { root.openFiles(""); root.fileManagerVisible = true }
            }
            BasicButton {
                text: qsTr("文本阅读")
                implicitWidth: (featureList.width - 12) / 2
                implicitHeight: 62
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                onClicked: { root.openFiles(""); root.fileManagerVisible = true }
            }
            BasicButton {
                text: qsTr("存储空间")
                implicitWidth: (featureList.width - 12) / 2
                implicitHeight: 62
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                onClicked: showTip(qsTr("数据目录：/home/user  和  /mnt/data"))
            }
            BasicButton {
                text: qsTr("AI 对话")
                implicitWidth: (featureList.width - 12) / 2
                implicitHeight: 62
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                onClicked: root.openAi()
            }
            BasicButton {
                text: qsTr("显示与灯光")
                implicitWidth: (featureList.width - 12) / 2
                implicitHeight: 62
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                onClicked: StackView.view.push("qrc:/ZybQmlFiles/ZybSet/SetLightPage.qml")
            }
            BasicButton {
                text: qsTr("ADB / SSH")
                implicitWidth: (featureList.width - 12) / 2
                implicitHeight: 62
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                onClicked: { root.sshPageVisible = true; root.refreshSshStatus() }
            }
            BasicButton {
                text: qsTr("音乐播放器")
                implicitWidth: (featureList.width - 12) / 2
                implicitHeight: 62
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                onClicked: showTip(qsTr("播放器正在接入 /mnt/data 媒体文件"))
            }
            BasicButton {
                text: qsTr("录音")
                implicitWidth: (featureList.width - 12) / 2
                implicitHeight: 62
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                onClicked: showTip(qsTr("录音功能正在适配"))
            }
            BasicButton {
                text: qsTr("息屏时间")
                implicitWidth: (featureList.width - 12) / 2
                implicitHeight: 62
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                onClicked: {
                    root.screenTimeoutVisible = true
                }
            }
        }
    }

    Loader {
        id: screenTimeoutLoader
        anchors.fill: parent
        z: 50
        active: root.screenTimeoutVisible
        source: active ? "qrc:/ZybQmlFiles/ZybSet/ScreenTimeoutPage.qml" : ""
        onLoaded: if (item) item.closeRequested.connect(function() { root.screenTimeoutVisible = false })
    }
    Rectangle {
        id: sshPage
        visible: root.sshPageVisible
        z: 20
        anchors.fill: parent
        color: "#0F0F0F"

        ZybBackButton {
            anchors.top: parent.top; anchors.topMargin: 10
            anchors.left: parent.left; anchors.leftMargin: 10
            onBackClicked: root.sshPageVisible = false
        }
        ZybText {
            text: qsTr("SSH 远程连接")
            font.styleName: "Bold"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top; anchors.topMargin: 18
        }
        BasicButton {
            text: "⏻"
            anchors.right: refreshButton.left; anchors.rightMargin: 10
            anchors.top: parent.top; anchors.topMargin: 9
            width: 56; height: 48; radius: 24; checkable: false
            backgroundTheme: root.cardColor
            onClicked: {
                if (root.sshRunning === qsTr("已开启")) {
                    sshService.stop()
                    root.showTip(qsTr("正在关闭 SSH 服务"))
                } else {
                    sshService.start()
                    root.showTip(qsTr("正在启动 SSH 服务"))
                }
                sshRefreshTimer.restart()
            }
        }
        BasicButton {
            id: refreshButton
            text: "↻"
            anchors.right: parent.right; anchors.rightMargin: 16
            anchors.top: parent.top; anchors.topMargin: 9
            width: 52; height: 48; radius: 24; checkable: false
            backgroundTheme: root.cardColor
            onClicked: root.refreshSshStatus()
        }
        Flickable {
            anchors.left: parent.left; anchors.leftMargin: 28
            anchors.right: parent.right; anchors.rightMargin: 20
            anchors.top: parent.top; anchors.topMargin: 74
            anchors.bottom: parent.bottom; anchors.bottomMargin: 14
            clip: true
            contentWidth: width
            contentHeight: sshDetails.height + 10
            flickableDirection: Flickable.VerticalFlick
            Column {
                id: sshDetails
                width: parent.width - 8
                spacing: 14
            ZybText { text: qsTr("服务：") + root.sshRunning }
            ZybText { text: qsTr("IP 地址：") + root.sshIp; wrapMode: Text.WrapAnywhere }
            ZybText { text: qsTr("端口：") + root.sshPort }
            ZybText { text: qsTr("用户名：") + root.sshUser }
            ZybText { text: qsTr("公钥：") + root.sshKeyStatus }
            ZybText { text: qsTr("连接命令：") + root.sshCommand; wrapMode: Text.WrapAnywhere }
            ZybText {
                width: parent.width; color: "#B8B8B8"; font.pixelSize: 18
                wrapMode: Text.WordWrap
                text: qsTr("使用 SSH 公钥免密码登录；仅允许 user 登录，root 的 SSH 登录已禁用。")
            }
            }
        }
    }

    property string sshRunning: "未知"
    property string sshIp: "正在读取…"
    property string sshPort: "22"
    property string sshUser: "user"
    property string sshKeyStatus: "未配置"
    Rectangle {
        id: filePage
        visible: root.fileManagerVisible
        z: 30
        anchors.fill: parent
        color: "#0F0F0F"
        ListView {
            id: fileList
            anchors.fill: parent
            clip: true; model: root.fileEntries
            header: Item {
                width: fileList.width; height: 104
                ZybBackButton { anchors.top: parent.top; anchors.topMargin: 10; anchors.left: parent.left; anchors.leftMargin: 10; onBackClicked: root.fileManagerVisible = false }
                ZybText { text: qsTr("文件管理"); font.styleName: "Bold"; anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.top; anchors.topMargin: 18 }
                BasicButton { text: qsTr("上一级"); anchors.right: parent.right; anchors.rightMargin: 15; anchors.top: parent.top; anchors.topMargin: 10; width: 86; height: 45; radius: 8; checkable: false; enabled: fileService.canGoUp(root.currentPath); backgroundTheme: root.cardColor; onClicked: root.openFiles(fileService.parentPath(root.currentPath)) }
                ZybText { text: root.currentPath; elide: Text.ElideMiddle; color: "#B8B8B8"; font.pixelSize: 16; anchors.top: parent.top; anchors.topMargin: 65; anchors.left: parent.left; anchors.leftMargin: 20; anchors.right: parent.right; anchors.rightMargin: 150 }
                BasicButton { text: root.fileOrder === 0 ? qsTr("名称") : (root.fileOrder === 1 ? qsTr("时间") : qsTr("大小")); anchors.right: reverseButton.left; anchors.rightMargin: 6; anchors.top: parent.top; anchors.topMargin: 60; width: 72; height: 34; radius: 6; checkable: false; backgroundTheme: root.cardColor; onClicked: { root.fileOrder = (root.fileOrder + 1) % 3; root.openFiles(root.currentPath) } }
                BasicButton { id: reverseButton; text: root.fileReversed ? "↓" : "↑"; anchors.right: parent.right; anchors.rightMargin: 15; anchors.top: parent.top; anchors.topMargin: 60; width: 42; height: 34; radius: 6; checkable: false; backgroundTheme: root.cardColor; onClicked: { root.fileReversed = !root.fileReversed; root.openFiles(root.currentPath) } }
            }
            delegate: BasicButton {
                width: fileList.width; height: 58; radius: 7; checkable: false; backgroundTheme: root.cardColor
                property bool held: false
                text: (modelData.isDir ? "[目录] " : "[文件] ") + modelData.name + (modelData.isDir ? "" : "    " + modelData.size)
                Timer { id: holdTimer; interval: 650; repeat: false; onTriggered: { parent.held = true; root.selectedFile = modelData; root.fileActionsVisible = true } }
                onPressedChanged: { if (pressed) { held = false; holdTimer.restart() } else holdTimer.stop() }
                onClicked: { if (held) return; if (modelData.isDir) root.openFiles(modelData.path); else if (modelData.text) root.openText(modelData.path); else root.fileStatus = qsTr("仅支持 TXT、MD、LRC、LOG") }
            }
            footer: ZybText { width: fileList.width; horizontalAlignment: Text.AlignHCenter; color: "#B8B8B8"; text: root.fileEntries.length === 0 ? root.fileStatus : "" }
        }
    }
    Rectangle {
        visible: root.fileActionsVisible
        z: 35
        anchors.fill: parent
        color: "#99000000"
        MouseArea { anchors.fill: parent; onClicked: root.fileActionsVisible = false }
        Rectangle {
            width: parent.width - 48; height: 220; radius: 10; color: "#292A2E"; anchors.centerIn: parent
            ZybText { text: root.selectedFile ? root.selectedFile.name : ""; elide: Text.ElideMiddle; anchors.top: parent.top; anchors.topMargin: 18; anchors.left: parent.left; anchors.leftMargin: 20; anchors.right: parent.right; anchors.rightMargin: 20 }
            BasicButton { text: qsTr("重命名"); anchors.left: parent.left; anchors.leftMargin: 20; anchors.right: parent.right; anchors.rightMargin: 20; anchors.top: parent.top; anchors.topMargin: 62; height: 50; radius: 7; checkable: false; backgroundTheme: root.cardColor; onClicked: { root.renameValue = root.selectedFile.name; root.fileActionsVisible = false; root.renameDialogVisible = true } }
            BasicButton { text: qsTr("删除"); anchors.left: parent.left; anchors.leftMargin: 20; anchors.right: parent.right; anchors.rightMargin: 20; anchors.top: parent.top; anchors.topMargin: 122; height: 50; radius: 7; checkable: false; backgroundTheme: Qt.rgba(0.7, 0.18, 0.18, 0.75); onClicked: { if (root.selectedFile && fileService.remove(root.selectedFile.path)) root.openFiles(root.currentPath); else root.fileStatus = qsTr("删除失败"); root.fileActionsVisible = false } }
        }
    }
    Rectangle {
        visible: root.renameDialogVisible
        z: 36
        anchors.fill: parent
        color: "#99000000"
        Rectangle {
            width: parent.width - 48; height: 190; radius: 10; color: "#292A2E"; anchors.centerIn: parent
            ZybText { text: qsTr("重命名"); anchors.top: parent.top; anchors.topMargin: 16; anchors.left: parent.left; anchors.leftMargin: 20 }
            Rectangle { anchors.left: parent.left; anchors.leftMargin: 20; anchors.right: parent.right; anchors.rightMargin: 20; anchors.top: parent.top; anchors.topMargin: 55; height: 46; radius: 5; color: "#151619"; TextInput { id: renameInput; anchors.fill: parent; anchors.margins: 8; color: "white"; font.pixelSize: 19; text: root.renameValue; selectByMouse: true; onTextChanged: root.renameValue = text } }
            BasicButton { text: qsTr("取消"); anchors.left: parent.left; anchors.leftMargin: 20; anchors.bottom: parent.bottom; anchors.bottomMargin: 16; width: 110; height: 48; radius: 7; checkable: false; backgroundTheme: root.cardColor; onClicked: root.renameDialogVisible = false }
BasicButton { text: qsTr("确定"); anchors.right: parent.right; anchors.rightMargin: 20; anchors.bottom: parent.bottom; anchors.bottomMargin: 16; width: 110; height: 48; radius: 7; checkable: false; backgroundTheme: root.cardColor; onClicked: { if (root.selectedFile && fileService.rename(root.selectedFile.path, root.renameValue)) root.openFiles(root.currentPath); else root.fileStatus = qsTr("重命名失败"); root.renameDialogVisible = false } }
            Component.onCompleted: if (root.renameDialogVisible) renameInput.forceActiveFocus()
            onVisibleChanged: if (visible) renameInput.forceActiveFocus()
        }
    }
    Rectangle {
        id: readerPage
        visible: root.readerVisible
        z: 40
        anchors.fill: parent
        color: "#0F0F0F"
        Flickable { id: textFlick; anchors.fill: parent; clip: true; contentWidth: width; contentHeight: textView.height + 78; flickableDirection: Flickable.VerticalFlick
            Item { width: textFlick.width; height: 65
                ZybBackButton { anchors.top: parent.top; anchors.topMargin: 10; anchors.left: parent.left; anchors.leftMargin: 10; onBackClicked: root.readerVisible = false }
                ZybText { text: textFileService.fileName(root.readerPath); font.styleName: "Bold"; elide: Text.ElideMiddle; anchors.top: parent.top; anchors.topMargin: 18; anchors.left: parent.left; anchors.leftMargin: 70; anchors.right: parent.right; anchors.rightMargin: 16 }
            }
            Text { id: textView; y: 70; x: 18; width: textFlick.width - 36; text: root.readerError || root.readerContent; color: "#F2F2F2"; font.pixelSize: 20; wrapMode: Text.Wrap; textFormat: Text.PlainText }
        }
    }

    Rectangle {
        id: aiPage
        visible: root.aiPageVisible
        z: 50
        anchors.fill: parent
        color: "#0F0F0F"
        // 导航栏：标题居中，左侧竖排工具列
        Rectangle {
            id: aiNavBar
            anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
            height: 40; color: "#0F0F0F"
            ZybText { text: qsTr("AI 对话"); font.styleName: "Bold"; font.pixelSize: 14; anchors.horizontalCenter: parent.horizontalCenter; anchors.verticalCenter: parent.verticalCenter }
        }
        // 左侧竖排工具列
        Column {
            id: aiToolCol
            anchors.top: parent.top; anchors.topMargin: 4
            anchors.left: parent.left; anchors.leftMargin: 6
            spacing: 8
            z: 60
            // 返回
            Rectangle {
                width: 46; height: 46; radius: 23; color: "#26272B"
                Canvas { anchors.fill: parent; anchors.margins: 12
                    onPaint: { var c=getContext("2d"); c.strokeStyle="#EEEEEE"; c.lineWidth=2.2; c.lineCap="round"; c.lineJoin="round"; c.beginPath(); c.moveTo(width*0.62,height*0.2); c.lineTo(width*0.3,height*0.5); c.lineTo(width*0.62,height*0.8); c.stroke() } }
                MouseArea { anchors.fill: parent; onClicked: { root.aiBusy = false; root.aiPageVisible = false } }
            }
            // 新对话（＋）
            Rectangle {
                width: 46; height: 46; radius: 23; color: "#26272B"
                Canvas { anchors.fill: parent; anchors.margins: 12
                    onPaint: { var c=getContext("2d"); c.strokeStyle="#EEEEEE"; c.lineWidth=2.2; c.lineCap="round"; c.beginPath(); c.moveTo(width*0.5,height*0.22); c.lineTo(width*0.5,height*0.78); c.moveTo(width*0.22,height*0.5); c.lineTo(width*0.78,height*0.5); c.stroke() } }
                MouseArea { anchors.fill: parent; onClicked: root.aiNewConversation() }
            }
            // 历史（时钟）
            Rectangle {
                width: 46; height: 46; radius: 23; color: "#26272B"
                Canvas { anchors.fill: parent; anchors.margins: 11
                    onPaint: { var c=getContext("2d"); c.strokeStyle="#EEEEEE"; c.lineWidth=2; c.lineCap="round"; c.beginPath(); c.arc(width/2,height/2,width*0.38,0,Math.PI*2); c.stroke(); c.beginPath(); c.moveTo(width/2,height*0.3); c.lineTo(width/2,height/2); c.lineTo(width*0.66,height*0.58); c.stroke() } }
                MouseArea { anchors.fill: parent; onClicked: { root.aiHistoryVisible = !root.aiHistoryVisible; if (root.aiHistoryVisible) root.aiFetchHistory() } }
            }
            // 重连（环形箭头）
            Rectangle {
                id: aiReconnectBtn
                width: 46; height: 46; radius: 23; color: root.aiServiceOnline ? "#1E3A2A" : "#3A1E1E"
                Canvas { anchors.fill: parent; anchors.margins: 11
                    onPaint: { var c=getContext("2d"); c.strokeStyle="#EEEEEE"; c.lineWidth=2; c.lineCap="round"; c.beginPath(); c.arc(width/2,height/2,width*0.34,Math.PI*0.3,Math.PI*1.9); c.stroke(); c.beginPath(); c.moveTo(width*0.2,height*0.32); c.lineTo(width*0.24,height*0.14); c.lineTo(width*0.36,height*0.24); c.stroke() } }
                MouseArea { anchors.fill: parent; onClicked: {
                    root.aiServiceOnline = false
                    aiReconnectTimer.restart()
                    sshService.restartAi()
                } }
                Timer {
                    id: aiReconnectTimer
                    interval: 9000; running: false; repeat: true
                    property int ticks: 0
                    onTriggered: {
                        ticks++
                        var xhr = new XMLHttpRequest()
                        xhr.open("GET", "http://127.0.0.1:8080/api/status")
                        xhr.timeout = 4000
                        xhr.onreadystatechange = function() {
                            if (xhr.readyState !== 4) return
                            if (xhr.status === 200) { root.aiServiceOnline = true; aiReconnectTimer.stop() }
                        }
                        try { xhr.send() } catch (e) {}
                        if (ticks >= 6) { aiReconnectTimer.stop(); ticks = 0 }
                    }
                }
            }
        }
        // 对话区（气泡式）
        ListView {
            id: aiOutputFlick
            anchors.top: aiNavBar.bottom; anchors.topMargin: 6
            anchors.left: parent.left; anchors.leftMargin: 56
            anchors.right: parent.right; anchors.rightMargin: 8
            anchors.bottom: aiQuickRow.top; anchors.bottomMargin: 6
            clip: true
            spacing: 8
            model: root.aiMessages
            delegate: Item {
                width: aiOutputFlick.width
                height: bubbleCol.height
                Column {
                    id: bubbleCol
                    width: parent.width
                    spacing: 2
                    Rectangle {
                        width: Math.min(msgText.implicitWidth + 20, aiOutputFlick.width * 0.8)
                        height: msgText.implicitHeight + 16
                        radius: 12
                        color: modelData.role === "user" ? "#2E5BFF" : "#2A2B30"
                        anchors.right: modelData.role === "user" ? parent.right : undefined
                        Text {
                            id: msgText
                            anchors.centerIn: parent
                            width: Math.min(implicitWidth, aiOutputFlick.width * 0.8 - 20)
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                            color: "#F2F2F2"
                            font.pixelSize: 14
                            text: modelData.text
                        }
                    }
                }
            }
            onCountChanged: positionViewAtEnd()
            // 思考中气泡
            footer: root.aiBusy ? thinkingBubble : null
        }
        Component {
            id: thinkingBubble
            Rectangle {
                width: 120; height: 32; radius: 12; color: "#2A2B30"
                Text { anchors.centerIn: parent; text: qsTr("正在思考…"); color: "#BBBBBB"; font.pixelSize: 13 }
            }
        }
        // 快捷气泡
        Row {
            id: aiQuickRow
            anchors.left: parent.left; anchors.leftMargin: 56
            anchors.right: parent.right; anchors.rightMargin: 8
            anchors.bottom: aiInputBox.top; anchors.bottomMargin: 6
            spacing: 6
            visible: !Qt.inputMethod.visible && root.aiMessages.length === 0
            height: visible ? 26 : 0
            Repeater {
                model: [qsTr("你好"), qsTr("今天天气"), qsTr("讲个笑话")]
                Rectangle {
                    width: quickText.implicitWidth + 16; height: 26; radius: 13
                    color: "#1E1F24"; border.color: "#3A3B42"; border.width: 1
                    Text { id: quickText; anchors.centerIn: parent; text: modelData; color: "#CCCCCC"; font.pixelSize: 12 }
                    MouseArea { anchors.fill: parent; onClicked: if (!root.aiBusy) root.aiChat(modelData) }
                }
            }
        }
        // 输入区
        Rectangle {
            id: aiInputBox; anchors.left: parent.left; anchors.leftMargin: 56; anchors.right: parent.right; anchors.rightMargin: 8; anchors.bottom: parent.bottom
            anchors.bottomMargin: Qt.inputMethod.visible ? (virtualKeyboard.height + 6) : 8
            height: 44; radius: 10; color: "#292A2E"
            TextInput {
                id: aiInputField; anchors.left: parent.left; anchors.leftMargin: 10; anchors.right: aiScan.left; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter
                color: "#F2F2F2"; font.pixelSize: 15; clip: true; selectByMouse: true
                activeFocusOnPress: true
                text: root.aiInput; onTextChanged: root.aiInput = text
            }
            ZybText { visible: aiInputField.text.length === 0; text: qsTr("输入问题…"); color: "#999999"; font.pixelSize: 15; anchors.left: aiInputField.left; anchors.leftMargin: 10; anchors.verticalCenter: aiInputField.verticalCenter }
            BasicButton {
 id: aiVoice
 text: root.aiVoiceRecording ? qsTr("停") : qsTr("语音")
 anchors.right: aiScan.left; anchors.rightMargin: 4; anchors.verticalCenter: parent.verticalCenter
 width: 48; height: 34; radius: 8; checkable: false; backgroundTheme: root.cardColor
 onClicked: {
  try {
   var xhr = new XMLHttpRequest()
   if (root.aiVoiceRecording) {
    // 停止录音并等待识别
    xhr.open("POST", "http://127.0.0.1:8080/api/voice/stop")
    xhr.timeout = 8000
    xhr.onreadystatechange = function() {
     if (xhr.readyState !== 4) return
     if (xhr.status === 200) {
      root.aiVoiceRecording = false
      root.aiStatus = qsTr("正在识别…")
      aiVoiceResultTimer.ticks = 0
      aiVoiceResultTimer.restart()
     } else {
      root.aiVoiceRecording = false
      root.aiStatus = qsTr("停止录音失败：") + xhr.status
     }
    }
    try { xhr.send() } catch (e) { root.aiVoiceRecording = false; root.aiStatus = qsTr("停止失败") }
   } else {
    // 开始录音
    xhr.open("POST", "http://127.0.0.1:8080/api/voice/start")
    xhr.timeout = 5000
    xhr.onreadystatechange = function() {
     if (xhr.readyState !== 4) return
     if (xhr.status === 200) {
      root.aiVoiceRecording = true
      root.aiStatus = qsTr("正在录音，再次点击停止")
     } else {
      root.aiStatus = qsTr("录音服务未启动：") + xhr.status
     }
    }
    try { xhr.send() } catch (e) { root.aiStatus = qsTr("启动失败") }
   }
  } catch (e) {
   root.aiVoiceRecording = false
   root.aiStatus = qsTr("语音操作失败：") + String(e)
  }
 }
}
            BasicButton {
                id: aiScan; text: root.aiScanArmed ? qsTr("等") : qsTr("扫"); anchors.right: aiSend.left; anchors.rightMargin: 4; anchors.verticalCenter: parent.verticalCenter
                width: 40; height: 34; radius: 8; checkable: false; backgroundTheme: root.cardColor
                onClicked: root.aiArmScan()
            }
            BasicButton {
                id: aiSend; text: root.aiBusy ? qsTr("…") : qsTr("发"); anchors.right: parent.right; anchors.rightMargin: 5; anchors.verticalCenter: parent.verticalCenter
                width: 40; height: 34; radius: 8; checkable: false; backgroundTheme: root.cardColor
                onClicked: { if (!root.aiBusy && root.aiInput.length > 0) { var q = root.aiInput; root.aiInput = ""; aiInputField.text = ""; root.aiChat(q) } }
            }
        }
        // ---- 历史对话抽屉（左侧滑入） ----
        Rectangle {
            id: aiHistoryDrawer
            visible: root.aiHistoryVisible
            anchors.top: parent.top; anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: Math.min(340, parent.width - 40)
            color: "#16171A"; border.color: "#2E2F36"; border.width: 1
            z: 60
            Column {
                anchors.fill: parent; anchors.margins: 10; spacing: 8
                // 标题行
                Item {
                    width: parent.width; height: 32
                    ZybText { text: qsTr("历史对话"); color: "#F2F2F2"; font.pixelSize: 16; anchors.verticalCenter: parent.verticalCenter }
                    Rectangle {
                        width: 26; height: 26; radius: 13; color: "#26272B"
                        anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                        Canvas { anchors.fill: parent; anchors.margins: 8
                            onPaint: { var c=getContext("2d"); c.strokeStyle="#EEEEEE"; c.lineWidth=2; c.lineCap="round"; c.beginPath(); c.moveTo(width*0.25,height*0.25); c.lineTo(width*0.75,height*0.75); c.moveTo(width*0.75,height*0.25); c.lineTo(width*0.25,height*0.75); c.stroke() } }
                        MouseArea { anchors.fill: parent; onClicked: root.aiHistoryVisible = false }
                    }
                }
                Rectangle { width: parent.width; height: 1; color: "#2E2F36" }
                // 空态
                ZybText {
                    visible: root.aiHistoryList.length === 0
                    text: qsTr("暂无历史对话"); color: "#888888"; font.pixelSize: 13
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                // 会话列表
                ListView {
                    width: parent.width; height: parent.height - 50
                    clip: true; spacing: 6
                    model: root.aiHistoryList
                    delegate: Rectangle {
                        width: parent.width; height: 52; radius: 8
                        color: "#1E1F24"; border.color: "#2E2F36"; border.width: 1
                        Column {
                            anchors.left: parent.left; anchors.leftMargin: 10; anchors.right: delBtn.left; anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter; spacing: 3
                            Text {
                                text: modelData.title || qsTr("（未命名）")
                                color: "#EEEEEE"; font.pixelSize: 14
                                elide: Text.ElideRight; width: parent.width
                            }
                            Text {
                                text: Qt.formatDateTime(new Date(modelData.created), "MM-dd hh:mm") + " · " + modelData.count + qsTr(" 条")
                                color: "#888888"; font.pixelSize: 11
                            }
                        }
                        // 删除按钮
                        Rectangle {
                            id: delBtn
                            width: 30; height: 30; radius: 6; color: "#3A1E1E"
                            anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
                            Canvas { anchors.fill: parent; anchors.margins: 9
                                onPaint: { var c=getContext("2d"); c.strokeStyle="#FF6B6B"; c.lineWidth=1.6; c.lineCap="round"; c.beginPath(); c.moveTo(width*0.3,height*0.28); c.lineTo(width*0.3,height*0.76); c.lineTo(width*0.7,height*0.76); c.lineTo(width*0.7,height*0.28); c.moveTo(width*0.24,height*0.28); c.lineTo(width*0.76,height*0.28); c.moveTo(width*0.42,height*0.28); c.lineTo(width*0.42,height*0.2); c.lineTo(width*0.58,height*0.2); c.lineTo(width*0.58,height*0.28); c.stroke() } }
                            MouseArea { anchors.fill: parent; onClicked: root.aiDeleteHistory(modelData.id) }
                        }
                        MouseArea {
                            anchors.fill: parent; anchors.rightMargin: 40
                            onClicked: root.aiLoadHistory(modelData.id)
                        }
                    }
                }
            }
            // 点抽屉外关闭
            MouseArea {
                anchors.right: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                width: aiPage.width - aiHistoryDrawer.width
                onClicked: root.aiHistoryVisible = false
            }
        }
    }
    property string sshCommand: ""
    property int sshRefreshAttempts: 0
    Timer {
        id: sshRefreshTimer
        interval: 700
        repeat: false
        onTriggered: {
            root.loadSshStatus()
            if (root.sshRefreshAttempts > 0) {
                root.sshRefreshAttempts--
                sshRefreshTimer.restart()
            }
        }
    }

    function openAi() {
        aiAnswer = ""
        aiStatus = qsTr("连接本地 AI 服务…")
        aiPageVisible = true
        var xhr = new XMLHttpRequest()
        xhr.open("GET", "http://127.0.0.1:8080/api/status")
        xhr.timeout = 5000
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== 4) return
            if (xhr.status === 200) {
                root.aiServiceOnline = true
                aiStatus = qsTr("AI 服务已就绪，请输入问题")
            } else {
                root.aiServiceOnline = false
                aiStatus = qsTr("AI 服务未启动（点右上角「重连 AI」）")
            }
        }
        try { xhr.send() } catch (e) { aiStatus = qsTr("AI 服务未启动") }
    }
    Timer {
        id: aiScanPollTimer
        interval: 500
        repeat: true
        onTriggered: root.aiPollScanResult()
    }

    function aiArmScan() {
        if (aiBusy || aiScanArmed) return
        // 保存进入原厂 OCR 前的光标和选区。
        aiInputField.forceActiveFocus()
        root.aiScanCursorPosition = aiInputField.cursorPosition
        root.aiScanSelectionStart = aiInputField.selectionStart
        root.aiScanSelectionEnd = aiInputField.selectionEnd
        root.aiScanHadSelection = (aiInputField.selectionStart !== aiInputField.selectionEnd)
        var xhr = new XMLHttpRequest
        xhr.open("POST", "http://127.0.0.1:8080/api/scan/arm")
        xhr.setRequestHeader("Content-Type", "application/json")
        xhr.timeout = 5000
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== 4) return
            if (xhr.status === 200) {
                try {
                    var obj = JSON.parse(xhr.responseText)
                    if (obj && obj.ok) {
                        root.aiScanArmed = true
                        root.aiScanPollCount = 0
                        root.aiStatus = qsTr("请扫描（30秒）")
                        aiScanPollTimer.start()
                        return
                    }
                } catch (e) {}
            }
            root.aiStatus = qsTr("扫描准备失败")
        }
        xhr.onerror = function() { root.aiStatus = qsTr("AI 服务未启动") }
        try { xhr.send("{}") } catch (e) { root.aiStatus = qsTr("扫描准备失败") }
    }

    // 将扫描结果插入正在编辑的位置；若有选区则先用结果替换选区。
    function aiInsertScanText(scanText) {
        aiInputField.forceActiveFocus()
        var start = root.aiScanSelectionStart
        var end = root.aiScanSelectionEnd
        if (!root.aiScanHadSelection) {
            start = root.aiScanCursorPosition
            end = start
        }
        if (start !== end) {
            var left = Math.min(start, end)
            var right = Math.max(start, end)
            aiInputField.remove(left, right)
            aiInputField.cursorPosition = left
        }
        aiInputField.insert(aiInputField.cursorPosition, scanText)
        root.aiInput = aiInputField.text
    }
    // 将语音识别结果追加到输入框当前光标处。
    function aiInsertVoiceText(voiceText) {
        aiInputField.forceActiveFocus()
        aiInputField.insert(aiInputField.cursorPosition, voiceText)
        root.aiInput = aiInputField.text
        root.aiStatus = qsTr("语音识别完成")
    }
    function aiPollScanResult() {
        if (!aiScanArmed) { aiScanPollTimer.stop(); return }
        aiScanPollCount += 1
        if (aiScanPollCount > 60) {
            aiScanArmed = false
            aiScanPollTimer.stop()
            aiStatus = qsTr("扫描超时")
            return
        }
        var xhr = new XMLHttpRequest()
        xhr.open("GET", "http://127.0.0.1:8080/api/scan/result")
        xhr.timeout = 1200
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== 4 || xhr.status !== 200) return
            try {
                var obj = JSON.parse(xhr.responseText)
                if (obj && obj.ok && typeof obj.text === "string" && obj.text.length > 0) {
                    root.aiInsertScanText(obj.text)
                    root.aiScanArmed = false
                    aiScanPollTimer.stop()
                    root.aiStatus = qsTr("扫描文字已填入")
                }
            } catch (e) {}
        }
        try { xhr.send() } catch (e) {}
    }

    function aiChat(q) {
        if (aiBusy) return
        aiBusy = true
        aiStatus = qsTr("正在思考…")
        var msgs = aiMessages.slice()
        msgs.push({ role: "user", text: q })
        // Add one mutable AI bubble immediately; stream updates replace its text.
        msgs.push({ role: "ai", text: qsTr("正在思考…") })
        aiMessages = msgs
        var xhr = new XMLHttpRequest()
        var consumed = 0
        var finalSeen = false
        xhr.open("POST", "http://127.0.0.1:8080/api/chat/stream")
        xhr.timeout = 125000
        xhr.setRequestHeader("Content-Type", "application/json")
        function setStreamingText(t) {
            var m = aiMessages.slice()
            if (m.length > 0) { m[m.length - 1] = { role: "ai", text: t || qsTr("（空回复）") }; aiMessages = m }
        }
        function consumeStream() {
            var all = xhr.responseText || ""
            var part = all.slice(consumed)
            var lastNl = part.lastIndexOf("
")
            if (lastNl < 0) return
            consumed += lastNl + 1
            var lines = part.slice(0, lastNl).split("
")
            for (var i = 0; i < lines.length; i++) {
                if (!lines[i]) continue
                try {
                    var evt = JSON.parse(lines[i])
                    if (evt.type === "update") { setStreamingText(evt.text); aiStatus = qsTr("正在生成…") }
                    else if (evt.type === "done") {
                        finalSeen = true
                        aiBusy = false
                        if (evt.ok) { setStreamingText(evt.text); aiStatus = qsTr("完成") }
                        else { setStreamingText(qsTr("失败：") + (evt.error || "unknown error")); aiStatus = qsTr("失败") }
                    }
                } catch (e) {}
            }
        }
        xhr.onreadystatechange = function() {
            if (xhr.readyState === 3 || xhr.readyState === 4) consumeStream()
            if (xhr.readyState === 4 && !finalSeen) {
                aiBusy = false
                if (xhr.status !== 200) { setStreamingText(qsTr("失败：HTTP ") + xhr.status); aiStatus = qsTr("失败") }
                else { setStreamingText(qsTr("流式响应异常结束")); aiStatus = qsTr("失败") }
            }
        }
        xhr.ontimeout = function() { if (!finalSeen) { aiBusy = false; setStreamingText(qsTr("请求超时")); aiStatus = qsTr("请求超时") } }
        xhr.onerror = function() { if (!finalSeen) { aiBusy = false; setStreamingText(qsTr("AI 服务未启动")); aiStatus = qsTr("失败") } }
        try { xhr.send(JSON.stringify({ text: q })) } catch (e) { aiBusy = false; setStreamingText(qsTr("发送失败")); aiStatus = qsTr("发送失败") }
    }
    function aiNewConversation() {
        if (aiBusy) return
        aiBusy = true
        aiStatus = qsTr("正在新建对话…")
        var xhr = new XMLHttpRequest()
        xhr.open("POST", "http://127.0.0.1:8080/api/new")
        xhr.timeout = 25000
        xhr.setRequestHeader("Content-Type", "application/json")
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== 4) return
            aiBusy = false
            if (xhr.status === 200) { aiStatus = qsTr("已新建对话"); aiMessages = [] }
            else { aiStatus = qsTr("新建失败：") + xhr.status }
        }
        xhr.ontimeout = function() { aiBusy = false; aiStatus = qsTr("新建超时") }
        xhr.onerror = function() { aiBusy = false; aiStatus = qsTr("AI 服务未启动") }
        try { xhr.send("{}") } catch (e) { aiBusy = false; aiStatus = qsTr("发送失败") }
    }

    // ---- 历史对话 ----
    property var aiHistoryList: []
    function aiFetchHistory() {
        var xhr = new XMLHttpRequest()
        xhr.open("GET", "http://127.0.0.1:8080/api/history")
        xhr.timeout = 10000
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== 4) return
            if (xhr.status === 200) {
                try { var obj = JSON.parse(xhr.responseText); if (obj && obj.ok) aiHistoryList = obj.sessions || [] } catch (e) {}
            }
        }
        try { xhr.send() } catch (e) {}
    }
    function aiLoadHistory(id) {
        if (aiBusy) return
        aiBusy = true
        aiStatus = qsTr("正在载入…")
        var xhr = new XMLHttpRequest()
        xhr.open("POST", "http://127.0.0.1:8080/api/history/load")
        xhr.timeout = 10000
        xhr.setRequestHeader("Content-Type", "application/json")
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== 4) return
            aiBusy = false
            if (xhr.status === 200) {
                try {
                    var obj = JSON.parse(xhr.responseText)
                    if (obj && obj.ok) {
                        var arr = []
                        var msgs = obj.messages || []
                        for (var i = 0; i < msgs.length; i++) arr.push({ role: msgs[i].role, text: msgs[i].text })
                        aiMessages = arr
                        aiHistoryVisible = false
                        aiStatus = qsTr("已载入历史")
                        return
                    }
                } catch (e) {}
                aiStatus = qsTr("载入失败")
            } else aiStatus = qsTr("载入失败：") + xhr.status
        }
        xhr.ontimeout = function() { aiBusy = false; aiStatus = qsTr("载入超时") }
        xhr.onerror = function() { aiBusy = false; aiStatus = qsTr("载入失败") }
        try { xhr.send(JSON.stringify({ id: id })) } catch (e) { aiBusy = false }
    }
    function aiDeleteHistory(id) {
        var xhr = new XMLHttpRequest()
        xhr.open("POST", "http://127.0.0.1:8080/api/history/delete")
        xhr.timeout = 10000
        xhr.setRequestHeader("Content-Type", "application/json")
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== 4) return
            if (xhr.status === 200) aiFetchHistory()
        }
        try { xhr.send(JSON.stringify({ id: id })) } catch (e) {}
    }

    function openFiles(path) {
        var values = fileService.openDirectory(path, fileOrder, fileReversed)
        if (!values || !values.ok) {
            fileStatus = values && values.error ? values.error : qsTr("无法读取目录")
            fileEntries = []
            return
        }
        currentPath = values.path
        fileEntries = values.entries
        fileStatus = fileEntries.length === 0 ? qsTr("文件夹为空") : ""
    }

    function openText(path) {
        var values = textFileService.readText(path)
        readerPath = path
        readerContent = values && values.ok ? values.content : ""
        readerError = values && values.ok ? "" : (values && values.error ? values.error : qsTr("打开文件失败"))
        readerVisible = true
    }

    function refreshSshStatus() {
        sshService.refresh()
        sshRefreshAttempts = 2
        sshRefreshTimer.restart()
    }

    function loadSshStatus() {
        var values = sshService.status()
        if (!values || !values.running)
            return
        root.sshRunning = values.running === "on" ? qsTr("已开启") : qsTr("未开启")
        root.sshIp = values.ip || qsTr("未连接 Wi-Fi")
        root.sshPort = values.port || "22"
        root.sshUser = values.user || "user"
        root.sshKeyStatus = values.key_status || qsTr("未配置")
        root.sshCommand = values.command || ""
    }

    function showTip(message) {
        tip.text = message
        tip.visible = true
        tipTimer.restart()
    }
    Text {
        id: tip
        visible: false
        z: 10
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 22
        color: "#12CFA7"
        font.pixelSize: 22
    }
    Timer { id: tipTimer; interval: 1800; onTriggered: tip.visible = false }

    InputPanel {
        id: virtualKeyboard
        parent: Window.window ? Window.window.contentItem : root
        z: 100
        width: Math.round(root.width * 0.48)
        height: Math.round(virtualKeyboard.width / 3.2)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        visible: Qt.inputMethod.visible

        function reportDiag(tag) {
            try {
                var kr = Qt.inputMethod.keyboardRectangle
                var p = virtualKeyboard.mapToItem(root, 0, 0)
                var pb = virtualKeyboard.mapToItem(root, 0, virtualKeyboard.height)
                var xhr = new XMLHttpRequest()
                xhr.open("POST", "http://127.0.0.1:8080/api/diag")
                xhr.setRequestHeader("Content-Type", "application/json")
                xhr.send(JSON.stringify({
                    tag: tag,
                    rootW: root.width, rootH: root.height,
                    winW: Window.window ? Window.window.width : -1,
                    winH: Window.window ? Window.window.height : -1,
                    panelW: virtualKeyboard.width, panelH: virtualKeyboard.height,
                    mapX: Math.round(p.x), mapY: Math.round(p.y),
                    mapBottom: Math.round(pb.y),
                    active: virtualKeyboard.active,
                    imVisible: Qt.inputMethod.visible,
                    imKbW: kr ? kr.width : -1, imKbH: kr ? kr.height : -1
                }))
            } catch (e) {}
        }

        onActiveChanged: reportDiag("active=" + active)
        onHeightChanged: reportDiag("height")
        onYChanged: reportDiag("y")
    }
}