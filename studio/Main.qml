import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Camera2Termux.Studio 1.0

ApplicationWindow {
    id: window
    width: 960
    height: 720
    minimumWidth: 360
    minimumHeight: 520
    visible: true
    title: "Camera2Termux Studio"
    color: "#171C20"

    readonly property color body: "#171C20"
    readonly property color graphite: "#283238"
    readonly property color viewfinder: "#080B0D"
    readonly property color textPrimary: "#E9ECE8"
    readonly property color textMuted: "#AAB4B6"
    readonly property color amber: "#C9A064"
    readonly property color red: "#CE4B4F"
    readonly property bool stacked: width < 850

    component StudioButton: Button {
        id: control
        implicitHeight: 42
        font.pixelSize: 14
        font.weight: Font.Medium
        contentItem: Text {
            text: control.text
            font: control.font
            color: control.enabled ? window.textPrimary : window.textMuted
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 5
            color: control.down ? "#3B484E" : control.hovered ? "#334047" : window.graphite
            border.width: control.activeFocus ? 2 : 1
            border.color: control.activeFocus ? window.amber : "#445258"
        }
    }

    component StudioCombo: ComboBox {
        id: combo
        implicitHeight: 42
        font.pixelSize: 13
        contentItem: Text {
            leftPadding: 12
            rightPadding: 32
            text: combo.displayText
            color: combo.enabled ? window.textPrimary : window.textMuted
            font: combo.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        indicator: Text {
            x: combo.width - width - 12
            anchors.verticalCenter: parent.verticalCenter
            text: "⌄"
            color: window.amber
            font.pixelSize: 18
        }
        background: Rectangle {
            radius: 5
            color: combo.down ? "#3B484E" : window.graphite
            border.width: combo.activeFocus ? 2 : 1
            border.color: combo.activeFocus ? window.amber : "#445258"
        }
        delegate: ItemDelegate {
            id: entry
            width: combo.width
            height: 40
            text: modelData
            font: combo.font
            contentItem: Text {
                text: entry.text
                color: window.textPrimary
                font: entry.font
                verticalAlignment: Text.AlignVCenter
                leftPadding: 12
            }
            background: Rectangle {
                color: entry.highlighted ? "#3B484E" : window.graphite
            }
        }
        popup: Popup {
            y: combo.height
            width: combo.width
            padding: 1
            implicitHeight: Math.min(contentItem.implicitHeight + 2, 420)
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator { }
            }
            background: Rectangle { color: window.graphite; border.color: "#445258" }
        }
    }

    component StudioField: TextField {
        implicitHeight: 42
        color: window.textPrimary
        placeholderTextColor: window.textMuted
        selectionColor: window.amber
        selectedTextColor: window.body
        background: Rectangle {
            radius: 5
            color: window.graphite
            border.width: parent.activeFocus ? 2 : 1
            border.color: parent.activeFocus ? window.amber : "#445258"
        }
    }

    FolderDialog {
        id: folderDialog
        title: "Choose a save folder"
        currentFolder: "file://" + studio.saveFolder
        onAccepted: studio.setSaveFolder(selectedFolder.toString())
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 62
            color: window.body
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                spacing: 12
                Text {
                    text: "CAMERA2TERMUX"
                    color: window.textPrimary
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.5
                }
                Text {
                    text: "STUDIO"
                    color: window.amber
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    font.letterSpacing: 1.5
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: studio.recording ? "●  RECORDING" : "●  LIVE CAPTURE"
                    color: studio.recording ? window.red : window.amber
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#3A4549" }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: window.stacked ? 1 : 2
            rowSpacing: 0
            columnSpacing: 0

            Rectangle {
                id: viewfinderFrame
                Layout.fillWidth: true
                Layout.fillHeight: !window.stacked
                Layout.preferredHeight: window.stacked ? Math.max(240, window.height * 0.43) : -1
                color: window.viewfinder

                CameraPreview {
                    objectName: "previewItem"
                    anchors.fill: parent
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 46
                    color: "#D9171C20"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 16
                        Text {
                            text: "TARGET  " + (studio.targetFps > 0 ? studio.targetFps + " fps" : "unavailable")
                            color: window.amber
                            font.pixelSize: 12
                            font.family: "monospace"
                        }
                        Text {
                            text: "CAPTURE  " + studio.deliveredFps.toFixed(1) + " fps"
                            color: window.textPrimary
                            font.pixelSize: 12
                            font.family: "monospace"
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            visible: studio.recording
                            text: "DROPPED  " + studio.droppedFrames
                            color: window.red
                            font.pixelSize: 12
                            font.family: "monospace"
                        }
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: window.stacked ? -1 : 328
                Layout.fillWidth: window.stacked
                Layout.fillHeight: true
                Layout.minimumHeight: window.stacked ? 400 : 0
                color: window.body
                border.color: "#364248"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    TabBar {
                        id: tabs
                        Layout.fillWidth: true
                        background: Rectangle { color: "transparent" }
                        TabButton {
                            text: "Capture"
                            font.pixelSize: 13
                            contentItem: Text { text: parent.text; color: tabs.currentIndex === 0 ? window.textPrimary : window.textMuted; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: "transparent"; Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: window.amber; visible: tabs.currentIndex === 0 } }
                        }
                        TabButton {
                            text: "Properties"
                            font.pixelSize: 13
                            contentItem: Text { text: parent.text; color: tabs.currentIndex === 1 ? window.textPrimary : window.textMuted; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: "transparent"; Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: window.amber; visible: tabs.currentIndex === 1 } }
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: tabs.currentIndex

                        ScrollView {
                            clip: true
                            contentWidth: availableWidth
                            ColumnLayout {
                                width: parent.width
                                spacing: 10

                                Text { text: "CAMERA"; color: window.textMuted; font.pixelSize: 11; font.weight: Font.DemiBold; font.letterSpacing: 1.2 }
                                StudioCombo {
                                    Layout.fillWidth: true
                                    model: studio.cameras
                                    currentIndex: studio.cameraIndex
                                    enabled: model.length > 0 && !studio.recording
                                    onActivated: function(index) { studio.chooseCamera(index) }
                                }
                                Text { text: "CAPTURE SIZE"; color: window.textMuted; font.pixelSize: 11; font.weight: Font.DemiBold; font.letterSpacing: 1.2 }
                                StudioCombo {
                                    Layout.fillWidth: true
                                    model: studio.sizes
                                    currentIndex: studio.sizeIndex
                                    enabled: model.length > 0 && !studio.recording
                                    onActivated: function(index) { studio.chooseSize(index) }
                                }
                                Text { text: "TARGET FRAME RATE"; color: window.textMuted; font.pixelSize: 11; font.weight: Font.DemiBold; font.letterSpacing: 1.2 }
                                StudioCombo {
                                    Layout.fillWidth: true
                                    model: studio.rates
                                    currentIndex: studio.fpsIndex
                                    enabled: model.length > 0 && !studio.recording
                                    onActivated: function(index) { studio.chooseFps(index) }
                                }
                                Text { text: "ROTATE OUTPUT"; color: window.textMuted; font.pixelSize: 11; font.weight: Font.DemiBold; font.letterSpacing: 1.2 }
                                StudioCombo {
                                    Layout.fillWidth: true
                                    model: ["Sensor default", "+90°", "+180°", "+270°"]
                                    currentIndex: studio.rotationIndex
                                    enabled: !studio.recording
                                    onActivated: function(index) { studio.chooseRotation(index) }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: (studio.rates.length ? "AE target is not delivered FPS. " : "Fixed FPS is unavailable. ") +
                                          (studio.camera2Report.indexOf("sensor orientation degrees:") >= 0 ?
                                           "Sensor rotation assumes portrait; adjust output if needed." :
                                           "Sensor orientation unavailable: output uses raw V4L2 orientation; adjust manually.")
                                    wrapMode: Text.WordWrap
                                    color: window.textMuted
                                    font.pixelSize: 11
                                }
                                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#364248"; Layout.topMargin: 8; Layout.bottomMargin: 8 }
                                Text { text: "SAVE FOLDER"; color: window.textMuted; font.pixelSize: 11; font.weight: Font.DemiBold; font.letterSpacing: 1.2 }
                                RowLayout {
                                    Layout.fillWidth: true
                                    StudioField {
                                        Layout.fillWidth: true
                                        text: studio.saveFolder
                                        selectByMouse: true
                                        onAccepted: studio.setSaveFolder(text)
                                        Accessible.name: "Save folder path"
                                    }
                                    StudioButton { text: "Browse"; onClicked: folderDialog.open() }
                                }
                                StudioButton {
                                    Layout.fillWidth: true
                                    text: "Take photo · PNG"
                                    enabled: studio.cameras.length > 0
                                    onClicked: studio.takePhoto()
                                }
                                StudioButton {
                                    Layout.fillWidth: true
                                    text: studio.recording ? "Stop recording" : "Record silent H.264 · MP4"
                                    enabled: studio.cameras.length > 0
                                    onClicked: studio.toggleRecording()
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "Photo and video use the selected full capture size. Preview alone is scaled to fit."
                                    wrapMode: Text.WordWrap
                                    color: window.textMuted
                                    font.pixelSize: 11
                                }
                            }
                        }

                        ColumnLayout {
                            spacing: 10
                            StudioField {
                                id: search
                                Layout.fillWidth: true
                                placeholderText: "Search properties"
                                Accessible.name: "Search camera properties"
                            }
                            ScrollView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                ColumnLayout {
                                    width: parent.width
                                    spacing: 10
                                    Text { text: "V4L2 PROPERTIES"; color: window.amber; font.pixelSize: 11; font.weight: Font.DemiBold; font.letterSpacing: 1.2 }
                                    Text {
                                        Layout.fillWidth: true
                                        text: studio.filterText(studio.properties, search.text) || "No matching V4L2 properties"
                                        wrapMode: Text.WrapAnywhere
                                        color: window.textPrimary
                                        font.pixelSize: 12
                                        font.family: "monospace"
                                    }
                                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#364248" }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text { text: "AVAILABLE CAMERA2 REPORT"; color: window.amber; font.pixelSize: 11; font.weight: Font.DemiBold; font.letterSpacing: 1.2; Layout.fillWidth: true }
                                        StudioButton { text: "Refresh"; onClicked: studio.refreshCamera2Report() }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: studio.filterText(studio.camera2Report, search.text) || "Refresh to read the current report"
                                        wrapMode: Text.WrapAnywhere
                                        color: window.textPrimary
                                        font.pixelSize: 12
                                        font.family: "monospace"
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: studio.status
                        color: studio.hasError ? window.red : window.textMuted
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        Accessible.name: "Camera status"
                    }
                    StudioButton {
                        Layout.fillWidth: true
                        visible: studio.hasError
                        text: "Reconnect camera"
                        onClicked: studio.reconnect()
                    }
                }
            }
        }
    }
}
