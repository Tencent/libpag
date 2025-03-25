import QtQuick
import QtQuick.Controls
import "components"

Item {
    id: root

    required property bool hasPAGFile
    required property bool isUseEnglish
    signal command(string command)

    Loader {
        id: windowsMenuLoader
        active: Qt.platform.os === "windows"
        sourceComponent: PAGMenuBar {
            id: windowsMenuBar

            property int menuWidth: root.isUseEnglish ? 300 : 200

            anchors.top: parent.top
            anchors.topMargin: 0
            anchors.left: parent.left
            anchors.leftMargin: 36

            PAGMenu {
                menuWidth: windowsMenuBar.menuWidth
                title: qsTr("File")
                Action {
                    text: qsTr("Open...")
                    shortcut: "Ctrl+O"
                    onTriggered: {
                        root.command("open-pag-file")
                    }
                }
                Action {
                    text: qsTr("Close")
                    shortcut: "Ctrl+Q"
                    onTriggered: {
                        root.command("close-window")
                    }
                }
                Action {
                    text: qsTr("Settings...")
                    onTriggered: {
                        root.command("open-preferences")
                    }
                }
            }

            PAGMenu {
                menuWidth: windowsMenuBar.menuWidth
                title: qsTr("Play")
                Action {
                    text: qsTr("Pause and go to the first frame")
                    enabled: root.hasPAGFile
                    shortcut: StandardKey.MoveToPreviousLine
                    onTriggered: {
                        root.command('first-frame')
                    }
                }
                Action {
                    text: qsTr("Pause and go to the last frame")
                    enabled: root.hasPAGFile
                    shortcut: StandardKey.MoveToNextLine
                    onTriggered: {
                        root.command('last-frame')
                    }
                }
                Action {
                    text: qsTr("Previous frame")
                    enabled: root.hasPAGFile
                    shortcut: StandardKey.MoveToPreviousChar
                    onTriggered: {
                        root.command('previous-frame')
                    }
                }
                Action {
                    text: qsTr("Next frame ")
                    enabled: root.hasPAGFile
                    shortcut: StandardKey.MoveToNextChar
                    onTriggered: {
                        root.command('next-frame')
                    }
                }
                Action {
                    text: qsTr("Pause/Play")
                    enabled: root.hasPAGFile
                    shortcut: "space"
                    onTriggered: {
                        root.command('pause-or-play')
                    }
                }
            }

            PAGMenu {
                menuWidth: windowsMenuBar.menuWidth
                title: qsTr("View")
                Action {
                    text: qsTr("Show/Hide Background")
                    enabled: root.hasPAGFile
                    shortcut: "B"
                    onTriggered: {
                        root.command('toggle-background')
                    }
                }
                Action {
                    text: qsTr("Show/Hide Edit Panel")
                    enabled: root.hasPAGFile
                    shortcut: "L"
                    onTriggered: {
                        root.command('toggle-edit-panel')
                    }
                }
            }

            PAGMenu {
                menuWidth: windowsMenuBar.menuWidth
                title: qsTr("Help")
                Action {
                    text: qsTr("Help for PAGViewer")
                    onTriggered: {
                        root.command('open-help')
                    }
                }
                Action {
                    text: qsTr("About PAGViewer")
                    onTriggered: {
                        root.command('open-about')
                    }
                }
                Action {
                    text: qsTr("Feedback")
                    onTriggered: {
                        root.command('open-feedback')
                    }
                }
                Action {
                    text: qsTr("About PAG Enterprise Edition")
                    onTriggered: {
                        root.command('open-commerce-page')
                    }
                }
            }
        }
    }
}
