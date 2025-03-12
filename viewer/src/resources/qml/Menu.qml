import PAG
import QtQuick as Quick
import Qt.labs.platform as Labs

MenuBar {
    property bool hasPAGFile: false
    property bool windowActive: true
    property bool isFullscreen: false

    signal command(string command)

    // TODO Merge macos menu and windows menu

    Labs.Menu {
        title: "PAGViewer"
        visible: Qt.platform.os === "osx"

        Labs.MenuItem {
            text: qsTr("About PAG Enterprise Edition")
            visible: windowActive
            role: "ApplicationSpecificRole"

            onTriggered: {
                console.log("[Menu] Triggered command: open-commerce-page")
                command('open-commerce-page')
            }
        }
        Labs.MenuItem {
            text: qsTr("Check Update Information")
            visible: windowActive
            role: "ApplicationSpecificRole"

            onTriggered: {
                console.log("[Menu] Triggered command: check-update")
                command('check-update')
            }
        }
        Labs.MenuItem {
            text: qsTr("Uninstall AE Plug-in")
            visible: windowActive
            role: "ApplicationSpecificRole"
            
            onTriggered: {
                console.log("[Menu] Triggered command: unInstall-ae-plugin")
                command('unInstall-ae-plugin')
            }
        }
        Labs.MenuItem {
            text: qsTr("Install AE Plug-in")
            visible: windowActive
            role: "ApplicationSpecificRole"
            
            onTriggered: {
                console.log("[Menu] Triggered command: install-ae-plugin")
                command('install-ae-plugin')
            }
        }
        Labs.MenuItem {
            text: qsTr("Preference Settings")
            visible: windowActive
            role: "ApplicationSpecificRole"
            
            onTriggered: {
                console.log("[Menu] Triggered command: open-preferences")
                command('open-preferences')
            }
        }
        Labs.MenuItem {
            text: qsTr("About PAGViewer")
            visible: windowActive
            role: "ApplicationSpecificRole"
            
            onTriggered: {
                console.log("[Menu] Triggered command: open-about")
                command('open-about')
            }
        }
        Labs.MenuItem {
            text: qsTr("Close")
            visible: windowActive
            shortcut: Labs.StandardKey.Close
            role: "QuitRole"
            
            onTriggered: {
                console.log("[Menu] Triggered command: close-window")
                command('close-window')
            }
        }
    }

    Labs.Menu {
        title: qsTr("File")
        visible: Qt.platform.os === "osx"

        Labs.MenuItem {
            text: qsTr("Open...")
            visible: windowActive
            shortcut: Labs.StandardKey.Open
            
            onTriggered: {
                console.log("[Menu] Triggered command: open-pag-file")
                command('open-pag-file')
            }
        }
        Labs.MenuItem {
            text: qsTr("Performance Test")
            visible: windowActive
            enabled: hasPAGFile
            
            onTriggered: {
                console.log("[Menu] Triggered command: begin-profile")
                command('begin-profile')
            }
        }
        Labs.MenuItem {
            text: qsTr("Performance Benchmark Test")
            visible: windowActive
            enabled: hasPAGFile
            
            onTriggered: {
                console.log("[Menu] Triggered command: performance-benchmark")
                command('performance-benchmark')
            }
        }
        Labs.Menu {
            title: qsTr("Export")
            Labs.MenuItem {
                text: qsTr("Export as PNG Sequence Frames")
                visible: windowActive
                enabled: hasPAGFile
                
                onTriggered: {
                    console.log("[Menu] Triggered command: export-png-sequence")
                    command('export-png-sequence')
                }
            }
            Labs.MenuItem {
                text: qsTr("Export as APNG")
                visible: windowActive
                enabled: hasPAGFile
                
                onTriggered: {
                    console.log("[Menu] Triggered command: export-apng")
                    command('export-apng')
                }
            }
            Labs.MenuItem {
                text: qsTr("Export Current Frame as PNG")
                visible: windowActive
                enabled: hasPAGFile
                shortcut: "Meta+P"
                
                onTriggered: {
                    console.log("[Menu] Triggered command: export-frame-as-png")
                    command('export-frame-as-png')
                }
            }
        }
    }

    Labs.Menu {
        title: qsTr("Play")
        visible: Qt.platform.os === "osx"

        Labs.MenuItem {
            text: qsTr("Pause and go back to the first frame")
            visible: windowActive
            enabled: hasPAGFile
            shortcut: Labs.StandardKey.MoveToPreviousLine
            
            onTriggered: {
                console.log("[Menu] Triggered command: first-frame")
                command('first-frame')
            }
        }
        Labs.MenuItem {
            text: qsTr("Pause and go back to the last frame")
            visible: windowActive
            enabled: hasPAGFile
            shortcut: Labs.StandardKey.MoveToNextLine
            
            onTriggered: {
                console.log("[Menu] Triggered command: last-frame")
                command('last-frame')
            }
        }
        Labs.MenuItem {
            text: qsTr("Previous frame")
            visible: windowActive
            enabled: hasPAGFile
            shortcut: Labs.StandardKey.MoveToPreviousChar
            
            onTriggered: {
                console.log("[Menu] Triggered command: previous-frame")
                command('previous-frame')
            }
        }
        Labs.MenuItem {
            text: qsTr("Next frame")
            visible: windowActive
            enabled: hasPAGFile
            shortcut: Labs.StandardKey.MoveToNextChar
            
            onTriggered: {
                console.log("[Menu] Triggered command: next-frame")
                command('next-frame')
            }
        }
        Labs.MenuItem {
            text: qsTr("Pause/Play")
            visible: windowActive
            enabled: hasPAGFile
            shortcut: " "
            
            onTriggered: {
                console.log("[Menu] Triggered command: pause-or-play")
                command('pause-or-play')
            }
        }
    }

    Labs.Menu {
        title: qsTr("View")
        visible: Qt.platform.os === "osx"

        Labs.MenuItem {
            text: qsTr("Show/Hide BackColor")
            visible: windowActive
            enabled: hasPAGFile
            shortcut: "b"
            
            onTriggered: {
                console.log("[Menu] Triggered command: toggle-background")
                command('toggle-background')
            }
        }
        Labs.MenuItem {
            text: qsTr("Show/Hide Edit Panel")
            visible: windowActive
            enabled: hasPAGFile
            shortcut: "l"
            
            onTriggered: {
                console.log("[Menu] Triggered command: toggle-edit-panel")
                command('toggle-edit-panel')
            }
        }
    }

    Labs.Menu {
        title: qsTr("Window")
        visible: Qt.platform.os === "osx"

        Labs.MenuItem {
            text: qsTr("Minimize")
            visible: windowActive
            shortcut: "Ctrl+M"
            
            onTriggered: {
                console.log("[Menu] Triggered command: minimize-window")
                command('minimize-window')
            }
        }
        Labs.MenuItem {
            text: qsTr("Zoom")
            visible: windowActive
            
            onTriggered: {
                console.log("[Menu] Triggered command: zoom-window")
                command('zoom-window')
            }
        }
        Labs.MenuItem {
            text: isFullscreen? qsTr("Exit the Full-screen") : qsTr("Full-screen")
            visible: windowActive
            
            onTriggered: {
                console.log("[Menu] Triggered command: fullscreen-window")
                command('fullscreen-window')
            }
        }
    }

    Labs.Menu {
        title: qsTr("Help")
        visible: Qt.platform.os === "osx"

        Labs.MenuItem {
            text: qsTr("Help for PAGViewer")
            visible: windowActive
            
            onTriggered: {
                console.log("[Menu] Triggered command: show-help")
                command('show-help')
            }
        }
        Labs.MenuItem {
            text: qsTr("Feedback")
            visible: windowActive
            
            onTriggered: {
                console.log("[Menu] Triggered command: show-report-issue")
                command('show-report-issue')
            }
        }
    }
    // Component.onCompleted: {
    //     shortcuts.command.connect(command)
    // }
}
