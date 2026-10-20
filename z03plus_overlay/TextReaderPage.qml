import QtQuick 2.0
import Z03Ultra 1.0

/* Self-contained reader: external RCC cannot rely on vendor relative imports. */
Rectangle {
    id: root
    anchors.fill: parent
    color: "#101114"
    property string filePath: ""
    property string content: ""
    property string errorText: ""
    TextFileService { id: textFileService }
    Rectangle { id: header; width: parent.width; height: 64; color: "#202226" }
    Text { text: "‹"; color: "white"; font.pixelSize: 46; anchors.left: parent.left; anchors.leftMargin: 18; anchors.verticalCenter: header.verticalCenter }
    MouseArea { anchors.left: parent.left; anchors.top: parent.top; width: 70; height: 64; onClicked: StackView.view.pop() }
    Text { text: textFileService.fileName(root.filePath); color: "white"; font.pixelSize: 21; elide: Text.ElideMiddle; anchors.left: parent.left; anchors.leftMargin: 70; anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: header.verticalCenter }
    Flickable {
        id: scroll; anchors.top: header.bottom; anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right; anchors.margins: 16; clip: true; contentWidth: width; contentHeight: reader.height + 12; flickableDirection: Flickable.VerticalFlick
        Text { id: reader; width: scroll.width; text: root.errorText || root.content; color: "#f0f1f2"; font.pixelSize: 19; wrapMode: Text.Wrap; textFormat: Text.PlainText }
    }
    function load() { var r = textFileService.readText(filePath); if (!r.ok) { errorText = r.error; content = "" } else { content = r.content; errorText = "" } }
    Component.onCompleted: load()
}