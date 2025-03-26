import PAG
import QtCore
import QtQuick
import QtQuick.Dialogs
import Qt.labs.settings
import "components"
import "utils"

PAGWindow {
    id: viewWindow
    visible: true
    width: isWindows ? 520 : 500
    height: 360
    minimumWidth: 400 + windowPadding
    minimumHeight: 320 + windowTitleBarHeight
    hasMenu: true
    resizeHandleSize: 5
    titleBarHeight: windowTitleBarHeight

    property string filePath
    property bool lastPlayStatusIsPlaying: false

    property bool isWindows: Qt.platform.os === 'windows'

    property int windowPadding: isWindows ? 2 : 0

    property int windowTitleBarHeight: isWindows ? 32 : 22

    Settings {
        id: settings
        property bool isEditPanelOpen: false

        property bool isShowVideoFrames: true

        property bool isUseEnglish: true

        property double lastX: 0

        property double lastY: 0
    }
    MainForm {
        id: mainForm
        resizeHandleSize: resizeHandleSize

        pagView {
            showVideoFrames: settings.isShowVideoFrames
            onProgressChanged: function (progress) {
                if (controlForm.progressSlider.value === progress) {
                    return;
                }
                controlForm.progressSlider.value = progress;
                updateProgress();
            }
            onFileChanged: {
                let path = pagView.filePath;
                path = path.replace(/\\/ig, '/').match(/\/([^\/]+)$/)[1];
                viewWindow.title = path;
                viewWindow.filePath = path;
                centerItem.color = pagView.backgroundColor;
                updateProgress();
                let oldX = viewWindow.x;
                let oldY = viewWindow.y;
                let oldWidth = viewWindow.width;
                let oldHeight = viewWindow.height;
                let preferredSize = pagView.preferredSize;
                let width = Math.max(viewWindow.minimumWidth, preferredSize.width);
                let height = Math.max(viewWindow.minimumHeight, preferredSize.height);
                let x = Math.max(0, oldX - ((width - oldWidth) / 2));
                let y = Math.max(50, oldY - ((height - oldHeight) / 2));
                settings.lastX = x;
                settings.lastY = y;
                viewWindow.x = x;
                viewWindow.y = y;
                viewWindow.width = width + windowPadding;
                viewWindow.height = height;
            }
        }
        controlForm {
            progressSlider {
                onValueChanged: {
                    if (pagView.progress === controlForm.progressSlider.value) {
                        return;
                    }
                    pagView.progress = controlForm.progressSlider.value;
                    updateProgress();
                }
                onPressedChanged: {
                    if (controlForm.progressSlider.pressed) {
                        viewWindow.lastPlayStatusIsPlaying = pagView.isPlaying;
                        pagView.isPlaying = false;
                    } else {
                        pagView.isPlaying = viewWindow.lastPlayStatusIsPlaying;
                        viewWindow.lastPlayStatusIsPlaying = false;
                    }
                }
            }
            backgroundButton {
                checked: mainForm.isBackgroundOn
                onClicked: {
                    toggleBackground(mainForm.controlForm.backgroundButton.checked);
                }
            }
            panelsButton {
                onClicked: {
                    toggleEditPanel(mainForm.controlForm.panelsButton.checked);
                }
            }
        }
        dropArea {
            onEntered: function (fileInfo) {
                fileInfo.accept(Qt.CopyAction);
                if (fileInfo.urls[0]) {
                    viewWindow.filePath = fileInfo.urls[0];
                }
            }
            onDropped: function (fileInfo) {
                let path;
                if (viewWindow.filePath === "") {
                    path = fileInfo.source.text;
                } else {
                    path = viewWindow.filePath;
                }
                if (path.endsWith("pag")) {
                    mainForm.pagView.setFile(path);
                }
            }
        }
    }

    FileDialog {
        id: openPAGFileDialog
        visible: false
        title: qsTr("Open PAG File")
        fileMode: FileDialog.OpenFile
        nameFilters: [ "PAG files(*.pag)" ]
        onAccepted: {
            let filePath = openPAGFileDialog.selectedFile
            mainForm.pagView.setFile(filePath);
        }
    }

    SettingsWindow {
        id: settingsWindow
        visible: false
        width: 500
        height: 160
        title: qsTr("Settings")
        useEnglish: settings.isUseEnglish
        onUseEnglishChanged: {
            if (!settingsWindow.visible || settingsWindow.useEnglish === settings.isUseEnglish) {
                return;
            }
            settings.isUseEnglish = settingsWindow.useEnglish;
        }
    }

    AboutWindow {
        id: aboutWindow
        visible: false
        width: settings.isUseEnglish ? 600 : 500
        height: 160 + windowTitleBarHeight
        title: qsTr("About PAGViewer")
        aboutMessage: "<b>PAGViewer</b> " + Qt.application.version + "<br><br>Copyright © 2017-present Tencent. All rights reserved."
    }

    Component.onCompleted: {
        viewWindow.title = "PAGViewer";

        let component = Qt.createComponent("Menu.qml");
        let menuBar = component.createObject(viewWindow, {
            hasPAGFile: Qt.binding(function () {
                return mainForm.hasPAGFile;
            }),
            windowActive: Qt.binding(function () {
                return viewWindow.active;
            }),
            isUseEnglish: Qt.binding(function () {
                return settings.isUseEnglish;
            }),
            isFullScreen: Qt.binding(function () {
                return viewWindow.visibility === Window.FullScreen;
            })
        });
        menuBar.command.connect(onCommand);
    }

    function updateProgress() {
        let duration = mainForm.pagView.duration;
        let displayedTime = duration * mainForm.pagView.progress;
        mainForm.controlForm.timeDisplayedText.text = Utils.msToTime(displayedTime);
        mainForm.controlForm.currentFrameText.text = mainForm.pagView.currentFrame;
        mainForm.controlForm.totalFrameText.text = mainForm.pagView.totalFrame;
    }

    function toggleBackground(checked) {
        if (checked === undefined) {
            checked = !mainForm.isBackgroundOn;
        }
        if (mainForm.isBackgroundOn !== checked) {
            mainForm.isBackgroundOn = checked;
        }
    }

    function toggleEditPanel(willOpen) {
        if (willOpen === undefined) {
            willOpen = !settings.isEditPanelOpen;
        }
        if (mainForm.controlForm.panelsButton.checked !== willOpen) {
            mainForm.controlForm.panelsButton.checked = willOpen;
        }
        settings.isEditPanelOpen = willOpen;
    }

    function onCommand(command) {
        console.log(`Get command: [${command}]`);
        switch (command) {
            case "open-pag-file":
                if (mainForm.hasPAGFile) {
                    let filePath = mainForm.pagView.filePath;
                    openPAGFileDialog.currentFolder = Utils.getFileDir(filePath);
                } else {
                    openPAGFileDialog.currentFolder = StandardPaths.writableLocation(StandardPaths.DocumentsLocation);
                }
                openPAGFileDialog.open();
                break;
            case "close-window":
                viewWindow.close();
                break;
            case "open-preferences":
                settingsWindow.visible = true;
                settingsWindow.raise();
                break;
            case "first-frame":
                mainForm.pagView.firstFrame();
                break;
            case "last-frame":
                mainForm.pagView.lastFrame();
                break;
            case "previous-frame":
                mainForm.pagView.previousFrame();
                break;
            case "next-frame":
                mainForm.pagView.nextFrame();
                break;
            case "pause-or-play":
                mainForm.pagView.isPlaying = !mainForm.pagView.isPlaying;
                break;
            case "toggle-background":
                toggleBackground();
                break;
            case "toggle-edit-panel":
                toggleEditPanel();
                break;
            case "open-help":
                Qt.openUrlExternally("https://pag.art/#pag-player");
                break;
            case "open-about":
                aboutWindow.visible = true;
                aboutWindow.raise();
                break;
            case "open-feedback":
                Qt.openUrlExternally("https://github.com/Tencent/libpag/discussions");
                break;
            case "open-commerce-page":
                Qt.openUrlExternally("https://pag.io/product.html#pag-enterprise-edition");
                break;
            case "minimize-window":
                viewWindow.showMinimized();
                break;
            case "zoom-window":
                viewWindow.visibility = viewWindow.visibility !== Window.Maximized ? Window.Maximized : Window.AutomaticVisibility;
                break;
            case "fullscreen-window":
                viewWindow.visibility = viewWindow.visibility !== Window.Maximized ? Window.Maximized : Window.AutomaticVisibility;
                break;
            default:
                console.log(`Undefined command: [${command}]`)
                break;
        }
    }
}
