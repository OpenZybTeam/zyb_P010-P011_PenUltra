import QtQuick 2.0
import Z03Plus 1.0

/* Deliberately self-contained: no relative imports of vendor QML modules. */
Rectangle {
    id: root
    anchors.fill: parent
    color: "#101114"
    property string currentPath: ""
    property var entries: []
    property string statusText: "加载中…"
    FileService { id: fileService }

    Rectangle { id: header; width: parent.width; height: 64; color: "#202226" }
    Text { text: "‹"; color: "white"; font.pixelSize: 46; anchors.left: parent.left; anchors.leftMargin: 18; anchors.verticalCenter: header.verticalCenter }
    MouseArea { anchors.left: parent.left; anchors.top: parent.top; width: 70; height: 64; onClicked: StackView.view.pop() }
    Text { text: "文件管理"; color: "white"; font.pixelSize: 24; anchors.centerIn: header }
    Text { text: "上一级"; color: fileService.canGoUp(root.currentPath) ? "#31c7a5" : "#777777"; font.pixelSize: 18; anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: header.verticalCenter }
    MouseArea { anchors.right: parent.right; anchors.top: parent.top; width: 90; height: 64; enabled: fileService.canGoUp(root.currentPath); onClicked: openPath(fileService.parentPath(root.currentPath)) }
    Text { id: location; text: root.currentPath; color: "#aeb4bc"; font.pixelSize: 15; elide: Text.ElideMiddle; anchors.left: parent.left; anchors.right: parent.right; anchors.top: header.bottom; anchors.margins: 16; height: 30; verticalAlignment: Text.AlignVCenter }

    ListView {
        id: list; anchors.top: location.bottom; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 12; clip: true; model: root.entries
        delegate: Rectangle {
            width: list.width; height: 58; color: rowMouse.pressed ? "#343941" : "#202328"; radius: 5
            property var item: modelData
            Text { anchors.left: parent.left; anchors.leftMargin: 14; anchors.right: sizeText.left; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; color: "#f3f4f5"; font.pixelSize: 19; elide: Text.ElideRight; text: (item.isDir ? "[目录] " : "[文件] ") + item.name }
            Text { id: sizeText; anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter; color: "#9da4ad"; font.pixelSize: 15; text: item.isDir ? "" : item.size }
            MouseArea { id: rowMouse; anchors.fill: parent; onClicked: { if (item.isDir) root.openPath(item.path); else if (item.text) StackView.view.push("qrc:/ZybQmlFiles/ZybSet/TextReaderPage.qml", { "filePath": item.path }); else root.statusText = "仅支持 TXT、MD、LRC" } }
        }
        footer: Text { width: list.width; height: 60; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: "#aeb4bc"; font.pixelSize: 18; text: root.entries.length === 0 ? root.statusText : "" }
    }
    function openPath(path) { var r = fileService.openDirectory(path); if (!r.ok) { statusText = r.error; entries = []; return } currentPath = r.path; entries = r.entries; statusText = "" }
    Component.onCompleted: openPath("")
}