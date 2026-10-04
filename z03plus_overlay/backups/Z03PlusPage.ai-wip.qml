import QtQuick 2.0
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3
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
    property bool aiPageVisible: false
    property bool aiSettingsVisible: false
    property string aiEndpoint: ""
    property string aiKey: ""
    property string aiModel: ""
    property string aiInput: ""
    property string aiAnswer: ""
    property string aiStatus: ""
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
    AiService {
        id: aiService
        onStarted: { root.aiStatus = qsTr("正在思考…"); root.aiAnswer = "" }
        onDelta: { root.aiAnswer = text; root.aiStatus = qsTr("正在生成…") }
        onCompleted: { root.aiAnswer = text; root.aiStatus = qsTr("完成") }
        onFailed: { root.aiStatus = qsTr("请求失败：") + reason }
        onConversationReset: { root.aiAnswer = ""; root.aiStatus = qsTr("已新建对话") }
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
                text: qsTr("录音与息屏")
                implicitWidth: (featureList.width - 12) / 2
                implicitHeight: 62
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                onClicked: showTip(qsTr("录音与息屏硬件接口正在适配"))
            }
        }
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
            Component.onCompleted: renameInput.forceActiveFocus()
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
        ZybBackButton {
            anchors.top: parent.top; anchors.topMargin: 10
            anchors.left: parent.left; anchors.leftMargin: 10
            onBackClicked: { if (aiService.busy()) aiService.cancel(); root.aiPageVisible = false }
        }
        ZybText { text: qsTr("AI 对话"); font.styleName: "Bold"; anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.top; anchors.topMargin: 18 }
        BasicButton {
            text: qsTr("设置"); anchors.top: parent.top; anchors.topMargin: 10; anchors.right: parent.right; anchors.rightMargin: 12
            width: 72; height: 44; radius: 7; checkable: false; backgroundTheme: root.cardColor
            onClicked: root.aiSettingsVisible = true
        }
        BasicButton {
            text: qsTr("新对话"); anchors.top: parent.top; anchors.topMargin: 62; anchors.right: parent.right; anchors.rightMargin: 12
            width: 84; height: 38; radius: 7; checkable: false; backgroundTheme: root.cardColor
            onClicked: aiService.newConversation()
        }
        Flickable {
            id: aiOutputFlick
            anchors.top: parent.top; anchors.topMargin: 66
            anchors.left: parent.left; anchors.leftMargin: 16
            anchors.right: parent.right; anchors.rightMargin: 16
            anchors.bottom: aiInputBox.top; anchors.bottomMargin: 8
            clip: true; contentWidth: width; contentHeight: aiOutput.height + 12
            Text {
                id: aiOutput; width: aiOutputFlick.width; wrapMode: Text.Wrap; textFormat: Text.PlainText
                color: "#F2F2F2"; font.pixelSize: 19
                text: root.aiAnswer.length > 0 ? root.aiAnswer : root.aiStatus
            }
            onContentHeightChanged: contentY = Math.max(0, contentHeight - height)
        }
        Rectangle {
            id: aiInputBox; anchors.left: parent.left; anchors.leftMargin: 12; anchors.right: parent.right; anchors.rightMargin: 12; anchors.bottom: parent.bottom; anchors.bottomMargin: 12
            height: 70; radius: 8; color: "#292A2E"
            TextInput {
                id: aiInputField; anchors.left: parent.left; anchors.leftMargin: 12; anchors.right: aiSend.left; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
                color: "#F2F2F2"; font.pixelSize: 18; clip: true; selectByMouse: true
                text: root.aiInput; onTextChanged: root.aiInput = text
            }
            ZybText { visible: aiInputField.text.length === 0; text: qsTr("输入问题…"); color: "#999999"; anchors.left: aiInputField.left; anchors.verticalCenter: aiInputField.verticalCenter }
            BasicButton {
                id: aiSend; text: aiService.busy() ? qsTr("停止") : qsTr("发送"); anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
                width: 68; height: 48; radius: 7; checkable: false; backgroundTheme: root.cardColor
                onClicked: { if (aiService.busy()) aiService.cancel(); else if (aiService.send(root.aiInput)) { root.aiInput = ""; aiInputField.text = "" } }
            }
        }
    }
    Rectangle {
        visible: root.aiSettingsVisible
        z: 55; anchors.fill: parent; color: "#DD000000"
        Rectangle {
            width: parent.width - 36; height: 330; radius: 10; color: "#292A2E"; anchors.centerIn: parent
            ZybText { text: qsTr("AI 设置（OpenAI 兼容）"); font.styleName: "Bold"; anchors.top: parent.top; anchors.topMargin: 16; anchors.left: parent.left; anchors.leftMargin: 16 }
            ZybText { text: qsTr("API 地址"); anchors.top: parent.top; anchors.topMargin: 58; anchors.left: parent.left; anchors.leftMargin: 16 }
            Rectangle { anchors.top: parent.top; anchors.topMargin: 82; anchors.left: parent.left; anchors.leftMargin: 16; anchors.right: parent.right; anchors.rightMargin: 16; height: 40; color: "#151619"; radius: 4; TextInput { id: aiEndpointInput; anchors.fill: parent; anchors.margins: 6; color: "white"; font.pixelSize: 15; text: root.aiEndpoint; selectByMouse: true } }
            ZybText { text: qsTr("API Key（留空则保留原值）"); anchors.top: parent.top; anchors.topMargin: 130; anchors.left: parent.left; anchors.leftMargin: 16 }
            Rectangle { anchors.top: parent.top; anchors.topMargin: 154; anchors.left: parent.left; anchors.leftMargin: 16; anchors.right: parent.right; anchors.rightMargin: 16; height: 40; color: "#151619"; radius: 4; TextInput { id: aiKeyInput; anchors.fill: parent; anchors.margins: 6; color: "white"; font.pixelSize: 15; echoMode: TextInput.Password; selectByMouse: true } }
            ZybText { text: qsTr("模型"); anchors.top: parent.top; anchors.topMargin: 202; anchors.left: parent.left; anchors.leftMargin: 16 }
            Rectangle { anchors.top: parent.top; anchors.topMargin: 226; anchors.left: parent.left; anchors.leftMargin: 16; anchors.right: parent.right; anchors.rightMargin: 16; height: 40; color: "#151619"; radius: 4; TextInput { id: aiModelInput; anchors.fill: parent; anchors.margins: 6; color: "white"; font.pixelSize: 15; text: root.aiModel; selectByMouse: true } }
            BasicButton { text: qsTr("取消"); anchors.left: parent.left; anchors.leftMargin: 16; anchors.bottom: parent.bottom; anchors.bottomMargin: 14; width: 90; height: 42; radius: 7; checkable: false; backgroundTheme: root.cardColor; onClicked: root.aiSettingsVisible = false }
            BasicButton { text: qsTr("保存"); anchors.right: parent.right; anchors.rightMargin: 16; anchors.bottom: parent.bottom; anchors.bottomMargin: 14; width: 90; height: 42; radius: 7; checkable: false; backgroundTheme: root.cardColor; onClicked: { if (aiService.saveSettings(aiEndpointInput.text, aiKeyInput.text, aiModelInput.text)) { root.openAi(); root.aiSettingsVisible = false } else root.aiStatus = qsTr("设置保存失败") } }
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
        var values = aiService.settings()
        aiEndpoint = values.endpoint || ""
        aiModel = values.model || ""
        aiKey = ""
        aiStatus = values.hasKey ? qsTr("已配置 API；可开始对话") : qsTr("请先打开设置填写 API Key")
        aiPageVisible = true
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
}