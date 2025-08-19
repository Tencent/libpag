import QtQuick
import QtQuick.Controls
import Qt.labs.platform as Platform
import "components"

Item {
    id: root

    required property bool hasPAGFile
    required property bool isUseEnglish
    required property bool windowActive
    required property bool isFullScreen
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
                        root.command("open-pag-file");
                    }
                }
                Action {
                    text: qsTr("Close")
                    shortcut: "Ctrl+Q"
                    onTriggered: {
                        root.command("close-window");
                    }
                }
                Action {
                    text: qsTr("Settings...")
                    onTriggered: {
                        root.command("open-preferences");
                    }
                }
                Action {
                    text: qsTr("Check for Updates")
                    onTriggered: {
                        root.command("check-for-updates");
                    }
                }
                Action {
                    text: qsTr("Performance Test")
                    enabled: root.hasPAGFile
                    onTriggered: {
                        root.command("performance-profile");
                    }
                }
                Action {
                    text: qsTr("Performance Benchmark Test")
                    enabled: root.hasPAGFile
                    onTriggered: {
                        root.command("performance-benchmark");
                    }
                }
                PAGMenu {
                    menuWidth: windowsMenuBar.menuWidth
                    title: qsTr("Export")
                    Action {
                        text: qsTr("Export as PNG Sequence Frames")
                        enabled: root.hasPAGFile
                        onTriggered: {
                            root.command('export-as-png-sequence');
                        }
                    }
                    Action {
                        text: qsTr("Export as APNG")
                        enabled: root.hasPAGFile
                        onTriggered: {
                            root.command('export-as-apng');
                        }
                    }
                    Action {
                        text: qsTr("Export current frame as PNG")
                        enabled: root.hasPAGFile
                        shortcut: "Ctrl+P"
                        onTriggered: {
                            root.command('export-frame-as-png');
                        }
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
                        root.command("first-frame");
                    }
                }
                Action {
                    text: qsTr("Pause and go to the last frame")
                    enabled: root.hasPAGFile
                    shortcut: StandardKey.MoveToNextLine
                    onTriggered: {
                        root.command("last-frame");
                    }
                }
                Action {
                    text: qsTr("Previous frame")
                    enabled: root.hasPAGFile
                    shortcut: StandardKey.MoveToPreviousChar
                    onTriggered: {
                        root.command("previous-frame");
                    }
                }
                Action {
                    text: qsTr("Next frame ")
                    enabled: root.hasPAGFile
                    shortcut: StandardKey.MoveToNextChar
                    onTriggered: {
                        root.command("next-frame");
                    }
                }
                Action {
                    text: qsTr("Pause/Play")
                    enabled: root.hasPAGFile
                    shortcut: "space"
                    onTriggered: {
                        root.command("pause-or-play");
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
                        root.command("toggle-background");
                    }
                }
                Action {
                    text: qsTr("Show/Hide Edit Panel")
                    enabled: root.hasPAGFile
                    shortcut: "L"
                    onTriggered: {
                        root.command("toggle-edit-panel");
                    }
                }
            }

            PAGMenu {
                menuWidth: windowsMenuBar.menuWidth
                title: qsTr("Help")
                Action {
                    text: qsTr("Help for PAGViewer")
                    onTriggered: {
                        root.command("open-help");
                    }
                }
                Action {
                    text: qsTr("About PAGViewer")
                    onTriggered: {
                        root.command("open-about");
                    }
                }
                Action {
                    text: qsTr("Feedback")
                    onTriggered: {
                        root.command("open-feedback");
                    }
                }
                Action {
                    text: qsTr("About PAG Enterprise Edition")
                    onTriggered: {
                        root.command("open-commerce-page");
                    }
                }
                Action {
                    text: qsTr("Install Plugin")
                    onTriggered: {
                        root.command("install-plugin");
                    }
                }
                Action {
                    text: qsTr("Uninstall Plugin")
                    onTriggered: {
                        root.command("uninstall-plugin");
                    }
                }
            }
        }
    }

    Loader {
        id: macosMenuBarLoader
        active: Qt.platform.os === "osx"
        sourceComponent: Platform.MenuBar {
            id: macosMenuBar
            Platform.Menu {
                title: qsTr("PAGViewer")
                Platform.MenuItem {
                    visible: windowActive
                    text: qsTr("About PAG Enterprise Edition")
                    role: "ApplicationSpecificRole"
                    onTriggered: {
                        root.command("open-commerce-page");
                    }
                }
                Platform.MenuItem {
                    visible: windowActive
                    text: qsTr("Check for Updates")
                    role: "ApplicationSpecificRole"
                    onTriggered: {
                        root.command("check-for-updates");
                    }
                }
                Platform.MenuItem {
                    visible: windowActive
                    text: qsTr("Install Plugin")
                    role: "ApplicationSpecificRole"
                    onTriggered: {
                        root.command("install-plugin");
                    }
                }
                Platform.MenuItem {
                    visible: windowActive
                    text: qsTr("Uninstall Plugin")
                    role: "ApplicationSpecificRole"
                    onTriggered: {
                        root.command("uninstall-plugin");
                    }
                }
                Platform.MenuItem {
                    visible: windowActive
                    text: qsTr("Preference Settings")
                    role: "ApplicationSpecificRole"
                    onTriggered: {
                        root.command("open-preferences");
                    }
                }
                Platform.MenuItem {
                    visible: windowActive
                    text: qsTr("About PAGViewer")
                    role: "ApplicationSpecificRole"
                    onTriggered: {
                        root.command("open-about");
                    }
                }
                Platform.MenuItem {
                    text: qsTr("Close")
                    shortcut: StandardKey.Close
                    role: "QuitRole"
                    onTriggered: {
                        root.command("close-window");
                    }
                }
            }
            Platform.Menu {
                title: qsTr("File")
                Platform.MenuItem {
                    text: qsTr("Open...")
                    shortcut: StandardKey.Open
                    onTriggered: {
                        root.command("open-pag-file");
                    }
                }
                Platform.MenuItem {
                    text: qsTr("Performance Test")
                    enabled: root.hasPAGFile
                    onTriggered: {
                        root.command("performance-profile");
                    }
                }
                Platform.MenuItem {
                    text: qsTr("Performance Benchmark Test")
                    enabled: root.hasPAGFile
                    onTriggered: {
                        root.command("performance-benchmark");
                    }
                }
                Platform.Menu {
                    title: qsTr("Export")
                    Platform.MenuItem {
                        text: qsTr("Export as PNG Sequence Frames")
                        enabled: root.hasPAGFile
                        onTriggered: {
                            root.command('export-as-png-sequence');
                        }
                    }
                    Platform.MenuItem {
                        text: qsTr("Export as APNG")
                        enabled: root.hasPAGFile
                        onTriggered: {
                            root.command('export-as-apng');
                        }
                    }
                    Platform.MenuItem {
                        text: qsTr("Export current frame as PNG")
                        enabled: root.hasPAGFile
                        shortcut: "Meta+P"
                        onTriggered: {
                            root.command('export-frame-as-png');
                        }
                    }
                }
            }
            Platform.Menu {
                title: qsTr("Play")
                Platform.MenuItem {
                    text: qsTr("Pause and go to the first frame")
                    enabled: root.hasPAGFile
                    shortcut: StandardKey.MoveToPreviousLine
                    onTriggered: {
                        root.command("first-frame");
                    }
                }
                Platform.MenuItem {
                    text: qsTr("Pause and go to the last frame")
                    enabled: root.hasPAGFile
                    shortcut: StandardKey.MoveToNextLine
                    onTriggered: {
                        root.command("last-frame");
                    }
                }
                Platform.MenuItem {
                    text: qsTr("Previous frame")
                    enabled: root.hasPAGFile
                    shortcut: StandardKey.MoveToPreviousChar
                    onTriggered: {
                        root.command("previous-frame");
                    }
                }
                Platform.MenuItem {
                    text: qsTr("Next frame")
                    enabled: root.hasPAGFile
                    shortcut: StandardKey.MoveToNextChar
                    onTriggered: {
                        root.command("next-frame");
                    }
                }
                Platform.MenuItem {
                    text: qsTr("Pause/Play")
                    enabled: root.hasPAGFile
                    shortcut: "space"
                    onTriggered: {
                        root.command("pause-or-play");
                    }
                }
            }
            Platform.Menu {
                title: qsTr("View")
                Platform.MenuItem {
                    text: qsTr("Show/Hide Background")
                    enabled: root.hasPAGFile
                    shortcut: "B"
                    onTriggered: {
                        root.command("toggle-background");
                    }
                }
                Platform.MenuItem {
                    text: qsTr("Show/Hide Edit Panel")
                    enabled: root.hasPAGFile
                    shortcut: "L"
                    onTriggered: {
                        root.command("toggle-edit-panel");
                    }
                }
            }
            Platform.Menu {
                title: qsTr("Window")
                Platform.MenuItem {
                    text: qsTr("Minimize")
                    shortcut: "Ctrl+M"
                    onTriggered: {
                        root.command("minimize-window");
                    }
                }
                Platform.MenuItem {
                    text: qsTr("Zoom")
                    onTriggered: {
                        root.command("zoom-window");
                    }
                }
                Platform.MenuItem {
                    text: root.isFullScreen ? qsTr("Exit Fullscreen") : qsTr("Fullscreen")
                    visible: root.isFullScreen
                    onTriggered: {
                        root.command("fullscreen-window");
                    }
                }
            }
            Platform.Menu {
                title: qsTr("Help")
                Platform.MenuItem {
                    text: qsTr("Help for PAGViewer")
                    onTriggered: {
                        root.command("open-help");
                    }
                }
                Platform.MenuItem {
                    text: qsTr("Feedback")
                    onTriggered: {
                        root.command("open-feedback");
                    }
                }
            }
        }
    }
}
