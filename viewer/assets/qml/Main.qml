import PAG
import QtCore
import QtQuick
import QtQuick.Dialogs
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.settings
import Qt.labs.platform as Platform
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

    property int minWindowHeightWithEditPanel: 650

    property var contentView: mainForm.contentView
    property var connectedContentView: null

    Settings {
        id: settings
        property bool isEditPanelOpen: false

        property bool isShowVideoFrames: true

        property bool isUseEnglish: true

        property bool isUseBeta: false

        property bool isAutoCheckUpdate: true

        property double lastX: 0

        property double lastY: 0

        property string benchmarkVersion: "0.0.0"

        property string templateAvgRenderingTime: "30000"

        property string templateFirstFrameRenderingTime: "60000"
    }
    MainForm {
        id: mainForm
        resizeHandleSize: resizeHandleSize

        centerItem {
            onWidthChanged: {
                resizeContentView();
            }
            onHeightChanged: {
                resizeContentView();
            }
        }

        contentViewLoader.onLoaded: {
            if (contentView) {
                connectContentViewSignals();
            }
        }

        controlForm {
            progressSlider {
                onValueChanged: {
                    if (!contentView)
                        return;
                    if (contentView.viewModel.progress === controlForm.progressSlider.value) {
                        return;
                    }
                    contentView.viewModel.progress = controlForm.progressSlider.value;
                    updateProgress();
                }
                onPressedChanged: {
                    if (!contentView)
                        return;
                    if (controlForm.progressSlider.pressed) {
                        viewWindow.lastPlayStatusIsPlaying = contentView.viewModel.isPlaying;
                        contentView.viewModel.isPlaying = false;
                    } else {
                        contentView.viewModel.isPlaying = viewWindow.lastPlayStatusIsPlaying;
                        viewWindow.lastPlayStatusIsPlaying = false;
                    }
                }
            }
            updateButton {
                onClicked: {
                    checkForUpdates(false);
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
                let lowerPath = path.toLowerCase();
                if (lowerPath.endsWith(".pag") || lowerPath.endsWith(".pagx")) {
                    mainForm.loadFile(path);
                }
            }
        }
    }

    function onContentViewProgressChanged(progress) {
        if (controlForm.progressSlider.value === progress) {
            return;
        }
        controlForm.progressSlider.value = progress;
        updateProgress();
    }

    function onContentViewFilePathChanged(filePath) {
        let path = contentView.viewModel.filePath;
        path = path.replace(/\\/ig, '/').match(/\/([^\/]+)$/)[1];
        viewWindow.title = path;
        viewWindow.filePath = path;
        centerItem.color = contentView.viewModel.backgroundColor;
        updateProgress();
        let oldX = viewWindow.x;
        let oldY = viewWindow.y;
        let oldWidth = viewWindow.width;
        let oldHeight = viewWindow.height;
        let preferredSize = contentView.viewModel.preferredSize;
        let width = Math.max(viewWindow.minimumWidth, preferredSize.width);
        let height = Math.max(viewWindow.minimumHeight, preferredSize.height + controlForm.height);
        if (mainForm.rightItemLoader.status === Loader.Ready) {
            width += mainForm.rightItemLoader.width + mainForm.splitHandleWidth;
        }
        let x = Math.max(0, oldX - ((width - oldWidth) / 2));
        let y = Math.max(50, oldY - ((height - oldHeight) / 2));
        settings.lastX = x;
        settings.lastY = y;
        viewWindow.x = x;
        viewWindow.y = y;
        viewWindow.width = width + windowPadding;
        viewWindow.height = height;
    }

    function disconnectContentViewSignals() {
        if (!connectedContentView)
            return;
        connectedContentView.viewModel.progressChanged.disconnect(onContentViewProgressChanged);
        connectedContentView.viewModel.filePathChanged.disconnect(onContentViewFilePathChanged);
        connectedContentView = null;
    }

    function connectContentViewSignals() {
        if (!contentView)
            return;
        disconnectContentViewSignals();
        connectedContentView = contentView;
        contentView.viewModel.progressChanged.connect(onContentViewProgressChanged);
        contentView.viewModel.filePathChanged.connect(onContentViewFilePathChanged);

        // Apply showVideoFrames setting
        if (contentView.viewModel.showVideoFrames !== undefined) {
            contentView.viewModel.showVideoFrames = settings.isShowVideoFrames;
        }
    }

    property alias controlForm: mainForm.controlForm
    property alias centerItem: mainForm.centerItem

    SettingsWindow {
        id: settingsWindow
        visible: false
        width: 500
        height: 160 + windowTitleBarHeight
        title: qsTr("Settings")
        autoCheckForUpdates: settings.isAutoCheckUpdate
        useBeta: settings.isUseBeta
        useEnglish: settings.isUseEnglish
        onUseEnglishChanged: {
            if (!settingsWindow.visible || settingsWindow.useEnglish === settings.isUseEnglish) {
                return;
            }
            settings.isUseEnglish = settingsWindow.useEnglish;
        }
        onAutoCheckForUpdatesChanged: {
            if (!settingsWindow.visible || settingsWindow.autoCheckForUpdates === settings.isAutoCheckUpdate) {
                return;
            }
            settings.isAutoCheckUpdate = settingsWindow.autoCheckForUpdates;
        }
        onUseBetaChanged: {
            if (!settingsWindow.visible || settingsWindow.useBeta === settings.isUseBeta) {
                return;
            }
            settings.isUseBeta = settingsWindow.useBeta;
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

    PAGTaskFactory {
        id: taskFactory
        objectName: "taskFactory"
    }

    PluginInstallerModel {
        id: pluginInstaller
        objectName: "pluginInstaller"
    }

    FileDialog {
        id: openFileDialog

        property var currentAcceptHandler: null

        visible: false
        title: ""
        fileMode: FileDialog.OpenFile
        nameFilters: []
    }

    Platform.FolderDialog {
        id: openFolderDialog

        property var currentAcceptHandler: null

        visible: false
        title: qsTr("Select Save Path")
    }

    Timer {
        id: startupTimer
        repeat: false
        interval: 1000
        onTriggered: {
            if (settings.isAutoCheckUpdate) {
                checkForUpdates(true);
            }
        }
    }

    Timer {
        id: updateTimer
        repeat: true
        interval: 1000 * 60 * 60 * 24
        onTriggered: {
            checkForUpdates(true);
        }
    }

    PAGWindow {
        id: progressWindow

        property var task
        property alias progressBar: progressBar

        width: 300
        height: 64
        minimumWidth: width
        maximumWidth: width
        minimumHeight: height
        maximumHeight: height
        hasMenu: false
        canResize: false
        titleBarHeight: windowTitleBarHeight
        visible: false

        PAGRectangle {
            id: rectangle

            color: "#2D2D37"
            anchors.fill: parent
            leftTopRadius: false
            rightTopRadius: false
            radius: 5

            ProgressBar {
                id: progressBar
                width: parent.width - 24
                height: 30
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
                value: 0

                contentItem: Item {
                    Rectangle {
                        width: parent.width
                        height: 15
                        radius: 5
                        color: "#DDDDDD"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle {
                        width: progressBar.visualPosition * parent.width
                        height: 15
                        radius: 5
                        color: "#448EF9"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        anchors.centerIn: parent
                        text: Math.round(progressBar.value * 100) + "%"
                        color: progressBar.value > 0.5 ? "white" : "black"
                        font.pixelSize: 12
                    }
                }
            }
        }

        onClosing: {
            if (task) {
                task.stop();
            }
        }
    }

    PAGMessageBox {
        id: benchmarkCompleteMessageBox
        width: 500
        visible: false
        height: 130 + windowTitleBarHeight
        textSize: 12
        title: qsTr("Performance Benchmark Test")
        message: qsTr("Performance Benchmark Test Complete")
    }

    BusyIndicator {
        id: benchmarkBusyIndicator
        running: false
    }

    Connections {
        id: taskConnections
        target: null

        function onProgressChanged(progress) {
            progressWindow.progressBar.value = progress;
        }

        function onVisibleChanged(visible) {
            progressWindow.visible = visible;
        }

        function onTaskFinished(filePath, result) {
            if (result !== 0) {
                let errStr = qsTr("Export failed, error code: ");
                alert(errStr + result);
            }
            progressWindow.task = null;
            progressWindow.progressBar.value = 0;
            progressWindow.visible = false;
        }
    }

    Connections {
        target: benchmarkModel

        function onBenchmarkComplete(isAuto, templateAvgRenderingTime, templateFirstFrameRenderingTime) {
            settings.templateAvgRenderingTime = templateAvgRenderingTime;
            settings.templateFirstFrameRenderingTime = templateFirstFrameRenderingTime;

            benchmarkBusyIndicator.visible = false;
            benchmarkBusyIndicator.running = false;

            if (isAuto) {
                settings.benchmarkVersion = Qt.application.version;
            } else {
                benchmarkCompleteMessageBox.visible = true;
                benchmarkCompleteMessageBox.raise();
            }
        }
    }

    Component.onCompleted: {
        viewWindow.title = "PAGViewer";

        let component = Qt.createComponent("Menu.qml");
        let menuBar = component.createObject(viewWindow, {
            hasPAGFile: Qt.binding(function () {
                return mainForm.hasPAGFile;
            }),
            hasAnimation: Qt.binding(function () {
                return mainForm.hasAnimation;
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

        pagWindow.requestOpenFile.connect(function (filePath) {
            mainForm.loadFile(filePath);
        });

        startupTimer.start();
        updateTimer.start();

        connectContentViewSignals();
    }

    function updateProgress() {
        if (!contentView)
            return;
        mainForm.controlForm.timeDisplayedText.text = contentView.viewModel.displayedTime;
        mainForm.controlForm.currentFrameText.text = contentView.viewModel.currentFrame;
        mainForm.controlForm.totalFrameText.text = contentView.viewModel.totalFrame;
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
        mainForm.isEditPanelOpen = willOpen;
        if (willOpen) {
            let widthChange = Math.max(mainForm.rightItemLoader.width, mainForm.minPanelWidth);
            if (viewWindow.visibility === Window.FullScreen) {
                mainForm.centerItem.width = viewWindow.width - widthChange;
            } else {
                viewWindow.width = viewWindow.width + widthChange + mainForm.splitHandleWidth;
            }
            if (viewWindow.height < minWindowHeightWithEditPanel) {
                viewWindow.height = minWindowHeightWithEditPanel;
            }
        } else {
            let widthChange = -1 * mainForm.rightItemLoader.width;
            if (viewWindow.visibility === Window.FullScreen) {
                mainForm.centerItem.width = viewWindow.width;
            } else if ((viewWindow.width + widthChange) < viewWindow.minimumWidth) {
                viewWindow.width = viewWindow.minimumWidth;
            } else {
                viewWindow.width = viewWindow.width + widthChange - mainForm.splitHandleWidth;
            }
        }
    }

    function resizeContentView() {
        if (!contentView)
            return;
        let width = contentView.viewModel.width;
        let height = contentView.viewModel.height;
        let windowWidth = mainForm.centerItem.width;
        let windowHeight = mainForm.centerItem.height - mainForm.controlForm.height;
        let finalHeight = 1;
        let finalWidth = 1;
        let pagIsNarrower = height / width > windowHeight / windowWidth;
        if (pagIsNarrower) {
            finalHeight = windowHeight;
            finalWidth = finalHeight / height * width;
        } else {
            finalWidth = windowWidth;
            finalHeight = finalWidth / width * height;
        }
        mainForm.contentViewLoader.item.width = finalWidth;
        mainForm.contentViewLoader.item.height = finalHeight;
        mainForm.contentViewLoader.item.x = (windowWidth - finalWidth) / 2;
        mainForm.contentViewLoader.item.y = (windowHeight - finalHeight) / 2;
    }

    function updateAvailable(hasNewVersion) {
        mainForm.controlForm.updateAvailable = hasNewVersion;
    }

    function checkForUpdates(keepSilent) {
        checkUpdateModel.checkForUpdates(keepSilent, settings.isUseBeta);
    }

    function onCommand(command) {
        switch (command) {
        case "open-pag-file":
            if (mainForm.hasPAGFile) {
                let filePath = contentView.viewModel.filePath;
                openFileDialog.currentFolder = Utils.getFileDir(filePath);
            } else {
                openFileDialog.currentFolder = StandardPaths.writableLocation(StandardPaths.DocumentsLocation);
            }
            if (openFileDialog.currentAcceptHandler) {
                openFileDialog.accepted.disconnect(openFileDialog.currentAcceptHandler);
            }
            openFileDialog.fileMode = FileDialog.OpenFile;
            openFileDialog.title = qsTr("Open PAG File");
            openFileDialog.nameFilters = ["PAG files(*.pag *.pagx)"];
            openFileDialog.currentAcceptHandler = function () {
                let filePath = openFileDialog.selectedFile;
                mainForm.loadFile(filePath);
            };
            openFileDialog.accepted.connect(openFileDialog.currentAcceptHandler);
            openFileDialog.open();
            break;
        case "close-window":
            viewWindow.close();
            break;
        case "open-preferences":
            settingsWindow.visible = true;
            settingsWindow.raise();
            break;
        case "first-frame":
            if (contentView)
                contentView.viewModel.firstFrame();
            break;
        case "last-frame":
            if (contentView)
                contentView.viewModel.lastFrame();
            break;
        case "previous-frame":
            if (contentView)
                contentView.viewModel.previousFrame();
            break;
        case "next-frame":
            if (contentView)
                contentView.viewModel.nextFrame();
            break;
        case "pause-or-play":
            if (contentView)
                contentView.viewModel.isPlaying = !contentView.viewModel.isPlaying;
            break;
        case "toggle-background":
            toggleBackground();
            break;
        case "toggle-edit-panel":
            toggleEditPanel();
            break;
        case "open-help":
            Qt.openUrlExternally("https://pag.io/#pag-player");
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
        case "install-plugin":
            pluginInstaller.installPlugin();
            break;
        case "uninstall-plugin":
            pluginInstaller.uninstallPlugin();
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
        case "export-frame-as-png":
            if (openFileDialog.currentAcceptHandler) {
                openFileDialog.accepted.disconnect(openFileDialog.currentAcceptHandler);
            }
            openFileDialog.fileMode = FileDialog.SaveFile;
            openFileDialog.title = qsTr("Select save path");
            openFileDialog.nameFilters = ["PNG files(*.png)"];
            openFileDialog.defaultSuffix = "png";
            openFileDialog.currentFolder = Utils.getFileDir(contentView.viewModel.filePath);
            openFileDialog.currentAcceptHandler = function () {
                let filePath = openFileDialog.selectedFile;
                let task = taskFactory.createTask(PAGTaskFactory.PAGTaskType_ExportPNG, filePath, {
                    "exportFrame": contentView.viewModel.currentFrame
                });
                if (task) {
                    taskConnections.target = task;
                    progressWindow.title = qsTr("Exporting");
                    progressWindow.task = task;
                    progressWindow.visible = true;
                    progressWindow.raise();
                    task.start();
                }
            };
            openFileDialog.accepted.connect(openFileDialog.currentAcceptHandler);
            openFileDialog.open();
            break;
        case "export-as-png-sequence":
            if (openFolderDialog.currentAcceptHandler) {
                openFolderDialog.accepted.disconnect(openFolderDialog.currentAcceptHandler);
            }
            openFolderDialog.title = qsTr("Select save path");
            openFolderDialog.currentFolder = Utils.getFileDir(contentView.viewModel.filePath);
            openFolderDialog.currentAcceptHandler = function () {
                let filePath = openFolderDialog.folder;
                let task = taskFactory.createTask(PAGTaskFactory.PAGTaskType_ExportPNG, filePath);
                if (task) {
                    taskConnections.target = task;
                    progressWindow.title = qsTr("Exporting");
                    progressWindow.progressBar.value = 0;
                    progressWindow.task = task;
                    progressWindow.visible = true;
                    progressWindow.raise();
                    task.start();
                }
            };
            openFolderDialog.accepted.connect(openFolderDialog.currentAcceptHandler);
            openFolderDialog.open();
            break;
        case "export-as-apng":
            if (openFileDialog.currentAcceptHandler) {
                openFileDialog.accepted.disconnect(openFileDialog.currentAcceptHandler);
            }
            openFileDialog.fileMode = FileDialog.SaveFile;
            openFileDialog.title = qsTr("Select save path");
            openFileDialog.nameFilters = ["APNG files(*.png)"];
            openFileDialog.defaultSuffix = "png";
            openFileDialog.currentFolder = Utils.getFileDir(contentView.viewModel.filePath);
            openFileDialog.currentAcceptHandler = function () {
                let filePath = openFileDialog.selectedFile;
                let task = taskFactory.createTask(PAGTaskFactory.PAGTaskType_ExportAPNG, filePath);
                if (task) {
                    taskConnections.target = task;
                    progressWindow.title = qsTr("Exporting");
                    progressWindow.progressBar.value = 0;
                    progressWindow.task = task;
                    progressWindow.visible = true;
                    progressWindow.raise();
                    task.start();
                }
            };
            openFileDialog.accepted.connect(openFileDialog.currentAcceptHandler);
            openFileDialog.open();
            break;
        case "check-for-updates":
            checkForUpdates(false);
            break;
        case "performance-profile":
            let task = taskFactory.createTask(PAGTaskFactory.PAGTaskType_Profiling, contentView.viewModel.filePath);
            if (task) {
                taskConnections.target = task;
                progressWindow.title = qsTr("Profiling");
                progressWindow.progressBar.value = 0;
                progressWindow.task = task;
                progressWindow.visible = true;
                progressWindow.raise();
                task.start();
            }
            break;
        case "performance-benchmark":
            if (contentView)
                contentView.viewModel.isPlaying = false;
            benchmarkBusyIndicator.visible = true;
            benchmarkBusyIndicator.running = true;
            benchmarkModel.startBenchmarkOnTemplate(false);
            break;
        default:
            break;
        }
    }
}
