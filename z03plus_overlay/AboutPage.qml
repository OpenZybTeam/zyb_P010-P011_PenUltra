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
    signal closeRequested()
    signal openLink(string url)

    property bool updateMirror: false

    UpdateService { id: updater }

    ZybBackButton {
        id: backButton
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 10
        onBackClicked: root.closeRequested()
    }
    ZybText {
        text: qsTr("关于 Z03Ultra")
        font.styleName: "Bold"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: backButton.verticalCenter
    }

    Flickable {
        id: aboutFlick
        anchors.top: backButton.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: 20
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        clip: true
        contentWidth: width
        contentHeight: contentColumn.height + 20
        flickableDirection: Flickable.VerticalFlick

        Column {
            id: contentColumn
            width: aboutFlick.width
            spacing: 12

            Row {
                width: parent.width
                spacing: 14
                Rectangle {
                    width: 62
                    height: 62
                    radius: 14
                    color: "#22262B"
                    ZybText {
                        anchors.centerIn: parent
                        text: "Z3"
                        font.styleName: "Bold"
                        font.pixelSize: 26
                        color: "#31C7A5"
                    }
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3
                    ZybText { text: "Z03Ultra"; font.styleName: "Bold"; font.pixelSize: 24 }
                    ZybText { text: qsTr("Z03 / P010 / P011 词典笔扩展"); color: "#9DA4AD"; font.pixelSize: 14 }
                }
            }

            Rectangle {
                width: parent.width
                height: updateColumn.height + 32
                radius: 10
                color: root.cardColor

                Column {
                    id: updateColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 10

                    Row {
                        width: parent.width
                        spacing: 10
                        ZybText {
                            text: qsTr("当前版本")
                            color: "#9DA4AD"
                            font.pixelSize: 15
                        }
                        ZybText {
                            text: "v" + updater.currentVersion
                            font.styleName: "Bold"
                            font.pixelSize: 15
                        }
                    }

                    ZybText {
                        width: parent.width
                        text: updater.status.length > 0 ? updater.status : qsTr("检测 GitHub Release 是否有新版本")
                        color: updater.state === "error" ? "#FF7A7A"
                             : updater.state === "available" ? "#31C7A5" : "#9DA4AD"
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }

                    Row {
                        width: parent.width
                        spacing: 10
                        visible: updater.busy && updater.state === "downloading"

                        Rectangle {
                            width: parent.width - 60
                            height: 8
                            radius: 4
                            color: "#2A2E33"

                            Rectangle {
                                width: parent.width * Math.min(100, updater.progress) / 100
                                height: parent.height
                                radius: 4
                                color: "#31C7A5"
                            }
                        }

                        ZybText {
                            text: updater.progress + "%"
                            color: "#9DA4AD"
                            font.pixelSize: 13
                        }
                    }

                    Row {
                        width: parent.width
                        spacing: 10

                        BasicButton {
                            width: (parent.width - 10) / 2
                            height: 48
                            radius: 8
                            checkable: false
                            backgroundTheme: root.cardColor
                            text: root.updateMirror ? qsTr("下载源：镜像") : qsTr("下载源：直连")
                            onClicked: root.updateMirror = !root.updateMirror
                        }

                        BasicButton {
                            width: (parent.width - 10) / 2
                            height: 48
                            radius: 8
                            checkable: false
                            backgroundTheme: root.cardColor
                            text: updater.state === "available"
                                  ? qsTr("安装 ") + updater.latestVersion : qsTr("检测更新")
                            enabled: !updater.busy
                            onClicked: {
                                if (updater.state === "available")
                                    updater.installUpdate(root.updateMirror)
                                else
                                    updater.checkForUpdates(root.updateMirror)
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: poweredColumn.height + 32
                radius: 10
                color: root.cardColor
                Column {
                    id: poweredColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 6
                    ZybText { text: qsTr("Powered By OpenZybTeam"); font.styleName: "Bold"; font.pixelSize: 18 }
                    ZybText {
                        width: parent.width
                        text: "https://github.com/OpenZybTeam/zyb_P010-P011_PenUltra"
                        color: "#31C7A5"
                        font.pixelSize: 13
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: basedColumn.height + 32
                radius: 10
                color: root.cardColor
                Column {
                    id: basedColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 6
                    ZybText { text: qsTr("Based On"); font.styleName: "Bold"; font.pixelSize: 18 }
                    ZybText {
                        width: parent.width
                        text: "https://github.com/maozi-114/zyb_P010-P011_PenPlus"
                        color: "#31C7A5"
                        font.pixelSize: 13
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }

            ZybText {
                width: parent.width
                text: qsTr("Z03Ultra 在原厂系统之上通过注入库扩展 QML 页面，保留原厂功能，新增文件管理、文本编辑、浏览器、Nix 应用商店等能力。")
                color: "#9DA4AD"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            BasicButton {
                width: parent.width
                height: 54
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                text: qsTr("打开 OpenZybTeam 项目主页")
                onClicked: root.openLink("https://github.com/OpenZybTeam/zyb_P010-P011_PenUltra")
            }
            BasicButton {
                width: parent.width
                height: 54
                radius: 8
                checkable: false
                backgroundTheme: root.cardColor
                text: qsTr("打开上游项目 PenPlus")
                onClicked: root.openLink("https://github.com/maozi-114/zyb_P010-P011_PenPlus")
            }

            ZybText {
                width: parent.width
                text: qsTr("开源许可与第三方组件说明见项目仓库。")
                color: "#6F7178"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
