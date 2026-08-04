import PAG
import QtCore
import QtQuick
import QtQuick.Window
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
    minimumHeight: 320 + windowTitleBarHeight + contentHeightPadding
    hasMenu: true
    resizeHandleSize: 5
    titleBarHeight: windowTitleBarHeight

    property string filePath
    property bool lastPlayStatusIsPlaying: false

    property bool isWindows: Qt.platform.os === 'windows'

    property int windowPadding: isWindows ? 2 : 0

    property int windowTitleBarHeight: isWindows ? 32 : 22

    // Chrome = title bar + control bar. The content canvas below them has a height of
    // (window height - chromeHeight), so window/canvas conversions go through this single value.
    readonly property int chromeHeight: windowTitleBarHeight + controlForm.height

    // On Windows the placeholder is inset by 1px at the bottom (see components/PAGWindow.qml),
    // so the window must be 1px taller than chrome + canvas for the canvas to match the file
    // exactly. This mirrors windowPadding, which compensates the 1px left/right insets.
    readonly property int contentHeightPadding: isWindows ? 1 : 0

    // Minimum window height while the side edit panel is open, so the panel stays usable.
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
        let panelOpen = mainForm.rightItemLoader.status === Loader.Ready;
        let geometry = computeWindowGeometry(preferredSize, panelOpen);
        let width = geometry.width;
        let height = geometry.height;
        let x = Math.max(0, oldX - ((width - oldWidth) / 2));
        let y = Math.max(50, oldY - ((height - oldHeight) / 2));
        settings.lastX = x;
        settings.lastY = y;
        viewWindow.x = x;
        viewWindow.y = y;
        viewWindow.width = width;
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
            pluginInstaller.checkPluginOnStartup();
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

        pagWindow.requestOpenFile.connect(mainForm.loadFile);

        if (shouldRunStartupTasks) {
            startupTimer.start();
            updateTimer.start();
        }

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

        let preferredSize = contentView ? contentView.viewModel.preferredSize : Qt.size(0, 0);
        if (viewWindow.visibility === Window.FullScreen) {
            // Full screen: the window size is fixed, so fit the canvas inside the available
            // content area at the file aspect ratio instead of resizing the window.
            applyFullScreenCanvas(preferredSize, willOpen);
            return;
        }
        // Recompute the whole window geometry so that both dimensions stay consistent with the
        // file aspect ratio. Opening or closing the panel goes through the same path, which
        // avoids the letterboxing that a linear width add/subtract reintroduced on close.
        let geometry = computeWindowGeometry(preferredSize, willOpen);
        viewWindow.width = geometry.width;
        viewWindow.height = geometry.height;
    }

    // Fits the canvas inside the available content area when the window size is fixed (full
    // screen). The window cannot be resized, so the canvas itself is sized to the file aspect
    // ratio within the area left of the panel; MainForm centers the content inside centerItem.
    function applyFullScreenCanvas(preferredSize, panelOpen) {
        let panelWidth = panelOpen ? Math.max(mainForm.rightItemLoader.width, mainForm.minPanelWidth)
            + mainForm.splitHandleWidth : 0;
        mainForm.centerItem.width = viewWindow.width - panelWidth;
    }

    function resizeContentView() {
        if (!contentView)
            return;
        let windowWidth = mainForm.centerItem.width;
        let windowHeight = mainForm.centerItem.height - mainForm.controlForm.height;
        mainForm.contentViewLoader.item.width = windowWidth;
        mainForm.contentViewLoader.item.height = windowHeight;
        mainForm.contentViewLoader.item.x = 0;
        mainForm.contentViewLoader.item.y = 0;
    }

    // Computes the window {width, height} that fits a canvas of the file aspect ratio.
    //
    // The width direction previously had no upper bound: reverse-deriving the width from the
    // canvas height for extreme aspect ratios (e.g. 1920x200) produced widths far larger than
    // any screen. This function keeps both dimensions self-consistent and clamps them to the
    // available screen size: when the width is capped it reverse-derives the height instead of
    // blowing up the width, and vice versa, so the canvas always matches the file ratio without
    // letterboxing.
    function computeWindowGeometry(preferredSize, panelOpen) {
        let availW = Screen.desktopAvailableWidth;
        let availH = Screen.desktopAvailableHeight;
        let panelWidth = panelOpen ? Math.max(mainForm.rightItemLoader.width, mainForm.minPanelWidth)
            + mainForm.splitHandleWidth : 0;

        if (!(preferredSize.width > 0) || !(preferredSize.height > 0)) {
            // preferredSize can be {0,0} before the file is loaded or the window has no screen
            // yet; avoid dividing by zero and just fall back to the minimum plus the panel.
            let fallbackW = Math.max(viewWindow.minimumWidth, viewWindow.minimumWidth + panelWidth);
            let fallbackH = panelOpen ? Math.max(viewWindow.minimumHeight, minWindowHeightWithEditPanel)
                : viewWindow.minimumHeight;
            return {
                "width": Math.min(fallbackW, availW),
                "height": Math.min(fallbackH, availH)
            };
        }

        let ratio = preferredSize.width / preferredSize.height;

        // Start from the file height (or the panel minimum height) and derive the width.
        let canvasHeight = preferredSize.height;
        if (panelOpen) {
            canvasHeight = Math.max(canvasHeight, minWindowHeightWithEditPanel - chromeHeight);
        }
        let canvasWidth = canvasHeight * ratio;

        let winW = canvasWidth + panelWidth + windowPadding;
        let winH = canvasHeight + chromeHeight + contentHeightPadding;

        // Width capped by the screen: reverse-derive the height so the ratio still holds.
        if (winW > availW) {
            canvasWidth = availW - panelWidth - windowPadding;
            canvasHeight = canvasWidth / ratio;
            winW = availW;
            winH = canvasHeight + chromeHeight + contentHeightPadding;
        }
        // Height capped by the screen: reverse-derive the width.
        if (winH > availH) {
            canvasHeight = availH - chromeHeight - contentHeightPadding;
            canvasWidth = canvasHeight * ratio;
            winH = availH;
            winW = canvasWidth + panelWidth + windowPadding;
        }

        // Minimum width clamp: keep the ratio by reverse-deriving the height, so the clamp does
        // not reintroduce top/bottom letterboxing for narrow-tall files. The reverse-derived
        // height is still bounded by the screen: when the minimum width and the screen height
        // cannot both hold at the file ratio, we accept the screen bound (some letterboxing)
        // rather than let the window run off-screen.
        if (winW < viewWindow.minimumWidth) {
            winW = viewWindow.minimumWidth;
            canvasWidth = winW - panelWidth - windowPadding;
            canvasHeight = canvasWidth / ratio;
            winH = canvasHeight + chromeHeight + contentHeightPadding;
            winH = Math.min(winH, availH);
        }
        // Minimum height clamp: reverse-derive the width for the same reason, and likewise bound
        // the reverse-derived width by the screen so the window never exceeds it (dropping the
        // ratio only when the minimum height and the screen width cannot both hold).
        if (winH < viewWindow.minimumHeight) {
            winH = viewWindow.minimumHeight;
            canvasHeight = winH - chromeHeight - contentHeightPadding;
            canvasWidth = canvasHeight * ratio;
            winW = canvasWidth + panelWidth + windowPadding;
            winW = Math.min(winW, availW);
        }

        return {
            "width": Math.round(winW),
            "height": Math.round(winH)
        };
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
        case "zoom-in":
            if (contentView)
                contentView.zoomAt(1.5, contentView.width / 2, contentView.height / 2);
            break;
        case "zoom-out":
            if (contentView)
                contentView.zoomAt(1.0 / 1.5, contentView.width / 2, contentView.height / 2);
            break;
        case "reset-zoom":
            if (contentView)
                contentView.resetView();
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
