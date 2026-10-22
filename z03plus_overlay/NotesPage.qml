import QtQuick 2.0
import QtQuick.Controls 2.3
import "../ZybControls/"
import "../Style"
import Z03Ultra 1.0

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0F0F0F"
    property color cardColor: Qt.rgba(60 / 255, 61, 66 / 255, 0.6)
    property bool eraser: false
    property color inkColor: "#1B1B1B"
    property int brushSize: 4
    property var strokes: []
    property var currentStroke: []
    property string status: ""
    property bool galleryVisible: false
    property var notes: []
    signal closeRequested()

    NoteService { id: noteService }

    function clearAll() { strokes = []; currentStroke = []; canvas.requestPaint() }
    function undo() { strokes.pop(); canvas.requestPaint() }
    function refreshNotes() { notes = noteService.list() }
    function saveNote() {
        var url = canvas.toDataURL("image/png")
        var path = noteService.save(url)
        if (path.length > 0) {
            status = qsTr("已保存 ") + path
            refreshNotes()
        } else {
            status = qsTr("保存失败")
        }
    }

    Component.onCompleted: refreshNotes()

    ZybBackButton {
        id: backButton
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 10
        onBackClicked: root.closeRequested()
    }
    ZybText {
        text: qsTr("手写便签")
        font.styleName: "Bold"
        font.pixelSize: 17
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: backButton.verticalCenter
    }

    Row {
        id: toolbar
        anchors.top: backButton.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: parent.right
        anchors.rightMargin: 14
        spacing: 8

        Repeater {
            model: [
                { "label": qsTr("黑"), "color": "#1B1B1B", "eraser": false },
                { "label": qsTr("红"), "color": "#C62828", "eraser": false },
                { "label": qsTr("蓝"), "color": "#1565C0", "eraser": false },
                { "label": qsTr("橡皮"), "color": "#FFFFFF", "eraser": true }
            ]
            Rectangle {
                width: 54
                height: 32
                radius: 8
                color: (modelData.eraser ? root.eraser : (!root.eraser && root.inkColor === modelData.color)) ? "#2FA98D" : "#1B1E24"
                border.width: 1
                border.color: "#2B2F38"
                ZybText {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: (modelData.eraser ? root.eraser : (!root.eraser && root.inkColor === modelData.color)) ? "#0F0F0F" : "#CCCCCC"
                    font.pixelSize: 12
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.eraser = modelData.eraser
                        if (!modelData.eraser)
                            root.inkColor = modelData.color
                    }
                }
            }
        }
        Repeater {
            model: [2, 4, 8]
            Rectangle {
                width: 33
                height: 32
                radius: 8
                color: root.brushSize === modelData ? "#2FA98D" : "#1B1E24"
                border.width: 1
                border.color: "#2B2F38"
                ZybText {
                    anchors.centerIn: parent
                    text: String(modelData)
                    color: root.brushSize === modelData ? "#0F0F0F" : "#CCCCCC"
                    font.pixelSize: 12
                }
                MouseArea { anchors.fill: parent; onClicked: root.brushSize = modelData }
            }
        }
        Rectangle {
            width: 50
            height: 32
            radius: 8
            color: "#1B1E24"
            border.width: 1
            border.color: "#2B2F38"
            ZybText { anchors.centerIn: parent; text: qsTr("撤销"); color: "#CCCCCC"; font.pixelSize: 12 }
            MouseArea { anchors.fill: parent; onClicked: root.undo() }
        }
        Rectangle {
            width: 50
            height: 32
            radius: 8
            color: "#1B1E24"
            border.width: 1
            border.color: "#2B2F38"
            ZybText { anchors.centerIn: parent; text: qsTr("清空"); color: "#FF9A9A"; font.pixelSize: 12 }
            MouseArea { anchors.fill: parent; onClicked: root.clearAll() }
        }
    }

    Rectangle {
        id: paper
        anchors.top: toolbar.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.bottom: actionRow.top
        anchors.bottomMargin: 8
        radius: 10
        color: "#FFFFFF"
        clip: true
        border.width: 1
        border.color: "#2B2F38"

        Canvas {
            id: canvas
            anchors.fill: parent
            anchors.margins: 1
            onPaint: {
                var ctx = getContext("2d")
                ctx.fillStyle = "#FFFFFF"
                ctx.fillRect(0, 0, width, height)
                var all = root.strokes.slice()
                if (root.currentStroke.length > 1)
                    all.push(root.currentStroke)
                for (var i = 0; i < all.length; i++) {
                    var stroke = all[i]
                    if (stroke.length < 2)
                        continue
                    ctx.strokeStyle = stroke[0].eraser ? "#FFFFFF" : stroke[0].color
                    ctx.lineWidth = stroke[0].size
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(stroke[0].x, stroke[0].y)
                    for (var j = 1; j < stroke.length; j++)
                        ctx.lineTo(stroke[j].x, stroke[j].y)
                    ctx.stroke()
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            onPressed: {
                root.currentStroke = [{ "x": mouse.x, "y": mouse.y, "color": root.inkColor,
                                        "size": root.eraser ? root.brushSize * 3 : root.brushSize,
                                        "eraser": root.eraser }]
                canvas.requestPaint()
            }
            onPositionChanged: {
                if (!pressed || root.currentStroke.length === 0)
                    return
                root.currentStroke.push({ "x": mouse.x, "y": mouse.y })
                canvas.requestPaint()
            }
            onReleased: {
                if (root.currentStroke.length > 1) {
                    var copy = root.currentStroke.slice()
                    root.strokes = root.strokes.concat([copy])
                }
                root.currentStroke = []
                canvas.requestPaint()
            }
            onCanceled: { root.currentStroke = []; canvas.requestPaint() }
        }
    }

    Row {
        id: actionRow
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        spacing: 8
        BasicButton {
            width: (parent.width - 16) / 2
            height: 40
            text: qsTr("保存为图片")
            checkable: false
            backgroundTheme: Qt.rgba(0.19, 0.62, 0.55, 0.85)
            onClicked: root.saveNote()
        }
        BasicButton {
            width: (parent.width - 16) / 2
            height: 40
            text: root.galleryVisible ? qsTr("收起便签") : qsTr("查看便签")
            checkable: false
            backgroundTheme: root.cardColor
            onClicked: { root.refreshNotes(); root.galleryVisible = !root.galleryVisible }
        }
    }

    Rectangle {
        visible: root.galleryVisible
        anchors.fill: parent
        color: "#E6000000"
        z: 40
        MouseArea { anchors.fill: parent; onClicked: root.galleryVisible = false }
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 40, 520)
            height: Math.min(parent.height - 80, 420)
            radius: 14
            color: "#171A21"
            border.width: 1
            border.color: "#2B2F38"
            Column {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8
                ZybText { text: qsTr("已保存的便签"); color: "#F2F3F5"; font.pixelSize: 15; font.styleName: "Bold" }
                ListView {
                    width: parent.width
                    height: parent.height - 40
                    clip: true
                    model: root.notes
                    spacing: 8
                    delegate: Rectangle {
                        width: parent.width
                        height: 64
                        radius: 10
                        color: "#1B1E24"
                        border.width: 1
                        border.color: "#2B2F38"
                        Image {
                            x: 8
                            width: 46
                            height: 46
                            anchors.verticalCenter: parent.verticalCenter
                            source: "file://" + modelData.path
                            fillMode: Image.PreserveAspectFit
                        }
                        Column {
                            anchors.left: parent.left
                            anchors.leftMargin: 64
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            ZybText { text: modelData.name; color: "#F2F3F5"; font.pixelSize: 13 }
                            ZybText { text: modelData.modified; color: "#8E9099"; font.pixelSize: 11 }
                        }
                        Rectangle {
                            width: 52
                            height: 28
                            radius: 8
                            color: "#2A2E38"
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            ZybText { anchors.centerIn: parent; text: qsTr("删除"); color: "#FF9A9A"; font.pixelSize: 12 }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: { noteService.removeNote(modelData.path); root.refreshNotes() }
                            }
                        }
                    }
                }
            }
        }
    }

    ZybText {
        visible: root.status.length > 0
        text: root.status
        color: "#7FE0C8"
        font.pixelSize: 11
        elide: Text.ElideRight
        width: parent.width - 40
        anchors.bottom: actionRow.top
        anchors.bottomMargin: 2
        anchors.horizontalCenter: parent.horizontalCenter
        horizontalAlignment: Text.AlignHCenter
    }
}
