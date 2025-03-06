import PAG
import QtQuick as Quick
import Qt.labs.platform as Labs

MenuBar {
    property bool hasPAGFile: false
    property bool windowActive: true
    property bool isFullscreen: false

    signal command(string command)

    // TODO
    // Shortcuts {
    //     id: shortcuts
    //
    //     visible:false
    // }

    Labs.Menu {
        title: "PAGViewer"
        visible: Qt.platform.os === "osx"

        Labs.MenuItem {
            text: qsTr("了解企业版")
            visible: windowActive
            role: "ApplicationSpecificRole"

            onTriggered: {
                console.log("[Menu] Triggered command: open-commerce-page")
                command('open-commerce-page')
            }
        }
        Labs.MenuItem {
            text: qsTr("检查更新")
            visible: windowActive
            role: "ApplicationSpecificRole"

            onTriggered: {
                console.log("[Menu] Triggered command: check-update")
                command('check-update')
            }
        }
        Labs.MenuItem {
            text: qsTr("删除AE插件")
            visible: windowActive
            role: "ApplicationSpecificRole"
            
            onTriggered: {
                console.log("[Menu] Triggered command: unInstall-ae-plugin")
                command('unInstall-ae-plugin')
            }
        }
        Labs.MenuItem {
            text: qsTr("安装AE插件")
            visible: windowActive
            role: "ApplicationSpecificRole"
            
            onTriggered: {
                console.log("[Menu] Triggered command: install-ae-plugin")
                command('install-ae-plugin')
            }
        }
        Labs.MenuItem {
            text: qsTr("偏好设置")
            visible: windowActive
            role: "ApplicationSpecificRole"
            
            onTriggered: {
                console.log("[Menu] Triggered command: open-preferences")
                command('open-preferences')
            }
        }
        Labs.MenuItem {
            text: qsTr("关于 PAGViewer")
            visible: windowActive
            role: "ApplicationSpecificRole"
            
            onTriggered: {
                console.log("[Menu] Triggered command: open-about")
                command('open-about')
            }
        }
        Labs.MenuItem {
            text: qsTr("关闭")
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
        title: qsTr("文件")
        visible: Qt.platform.os === "osx"

        Labs.MenuItem {
            text: qsTr("打开...")
            visible: windowActive
            shortcut: Labs.StandardKey.Open
            
            onTriggered: {
                console.log("[Menu] Triggered command: open-pag-file")
                command('open-pag-file')
            }
        }
        Labs.MenuItem {
            text: qsTr("性能校验")
            visible: windowActive
            enabled: hasPAGFile
            
            onTriggered: {
                console.log("[Menu] Triggered command: begin-profile")
                command('begin-profile')
            }
        }
        Labs.MenuItem {
            text: qsTr("性能基准测试")
            visible: windowActive
            enabled: hasPAGFile
            
            onTriggered: {
                console.log("[Menu] Triggered command: performance-benchmark")
                command('performance-benchmark')
            }
        }
        Labs.Menu {
            title: qsTr("导出")
            Labs.MenuItem {
                text: qsTr("导出 PNG 序列帧")
                visible: windowActive
                enabled: hasPAGFile
                
                onTriggered: {
                    console.log("[Menu] Triggered command: export-png-sequence")
                    command('export-png-sequence')
                }
            }
            Labs.MenuItem {
                text: qsTr("导出 APNG")
                visible: windowActive
                enabled: hasPAGFile
                
                onTriggered: {
                    console.log("[Menu] Triggered command: export-apng")
                    command('export-apng')
                }
            }
            Labs.MenuItem {
                text: qsTr("导出当前帧为 PNG")
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
        title: qsTr("播放")
        visible: Qt.platform.os === "osx"

        Labs.MenuItem {
            text: qsTr("暂停并回到首帧")
            visible: windowActive
            enabled: hasPAGFile
            shortcut: Labs.StandardKey.MoveToPreviousLine
            
            onTriggered: {
                console.log("[Menu] Triggered command: first-frame")
                command('first-frame')
            }
        }
        Labs.MenuItem {
            text: qsTr("暂停并回到末帧")
            visible: windowActive
            enabled: hasPAGFile
            shortcut: Labs.StandardKey.MoveToNextLine
            
            onTriggered: {
                console.log("[Menu] Triggered command: last-frame")
                command('last-frame')
            }
        }
        Labs.MenuItem {
            text: qsTr("上一帧")
            visible: windowActive
            enabled: hasPAGFile
            shortcut: Labs.StandardKey.MoveToPreviousChar
            
            onTriggered: {
                console.log("[Menu] Triggered command: previous-frame")
                command('previous-frame')
            }
        }
        Labs.MenuItem {
            text: qsTr("下一帧")
            visible: windowActive
            enabled: hasPAGFile
            shortcut: Labs.StandardKey.MoveToNextChar
            
            onTriggered: {
                console.log("[Menu] Triggered command: next-frame")
                command('next-frame')
            }
        }
        Labs.MenuItem {
            text: qsTr("暂停/播放")
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
        title: qsTr("视图")
        visible: Qt.platform.os === "osx"

        Labs.MenuItem {
            text: qsTr("显示/隐藏 背景色")
            visible: windowActive
            enabled: hasPAGFile
            shortcut: "b"
            
            onTriggered: {
                console.log("[Menu] Triggered command: toggle-background")
                command('toggle-background')
            }
        }
        Labs.MenuItem {
            text: qsTr("显示/隐藏 编辑面板")
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
        title: qsTr("窗口")
        visible: Qt.platform.os === "osx"

        Labs.MenuItem {
            text: qsTr("最小化")
            visible: windowActive
            shortcut: "Ctrl+M"
            
            onTriggered: {
                console.log("[Menu] Triggered command: minimize-window")
                command('minimize-window')
            }
        }
        Labs.MenuItem {
            text: qsTr("缩放")
            visible: windowActive
            
            onTriggered: {
                console.log("[Menu] Triggered command: zoom-window")
                command('zoom-window')
            }
        }
        Labs.MenuItem {
            text: isFullscreen? qsTr("退出全屏") : qsTr("全屏")
            visible: windowActive
            
            onTriggered: {
                console.log("[Menu] Triggered command: fullscreen-window")
                command('fullscreen-window')
            }
        }
    }

    Labs.Menu {
        title: qsTr("帮助")
        visible: Qt.platform.os === "osx"

        Labs.MenuItem {
            text: qsTr("PAGViewer 帮助")
            visible: windowActive
            
            onTriggered: {
                console.log("[Menu] Triggered command: show-help")
                command('show-help')
            }
        }
        Labs.MenuItem {
            text: qsTr("问题反馈")
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
