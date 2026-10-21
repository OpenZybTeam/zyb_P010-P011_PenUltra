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
    property string searchText: ""
    property string attribute: ""
    property string status: ""
    property string activeField: ""
    property int installSeconds: 0
    signal closeRequested()

    function elapsedText() {
        var m = Math.floor(installSeconds / 60)
        var sec = installSeconds % 60
        return m + ":" + (sec < 10 ? "0" + sec : "" + sec)
    }

    NixService { id: nix }

    function doSearch() {
        var text = searchInput.text.trim()
        if (text.length === 0) {
            root.status = qsTr("请输入搜索词")
            return
        }
        nix.search(text)
    }

    function doInstall() {
        var text = attributeInput.text.trim()
        if (text.length === 0) {
            root.status = qsTr("请输入包属性，例如 nixpkgs.hello")
            return
        }
        nix.installPackage(text)
    }

    ZybBackButton {
        id: backButton
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 10
        onBackClicked: {
            if (nix.busy)
                nix.cancel()
            root.closeRequested()
        }
    }
    ZybText {
        text: qsTr("应用商店 · Nix")
        font.styleName: "Bold"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: backButton.verticalCenter
    }
    ZybText {
        text: nix.busy ? (nix.progress > 0 ? qsTr("安装 ") + nix.progress + "%" : qsTr("执行中…"))
                       : (nix.installed ? qsTr("已安装") : qsTr("未安装"))
        color: nix.busy ? "#E0A24A" : (nix.installed ? "#31C7A5" : "#E05A5A")
        font.pixelSize: 12
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: backButton.verticalCenter
    }

    Rectangle {
        id: statusCard
        anchors.top: backButton.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        height: nix.installed ? 0 : (nix.progress > 0 ? 148 : 92)
        visible: !nix.installed
        radius: 8
        color: root.cardColor
        ZybText {
            text: qsTr("Nix 未安装。安装会从清华 TUNA 镜像下载 Nix 2.9.2（armv7l），并添加 nixos-22.05 频道；安装目录优先放到 /home/nix，再软链接为 /nix；过程约数分钟。")
            width: parent.width - 150
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: "#B8B8B8"
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.top: parent.top
            anchors.topMargin: 14
        }
        BasicButton {
            text: nix.busy ? qsTr("安装中…") : qsTr("安装 Nix")
            width: 118
            height: 46
            radius: 8
            checkable: false
            enabled: !nix.busy
            backgroundTheme: Qt.rgba(0.19, 0.78, 0.65, 0.7)
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.top: parent.top
            anchors.topMargin: 14
            onClicked: {
                root.installSeconds = 0
                nix.install()
            }
        }
        Rectangle {
            id: progressBox
            visible: nix.progress > 0
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 10
            height: 40
            color: "transparent"
            ZybText {
                id: stageLabel
                text: nix.stage + (nix.progress < 100 ? "  ·  已用 " + root.elapsedText() : "")
                color: "#D0D0D0"
                font.pixelSize: 11
                elide: Text.ElideRight
                width: parent.width - 52
                anchors.left: parent.left
                anchors.top: parent.top
            }
            ZybText {
                text: nix.progress + "%"
                color: nix.progress >= 100 ? "#31C7A5" : "#E0A24A"
                font.pixelSize: 11
                anchors.right: parent.right
                anchors.top: parent.top
            }
            Rectangle {
                id: progressBg
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: stageLabel.bottom
                anchors.topMargin: 5
                height: 7
                radius: 3
                color: "#1E1F24"
                Rectangle {
                    width: progressBg.width * Math.max(0, Math.min(100, nix.progress)) / 100
                    height: parent.height
                    radius: 3
                    color: nix.progress >= 100 ? "#31C7A5" : "#E0A24A"
                    Behavior on width { NumberAnimation { duration: 250 } }
                }
            }
        }
    }

    Rectangle {
        id: searchBox
        anchors.top: statusCard.bottom
        anchors.topMargin: nix.installed ? 10 : 8
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: parent.right
        anchors.rightMargin: 14
        height: 42
        radius: 8
        color: "#292A2E"
        TextInput {
            id: searchInput
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: searchButton.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            color: "#F2F2F2"
            font.pixelSize: 14
            clip: true
            selectByMouse: true
            text: root.searchText
            onTextChanged: root.searchText = text
            onAccepted: root.doSearch()
        }
        MouseArea {
            anchors.fill: searchInput
            onClicked: {
                searchInput.forceActiveFocus()
                root.activeField = "search"
                overlayNixKeyboard.showKeyboard()
            }
        }
        ZybText {
            visible: searchInput.text.length === 0
            text: qsTr("搜索软件包，如 htop")
            color: "#999999"
            font.pixelSize: 14
            anchors.left: searchInput.left
            anchors.verticalCenter: searchInput.verticalCenter
        }
        BasicButton {
            id: searchButton
            text: qsTr("搜索")
            width: 72
            height: 34
            radius: 7
            checkable: false
            enabled: nix.installed && !nix.busy
            backgroundTheme: root.cardColor
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.doSearch()
        }
    }

    Rectangle {
        id: installBox
        anchors.top: searchBox.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: parent.right
        anchors.rightMargin: 14
        height: 42
        radius: 8
        color: "#292A2E"
        TextInput {
            id: attributeInput
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: installButton.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            color: "#F2F2F2"
            font.pixelSize: 14
            clip: true
            selectByMouse: true
            text: root.attribute
            onTextChanged: root.attribute = text
            onAccepted: root.doInstall()
        }
        MouseArea {
            anchors.fill: attributeInput
            onClicked: {
                attributeInput.forceActiveFocus()
                root.activeField = "attribute"
                overlayNixKeyboard.showKeyboard()
            }
        }
        ZybText {
            visible: attributeInput.text.length === 0
            text: qsTr("包属性，如 nixpkgs.htop")
            color: "#999999"
            font.pixelSize: 14
            anchors.left: attributeInput.left
            anchors.verticalCenter: attributeInput.verticalCenter
        }
        BasicButton {
            id: installButton
            text: qsTr("安装")
            width: 72
            height: 34
            radius: 7
            checkable: false
            enabled: nix.installed && !nix.busy
            backgroundTheme: root.cardColor
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.doInstall()
        }
    }

    Flickable {
        id: quickFlick
        anchors.top: installBox.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: parent.right
        anchors.rightMargin: 14
        height: 30
        contentWidth: quickPackages.width
        clip: true
        flickableDirection: Flickable.HorizontalFlick
        Row {
            id: quickPackages
            spacing: 6
            Repeater {
                model: ["nixpkgs.hello", "nixpkgs.htop", "nixpkgs.neofetch", "nixpkgs.git",
                        "nixpkgs.curl", "nixpkgs.python3", "nixpkgs.ffmpeg"]
                Rectangle {
                    height: 28
                    width: quickPackageLabel.implicitWidth + 16
                    radius: 14
                    color: "#1E1F24"
                    border.color: "#3A3B42"
                    border.width: 1
                    Text {
                        id: quickPackageLabel
                        anchors.centerIn: parent
                        text: modelData
                        color: "#CCCCCC"
                        font.pixelSize: 11
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            attributeInput.text = modelData
                            root.attribute = modelData
                        }
                    }
                }
            }
        }
    }

    Row {
        id: actionRow
        anchors.top: quickFlick.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 14
        spacing: 8
        BasicButton {
            text: qsTr("已安装")
            width: 86
            height: 32
            radius: 6
            checkable: false
            enabled: nix.installed && !nix.busy
            backgroundTheme: root.cardColor
            onClicked: nix.listInstalled()
        }
        BasicButton {
            text: qsTr("更新频道")
            width: 98
            height: 32
            radius: 6
            checkable: false
            enabled: nix.installed && !nix.busy
            backgroundTheme: root.cardColor
            onClicked: nix.updateChannels()
        }
        BasicButton {
            text: qsTr("停止")
            width: 72
            height: 32
            radius: 6
            checkable: false
            visible: nix.busy
            backgroundTheme: Qt.rgba(0.7, 0.18, 0.18, 0.7)
            onClicked: nix.cancel()
        }
        BasicButton {
            text: qsTr("清空日志")
            width: 92
            height: 32
            radius: 6
            checkable: false
            backgroundTheme: root.cardColor
            onClicked: nix.clearLog()
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
        anchors.bottomMargin: 10
        radius: 8
        color: "#151619"
        clip: true

        ScrollView {
            anchors.fill: parent
            anchors.margins: 8
            clip: true
            TextArea {
                id: logArea
                readOnly: true
                wrapMode: TextEdit.WrapAnywhere
                color: "#C8C8C8"
                font.family: "monospace"
                font.pixelSize: 11
                selectByMouse: true
                background: Rectangle { color: "transparent" }
            }
        }
    }

    Connections {
        target: nix
        onLogChanged: {
            logArea.text = nix.log
            logArea.cursorPosition = logArea.length
        }
        onFinished: {
            root.status = (ok ? qsTr("完成：") : qsTr("失败：")) + message
            if (!ok)
                root.installSeconds = 0
        }
        onInstalledChanged: if (nix.installed) root.status = qsTr("Nix 已就绪")
    }

    ZybKeyBoard {
        id: overlayNixKeyboard
        z: 300
        width: root.width
        height: root.height
        y: Style.gHeight
        visible: y < Style.gHeight
        onCommitInput: {
            if (root.activeField === "search") {
                searchInput.text = t1
                searchInput.cursorPosition = t1.length
                root.searchText = t1
            } else if (root.activeField === "attribute") {
                attributeInput.text = t1
                attributeInput.cursorPosition = t1.length
                root.attribute = t1
            }
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: nix.busy && nix.progress > 0
        onTriggered: root.installSeconds++
    }

    Component.onCompleted: {
        nix.detect()
        logArea.text = nix.log
        logArea.cursorPosition = logArea.length
        root.status = nix.installed ? qsTr("Nix 已就绪") : qsTr("请先安装 Nix")
    }
}
