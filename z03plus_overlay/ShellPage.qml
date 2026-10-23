import QtQuick 2.0
import QtQuick.Controls 2.3
import "../ZybControls/"
import "../ZybVirturlKeyBoard"
import "../Style"
import Z03Ultra 1.0

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0F0F0F"
    property color cardColor: Qt.rgba(60 / 255, 61 / 255, 66 / 255, 0.6)
    property string command: ""
    property string status: ""
    property string activeField: ""
    signal closeRequested()

    ShellService { id: shell }

    function appendOutput(text) {
        outputArea.text += text
        if (outputArea.text.length > 200000)
            outputArea.text = outputArea.text.slice(-120000)
        outputArea.cursorPosition = outputArea.length
    }

    function execute() {
        var cmd = commandInput.text.trim()
        if (cmd.length === 0) {
            root.status = qsTr("请输入命令")
            return
        }
        root.command = cmd
        root.status = ""
        appendOutput("\n$ " + cmd + "\n")
        if (!shell.run(cmd, 60000))
            root.status = qsTr("命令启动失败")
    }

    ZybBackButton {
        id: backButton
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 10
        onBackClicked: {
            if (shell.running)
                shell.cancel()
            root.closeRequested()
        }
    }
    ZybText {
        text: qsTr("终端 Shell")
        font.styleName: "Bold"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: backButton.verticalCenter
    }
    ZybText {
        text: shell.running ? qsTr("运行中…") : qsTr("空闲")
        color: shell.running ? "#31C7A5" : "#8E9099"
        font.pixelSize: 12
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: backButton.verticalCenter
    }

    Rectangle {
        id: quickBar
        anchors.top: backButton.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: parent.right
        anchors.rightMargin: 14
        height: 30
        color: "transparent"
        Flickable {
            anchors.fill: parent
            contentWidth: quickRow.width
            clip: true
            flickableDirection: Flickable.HorizontalFlick
            Row {
                id: quickRow
                spacing: 6
                Repeater {
                    model: ["ls /", "df -h", "free -m", "uname -a", "ps", "date"]
                    Rectangle {
                        width: quickLabel.implicitWidth + 18
                        height: 28
                        radius: 14
                        color: "#1E1F24"
                        border.color: "#3A3B42"
                        border.width: 1
                        Text {
                            id: quickLabel
                            anchors.centerIn: parent
                            text: modelData
                            color: "#CCCCCC"
                            font.pixelSize: 12
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                commandInput.text = modelData
                                root.command = modelData
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: inputBox
        anchors.top: quickBar.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: parent.right
        anchors.rightMargin: 14
        height: 46
        radius: 8
        color: "#292A2E"
        TextInput {
            id: commandInput
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: runButton.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            color: "#F2F2F2"
            font.family: "monospace"
            font.pixelSize: 14
            clip: true
            selectByMouse: true
            text: root.command
            onTextChanged: root.command = text
            onAccepted: root.execute()
        }
        MouseArea {
            anchors.fill: commandInput
            onClicked: {
                commandInput.forceActiveFocus()
                root.activeField = "command"
                Qt.inputMethod.show()
            }
        }
        ZybText {
            visible: commandInput.text.length === 0
            text: qsTr("输入 shell 命令…")
            color: "#999999"
            font.pixelSize: 14
            anchors.left: commandInput.left
            anchors.verticalCenter: commandInput.verticalCenter
        }
        BasicButton {
            id: runButton
            text: qsTr("执行")
            width: 74
            height: 34
            radius: 7
            checkable: false
            backgroundTheme: root.cardColor
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.execute()
        }
    }

    Row {
        id: actionRow
        anchors.top: inputBox.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 14
        spacing: 8
        BasicButton {
            text: qsTr("清空输出")
            width: 108
            height: 34
            radius: 6
            checkable: false
            backgroundTheme: root.cardColor
            onClicked: outputArea.text = ""
        }
        BasicButton {
            text: qsTr("停止")
            width: 88
            height: 34
            radius: 6
            checkable: false
            visible: shell.running
            backgroundTheme: Qt.rgba(0.7, 0.18, 0.18, 0.7)
            onClicked: shell.cancel()
        }
        ZybText {
            text: root.status
            color: "#E0A24A"
            font.pixelSize: 12
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Rectangle {
        anchors.top: actionRow.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        radius: 8
        color: "#151619"
        clip: true

        ScrollView {
            anchors.fill: parent
            anchors.margins: 8
            clip: true
            TextArea {
                id: outputArea
                readOnly: true
                wrapMode: TextEdit.WrapAnywhere
                color: "#D8D8D8"
                font.family: "monospace"
                font.pixelSize: 12
                selectByMouse: true
                background: Rectangle { color: "transparent" }
            }
        }
    }

    Connections {
        target: shell
        onOutput: root.appendOutput(text)
        onFinished: {
            root.status = qsTr("退出码：") + exitCode
            if (output.length > 0 && outputArea.text.length === 0)
                root.appendOutput(output)
        }
    }


    Component.onCompleted: commandInput.forceActiveFocus()
}
