import PAG
import QtCore
import QtQuick
import "components"

PAGWindow {
    id: viewWindow
    visible: true
    width: isWindows ? 520 : 500
    height: 360
    minimumWidth: 400 + windowPadding
    minimumHeight: 320 + windowTitleBarHeight
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

    Component.onCompleted: {
        viewWindow.title = "PAGViewer";

        let component = Qt.createComponent("Menu.qml");
        let menuBar = component.createObject(viewWindow, {
            hasPAGFile: Qt.binding(function() { return mainForm.hasPAGFile; }),
            isUseEnglish: Qt.binding(function() { return settings.isUseEnglish; })
        });
        menuBar.command.connect(onCommand);
    }

    function updateProgress() {
        let duration = mainForm.pagView.duration;
        let displayedTime = duration * mainForm.pagView.progress;
        mainForm.controlForm.timeDisplayedText.text = msToTime(displayedTime);
        mainForm.controlForm.currentFrameText.text = mainForm.pagView.currentFrame;
        mainForm.controlForm.totalFrameText.text = mainForm.pagView.totalFrame;
    }
    function msToTime(duration) {
        let seconds = parseInt((duration / 1000) % 60);
        let minutes = parseInt((duration / (1000 * 60)) % 60);
        minutes = (minutes < 10) ? "0" + minutes : minutes;
        seconds = (seconds < 10) ? "0" + seconds : seconds;
        return minutes + ":" + seconds;
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
            willOpen = !mainForm.isEditPanelOpen;
        }
        if (mainForm.controlForm.panelsButton.checked !== willOpen) {
            mainForm.controlForm.panelsButton.checked = willOpen;
        }
    }
    function onCommand(command) {
        console.log(`Get command: [${command}]`);

    }
}
