import PAG
import QtCore
import QtQuick
import QtQuick.Window
import QtQuick.Dialogs
import QtQuick.Controls
import Qt.labs.platform as Lab11

PApplicationWindow {
    id: mainWindow

    property var warn1: qsTr("导出失败，错误码:")
    property var pagFilePath
    property var taskType
    property var exportLocation
    property bool checkUpdateSilent: false
    property int currentFrame: 0
    property int qWindowMaximize: 4
    property int qWindowAutomatic: 1
    property int qWindowFullscreen: 5
    property int windowResizeDuration: 200
    property int windowHeightEditPanelOpen: 650
    property int windowPadding: (Qt.platform.os === 'windows') ? 2 : 0
    property int windowTitleBarHeight: (Qt.platform.os === 'windows') ? 32 : 22
    property string filePath

    width: Qt.platform.os === 'windows' ? 520 : 500
    height: 360
    visible: true
    hasMenu: true
    canResize: true
    minimumWidth: 400 + windowPadding
    minimumHeight: 320 + windowTitleBarHeight

    Settings {
        id: settings

        property bool isUseBeta: false
        property bool isFirstRun: true
        property bool isEditPanelOpen: false
        property bool isShowVideoFrames: true
        property bool isUseEnglish: false
        property bool isAutoCheckUpdate: true
        property int uiAvgRenderingTime: 30000
        property int uiFirstFrameRenderingTime: 60000
        property int templateAvgRenderingTime: 30000
        property int templateFirstFrameRenderingTime: 60000
        // TODO
        // property int currentBenchmarkSceneType: PAGBenchmarkModel.BENCHMARK_SCENE_TYPE_TEMPLATE
        property double lastX: 0
        property double lastY: 0
        property double lastCheckUpdateTime: 0
        property string hadBenchmarkVersion: "0.0.0"
    }

    MainForm {
        id: mainForm

        objectName: "main"

        controlForm {
            btnBackground {
                checked: mainForm.isBackgroundOn

                onCheckedChanged: {
                    toggleBackground(mainForm.controlForm.btnBackground.checked)
                }
            }

            sliderProgress {
                onValueChanged: {
                    if(mainForm.controlForm.progress === mainForm.controlForm.sliderProgress.value) {
                        return
                    }
                    mainForm.controlForm.progress = mainForm.controlForm.sliderProgress.value
                    updateProgressText()
                }

                onPressedChanged : {
                    let pressed = mainForm.controlForm.sliderProgress.pressed
                    if(pressed) {
                        mainForm.controlForm.lastPlayStatus = mainForm.pagViewer.isPlaying
                        mainForm.pagViewer.isPlaying = false
                    } else {
                        mainForm.pagViewer.isPlaying = mainForm.controlForm.lastPlayStatus
                        mainForm.controlForm.lastPlayStatus = false
                    }
                }
            }

            btnNext.onClicked: {
                pagViewer.nextFrame()
            }

            btnPanels.onClicked: {
                toggleEditPanel(mainForm.controlForm.btnPanels.checked)
            }

            btnPrevious.onClicked: {
                pagViewer.previousFrame()
            }

            onIsPlayingChanged: {
                if(controlForm.isPlaying === pagViewer.isPlaying) {
                    return
                }
                pagViewer.isPlaying = controlForm.isPlaying
            }

            onProgressChanged: {
                if(mainForm.controlForm.sliderProgress.value !== mainForm.controlForm.progress) {
                    mainForm.controlForm.sliderProgress.value = mainForm.controlForm.progress
                }
                updateProgressText()
            }
        }

        dropArea {
            onEntered: function(drag) {
                drag.accept (Qt.CopyAction)
                if(drag.urls[0]) {
                    mainWindow.filePath = (drag.urls[0])
                    console.log("Drop Area entered filePath: ", mainWindow.filePath)
                }
            }

            onDropped: function(drag) {
                let path
                if(mainWindow.filePath !== "") {
                    path = filePath
                } else {
                    path = drag.source.text
                }

                if (path.endsWith("pag")) {
                    mainForm.pagViewer.setFile(path)
                } else {
                    mainForm.pagViewer.replaceImageAtPoint(path, drag.x - mainForm.pagViewer.x, drag.y - mainForm.pagViewer.y)
                }
            }
        }

        SequentialAnimation on isEditPanelOpen {
            id: panelHide

            running: false

            PauseAnimation {
                duration: windowResizeDuration
            }

            PropertyAction {
                value: false
            }

            PropertyAction {
                target: mainForm
                property: "playerMinWidth"
                value:mainForm.playerDefaultMinWidth
            }
        }

        SequentialAnimation on isEditPanelOpen {
            id: panelShow

            running: false

            PropertyAction {
                value:true
            }

            PauseAnimation {
                duration: windowResizeDuration
            }

            PropertyAction {
                target: mainForm
                property: "playerMinWidth"
                value:mainForm.playerDefaultMinWidth
            }
        }

        pagViewer {
            showVideoFrames: settings.isShowVideoFrames

            onIsPlayingChanged: function(v) {
                if(controlForm.isPlaying === pagViewer.isPlaying) {
                    return
                }
                controlForm.isPlaying = pagViewer.isPlaying
            }

            onProgressChanged: function(progress) {
                if(mainForm.controlForm.progress === progress) {
                    return
                }
                mainForm.controlForm.progress = progress
            }

            onFileChanged: {
                let path = mainForm.pagViewer.filePath

                console.log("File changed to:", path)
                path = path.replace(/\\/ig,'/').match(/\/([^\/]+)$/)[1]
                mainWindow.title = path
                mainForm.profilerForm.pagFileChanged(mainForm.pagViewer.filePath)
                mainWindow.filePath = mainForm.pagViewer.filePath

                mainForm.hasPAGFile = true
                mainForm.controlForm.hasPAGFile = true
                mainForm.centerItem.color = mainForm.pagViewer.backgroundColor
                updateProgressText()

                let px = mainWindow.x
                let py = mainWindow.y
                let sw = mainWindow.width
                let sh = mainWindow.height
                let preferredSize = pagViewer.preferredSize
                let height = controlForm.height + mainWindow.windowTitleBarHeight + preferredSize.height
                let y = Math.max(50, py - ((height - sh) / 2))
                if(settings.lastY === y){
                    y += 25
                }
                settings.lastY = y

                let minWidth = mainWindow.minimumWidth
                let width = Math.max(minWidth, preferredSize.width)
                if(mainForm.isEditPanelOpen){
                    let rightPanelWidth = (rightItem.width === 0) ? mainForm.panelDefaultWidth : rightItem.width
                    width += (rightPanelWidth + mainForm.splitHandleWidth)
                }

                let x = Math.max(0, px - ((width - sw ) / 2))
                if(settings.lastX === x){
                    x += 25
                }
                settings.lastX = x
                mainWindow.x = x
                mainWindow.y = y
                mainWindow.width = width + windowPadding
                mainWindow.height = height

                mainForm.profilerForm.performanceWarnLeftDialog.close()
                mainForm.profilerForm.performanceWarnRightDialog.close()

                // TODO
                // benchmarkTimer.start()
            }
        }

        centerItem {
            onWidthChanged: {
                resizePAGViewer()
            }

            onHeightChanged: {
                resizePAGViewer()
            }
        }

        rightItem {
            width:0
        }

        textListContainer {
            visible: pagViewer.textCount > 0
        }

        btnToggleTextList {
            onClicked: {
                mainForm.textListOpen = mainForm.btnToggleTextList.checked
            }

            Behavior on rotation {
                NumberAnimation {
                    duration: windowResizeDuration
                    easing.type: Easing.InCurve
                }
            }
        }

        imageListContainer {
            visible: pagViewer.imageCount > 0
        }

        btnToggleImageList {
            onClicked: {
                mainForm.imageListOpen = mainForm.btnToggleImageList.checked
            }

            Behavior on rotation {
                NumberAnimation {
                    duration: windowResizeDuration
                    easing.type: Easing.InCurve
                }
            }
        }

        imageListView {
            onReplaceImage: function(index,path) {
                pagViewer.onReplaceImage(index,path)
            }
        }
    }

    Lab11.FileDialog {
        id: openPagDialog

        visible: false
        title: qsTr("打开 PAG 文件")
        nameFilters: ["PAG files (*.pag)"]
        fileMode: Lab11.FileDialog.OpenFile

        onAccepted: {
            mainWindow.filePath = openPagDialog.file
            mainForm.pagViewer.setFile(filePath)
        }
    }

    Lab11.FileDialog {
        id: taskFileDialog

        visible: false
        title: qsTr("保存")
        fileMode: Lab11.FileDialog.SaveFile

        onAccepted: {
            let path = taskFileDialog.file.toString()
            if (mainWindow.taskType === FileTaskFactory.TaskType_APNG){
                path = path.replace(".png","")
            } else if(mainWindow.taskType === FileTaskFactory.TaskType_PNGs) {
                path = path.replace("_PNG.png","")
            }
            let task = mainForm.taskFactory.createTask(mainWindow.taskType, path)
            if (task) {
                taskConnections.target = task
                mainForm.popup.active = true
                mainForm.popup.item.task = task
                task.start()
            }
        }
    }

    SettingsWindow {
        id: settingsWindow

        width: 500
        height: 160
        visible: false
        useBeta: settings.isUseBeta
        useEnglish: settings.isUseEnglish

        onUseBetaChanged: {
            if (!settingsWindow.visible || (settingsWindow.useBeta === settings.isUseBeta)) {
                return
            }

            // TODO
            // let ok = checkUpdateModel.setIsBetaVersion(settingsWindow.useBeta)
            if (ok) {
                settings.isUseBeta = settingsWindow.useBeta
                if(settingsWindow.useBeta) {
                    checkUpdate(false)
                }
            } else {
                settingsWindow.useBeta = !settingsWindow.useBeta
                console.log("Set Use Beta Error")
            }
            console.log("Set settings.isUseBeta: ", settings.isUseBeta)
        }

        onAutoCheckUpdateChanged: {
            settings.isAutoCheckUpdate = settingsWindow.autoCheckUpdate
            console.log("Set settings.isAutoCheckUpdate: ", settingsWindow.autoCheckUpdate)
        }

        onShowVideoFramesChanged: {
            settings.isShowVideoFrames = settingsWindow.showVideoFrames
            mainForm.pagViewer.showVideoFrames = settingsWindow.showVideoFrames
            console.log("Set settings.isShowVideoFrames: ", settingsWindow.isShowVideoFrames)
        }

        onUseEnglishChanged: {
            if(!settingsWindow.visible || (settingsWindow.useEnglish === settings.isUseEnglish)) {
                return
            }
            settings.isUseEnglish = settingsWindow.useEnglish
            console.log("Set settings.isUseEnglish: ", settingsWindow.useEnglish)
        }
    }

    MessageBox {
        id: noUpdateMessage

        width: 500
        height: 130 + mainWindow.windowTitleBarHeight
        title: qsTr("检查更新")
        message: qsTr("PAGViewer 已经是最新版本")
        visible: false
    }

    MessageBox {
        id: confirmUpdateMessage

        width: 500
        height: 160 + mainWindow.windowTitleBarHeight
        title: qsTr("检查更新")
        message: qsTr("检查到 PAGViewer 有新版本，是否立刻更新？")
        visible: false
        showCancel: true

        onAccepted: {
            console.log("User confirm to update")
            // TODO
            // checkUpdateModel.startUpdatePlayer()
        }
        onCanceled: {
            console.log("canceled")
        }
    }

    AboutMessageBox {
        id: aboutWindow

        width: settings.isUseEnglish ? 600 : 500
        height: 180 + mainWindow.windowTitleBarHeight
        title: qsTr("关于 PAGViewer")
        message: "<b>PAGViewer</b> "+Qt.application.version+ "<br><br>Copyright © 2017-present Tencent. All rights reserved."
        textSize: 12
        visible: false
        messageLicense: qsTr("软件许可及服务协议")
        messagePrivacy: qsTr("隐私保护声明")
    }

    MessageBox {
        id: reportIssueWindow

        width: 500
        height: 130 + mainWindow.windowTitleBarHeight
        title: qsTr("问题反馈")
        message: qsTr("如果您在使用 PAGViewer 的过程中有任何问题，请在论坛提问")
        textSize: 12
        visible: false
    }

    MessageBox {
        id: attributeSaveErrorWindow

        width: 600
        height: 130 + mainWindow.windowTitleBarHeight
        title: qsTr("属性修改")
        message: ""
        textSize: 12
        visible: false
    }

    MessageBox {
        id: benchmarkCompleteWindow

        width: 500
        height: 130 + mainWindow.windowTitleBarHeight
        title: qsTr("性能基准测试")
        message: qsTr("性能基准测试已完成")
        textSize: 12
        visible: false
    }

    // TODO
    // CustomBusyIndicator{
    //     id:busyLoading
    //     visible: false
    //     running: false
    //     width:100
    //     height:100
    //     anchors.centerIn: parent
    // }

    /* Connection between task and process */
    Connections {
        id: taskConnections

        function onProgressChange(progress){
            console.log("Progress: " + mainForm.popup.item.popProgress.value + " -> " + progress)
            mainForm.popup.item.popProgress.value = progress
        }

        function onProgressVisibleChange(visible) {
            console.log("Visible change: " + visible)
            mainForm.popup.active = visible
        }

        function onTaskComplete(filePath, result) {
            if (result !== 0) {
                alert(warn1 + result)
            }
        }
    }

    Connections {
        target: benchmarkModel
    
        function onBenchmarkComplete(templateAvgRenderingTime, templateFirstFrameRenderingTime, uiAvgRenderingTime, uiFirstFrameRenderingTime, isAuto) {
            console.log(" templateAvgRenderingTime: ", templateAvgRenderingTime, " templateFirstFrameRenderingTime: ", templateFirstFrameRenderingTime)
            console.log(" uiAvgRenderingTime: ", uiAvgRenderingTime, " uiFirstFrameRenderingTime: ", uiFirstFrameRenderingTime)
            settings.templateAvgRenderingTime = templateAvgRenderingTime
            settings.templateFirstFrameRenderingTime = templateFirstFrameRenderingTime
            settings.uiAvgRenderingTime = uiAvgRenderingTime
            settings.uiFirstFrameRenderingTime = uiFirstFrameRenderingTime
    
            busyLoading.visible = false
            busyLoading.running = false
    
            console.log("------ isAuto:  ", isAuto)
    
            if(!isAuto){
                benchmarkCompleteWindow.visible = true
                benchmarkCompleteWindow.raise()
            }
    
            if(isAuto){
                settings.hadBenchmarkVersion = Qt.application.version
            }
        }
    }

    Connections {
        target: mainForm.popup.item

        onClosing: {
            mainForm.popup.item.task.stop()
        }
    }

    function getFilePath(str) {
        str = str.replace(/\\/ig,'/')
        return (str.slice(0, str.lastIndexOf("/") + 1))
    }

    function getFileName(str) {
        str = str.replace(/\\/ig,'/')
        return (str.slice(str.lastIndexOf("/") + 1, str.lastIndexOf(".")))
    }

    function getNativeFilePath(str) {
        str = str.replace(/\\/ig,'/')
        let schema = "file://"
        if (Qt.platform.os === "windows"){
            schema+="/"
        }
        return schema + str
    }

    function removeFileSchema(pathWithSchema) {
        const FILE_SCHEMA = 'file://'

        if (!pathWithSchema.startsWith(FILE_SCHEMA)) {
            return pathWithSchema
        }
        let result = pathWithSchema.slice(FILE_SCHEMA.length)
        if (Qt.platform.os === 'windows') {
            result = result.slice(1)
        }
        return result
    }

    function onCommand(command) {
        console.log("Get command: " + command)
        switch (command) {
            case 'open-preferences': {
                settingsWindow.visible = true
                settingsWindow.raise()
                break
            }
            case 'open-pag-file': {
                if (mainForm.pagViewer.filePath) {
                    let path = mainForm.pagViewer.filePath
                    openPagDialog.folder = getFilePath(path)
                } else {
                    openPagDialog.folder = StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
                }
                openPagDialog.open()
                break
            }
            case 'close-window': {
                mainWindow.close()
                break
            }
            case 'begin-profile': {
                let task = mainForm.taskFactory.createTask(FileTaskFactory.TaskType_CMS)
                if (task) {
                    taskConnections.target = task
                    mainForm.popup.active = true
                    console.log(mainForm.popup.item)
                    mainForm.popup.item.task = task
                    task.start()
                }
                break
            }
            case 'export-png-sequence': {
                mainWindow.taskType = FileTaskFactory.TaskType_PNGs
                let exportPath = mainForm.pagViewer.filePath
                taskFileDialog.folder =  getNativeFilePath(getFilePath(exportPath))
                taskFileDialog.currentFile = getNativeFilePath(exportPath.replace(".pag", "_PNG.png"))
                taskFileDialog.open()
                break
            }
            case 'export-apng': {
                mainWindow.taskType = FileTaskFactory.TaskType_APNG
                let exportPath = mainForm.pagViewer.filePath.replace(".pag", "_APNG.png")
                taskFileDialog.folder =  getNativeFilePath(getFilePath(exportPath))
                taskFileDialog.currentFile = getNativeFilePath(exportPath)
                taskFileDialog.open()
                break
            }
            case 'export-frame-as-png': {
                mainWindow.taskType = FileTaskFactory.TaskType_Frame
                let exportPath = mainForm.pagViewer.filePath.replace(".pag", "_" + (currentFrame + 1) + ".png")
                taskFileDialog.folder =  getNativeFilePath(getFilePath(exportPath))
                taskFileDialog.currentFile = getNativeFilePath(exportPath)
                taskFileDialog.open()
                break
            }
            case 'first-frame': {
                mainForm.pagViewer.firstFrame()
                break
            }
            case 'last-frame': {
                mainForm.pagViewer.lastFrame()
                break
            }
            case 'previous-frame': {
                mainForm.pagViewer.previousFrame()
                break
            }
            case 'next-frame': {
                mainForm.pagViewer.nextFrame()
                break
            }
            case 'pause-or-play': {
                mainForm.pagViewer.isPlaying = !mainForm.pagViewer.isPlaying
                break
            }
            case 'toggle-background': {
                toggleBackground()
                break
            }
            case 'toggle-edit-panel': {
                toggleEditPanel()
                break
            }
            case 'minimize-window': {
                mainWindow.showMinimized()
                break
            }
            case 'zoom-window': {
                mainWindow.visibility = (mainWindow.visibility !== qWindowMaximize) ? qWindowMaximize : qWindowAutomatic
                break
            }
            case 'fullscreen-window': {
                mainWindow.visibility = (mainWindow.visibility !== qWindowMaximize) ? qWindowMaximize : qWindowAutomatic
                break
            }
            case 'show-help': {
                Qt.openUrlExternally("https://pag.art/#pag-player")
                break
            }
            case 'check-update': {
                checkUpdate(false)
                break
            }
            case 'performance-benchmark': {
                mainForm.pagViewer.isPlaying = false
                // TODO
                // busyLoading.visible = true
                // busyLoading.running = true
                benchmarkModel.startBenchmarkFromQRC(false)
                break
            }
            case 'install-ae-plugin': {
                // let ret = checkUpdateModel.installAEPlugin(true)
                break
            }
            case 'unInstall-ae-plugin': {
                // let ret = checkUpdateModel.uninstallAEPlugin()
                break
            }
            case 'open-about': {
                aboutWindow.visible = true
                aboutWindow.raise()
                break
            }
            case 'show-report-issue': {
                Qt.openUrlExternally("https://github.com/Tencent/libpag/discussions")
                break
            }
            case 'open-commerce-page': {
                Qt.openUrlExternally("https://pag.io/product.html#pag-enterprise-edition")
                break
            }
            case 'reload-project': {
                windowManager.openProject('')
                break
            }
            default: {
                console.log("Unknown command: " + command)
            }
        }
    }

    function toggleEditPanel(willOpen) {
        if(willOpen === undefined) {
            willOpen = !mainForm.isEditPanelOpen
        }

        if(mainForm.controlForm.btnPanels.checked !== willOpen) {
            mainForm.controlForm.btnPanels.checked = willOpen
        }
        let widthChange = (mainForm.rightItem.width) === 0 ? mainForm.panelDefaultWidth: mainForm.rightItem.width
        widthChange += mainForm.splitHandleWidth
        widthChange *= willOpen ? 1: -1
        panelShow.running = willOpen
        panelHide.running = !willOpen
        let windWidthChange = widthChange
        let playerWidthChange = 0
        if (mainWindow.isMaximized) {
            windWidthChange = 0
            playerWidthChange = widthChange
        }
        mainForm.playerMinWidth = mainForm.centerItem.width - playerWidthChange
        let width = mainWindow.width + windWidthChange
        if (width < mainWindow.minimumWidth) {
            width = mainWindow.minimumWidth
        }
        mainWindow.width = width
        if(willOpen && (mainWindow.height < mainWindow.windowHeightEditPanelOpen)) {
            let heightChange = mainWindow.windowHeightEditPanelOpen - mainWindow.height
            mainWindow.height = mainWindow.windowHeightEditPanelOpen
            mainWindow.y -= (heightChange / 2)
            mainWindow.y = Math.max(80, mainWindow.y)
        }
        settings.isEditPanelOpen = willOpen
    }

    function toggleBackground(turnOn) {
        if(turnOn === undefined) {
            turnOn = !mainForm.isBackgroundOn
        }
        mainForm.isBackgroundOn = turnOn
    }

    function checkUpdate(silent) {
        let now = new Date().getTime()
        checkUpdateSilent = silent
        settings.lastCheckUpdateTime = now
        console.log("checking update")

        let http = new XMLHttpRequest()
        let url = "https://pag.qq.com"
        http.open("Get", url, true)
        http.setRequestHeader("Connection", "close")

        http.onreadystatechange = function() {
            if (http.readyState === 4) {
                if (http.status === 200) {
                    /* feedUrl should be http://dldir1.qq.com/qqmi/libpag/ */
                    let feedUrl = http.response
                    if (settings.isUseBeta) {
                        feedUrl += "beta/"
                    }
                    feedUrl += ((Qt.platform.os === "osx") ? "PagAppcast.xml" : "PagAppcastWindows.xml")
                    console.log("feedUrl1: ", feedUrl)
                    feedUrl = feedUrl.replace("http://", "https://")
                    feedUrl = feedUrl.substring(feedUrl.indexOf("https://"))
                    console.log("feedUrl2: ", feedUrl)
                    checkUpdateModel.checkPlayerUpdate(!silent, feedUrl)
                } else {
                    console.log("error: " + http.status)
                }
            }
        }
        http.send()
    }

    function onGetCheckUpdateResult(hasUpdate) {
        console.log("hasUpdate:  " + hasUpdate)
        if (hasUpdate === true) {
            confirmUpdateMessage.visible = true
            confirmUpdateMessage.raise()
        } else if(!checkUpdateSilent) {
            noUpdateMessage.visible = true
            noUpdateMessage.raise()
        }
    }

    function updateProgressText() {
        let duration = mainForm.pagViewer.duration
        let current = mainForm.pagViewer.progress * duration
        mainForm.controlForm.txtProgress.text = msToTime(current)
        mainForm.controlForm.txtDurationInSecond.text = msToTime(duration)
        mainForm.controlForm.txtCurrentFrame.text = mainForm.pagViewer.currentFrame
        mainForm.controlForm.txtTotalFrame.text = mainForm.pagViewer.totalFrame
    }

    function msToTime(duration) {
        let seconds = parseInt((duration / 1000) % 60)
        let minutes = parseInt((duration / (1000 * 60)) % 60)

        minutes = (minutes < 10) ? "0" + minutes : minutes
        seconds = (seconds < 10) ? "0" + seconds : seconds

        return minutes + ":" + seconds
    }

    function resizePAGViewer(){
        let pH = mainForm.pagViewer.pagHeight
        let pW = mainForm.pagViewer.pagWidth
        let wH = mainForm.centerItem.height - mainForm.controlForm.height
        let wW = mainForm.centerItem.width
        let finalWidth = 1
        let finalHeight = 1
        let pagIsNarrower = (pH / pW) > (wH / wW)
        if (pagIsNarrower) {
            finalHeight = wH
            finalWidth = finalHeight / pH * pW
        } else {
            finalWidth = wW
            finalHeight = finalWidth / pW * pH
        }
        mainForm.pagViewer.width = finalWidth
        mainForm.pagViewer.height = finalHeight
        mainForm.pagViewer.x = (wW - finalWidth) / 2
        mainForm.pagViewer.y = (wH - finalHeight) / 2
    }

    function getCurrentFilePath() {
        const currentPath = (Qt.platform.os === 'windows') ? mainForm.pagViewer.filePath.replace(/\\/ig,'/') : mainForm.pagViewer.filePath
        return removeFileSchema(currentPath)
    }

    function getCurrentFileName() {
        const path = mainForm.pagViewer.filePath
        if (!path) {
            return ""
        }
        return path.replace(/\\/ig,'/').match(/\/([^\/]+)$/)[1]
    }

    function checkVariables() {

    }

    Component.onCompleted: {
        if (settings.isFirstRun) {
            console.log("First Run")
            settings.isFirstRun = false
            settings.isUseBeta = Qt.application.version.indexOf("beta") > 0
            settings.isUseEnglish = languageModel.getSystemLanguage()
            settingsWindow.useBeta = settings.isUseBeta
            settingsWindow.useEnglish = settings.isUseEnglish
        }

        languageModel.setLanguage(settings.isUseEnglish)

        resizePAGViewer()

        // todo: merge menu to single file
        let isMacOS = Qt.platform.os === "osx"
        let menu = Qt.createComponent("Menu.qml")
        let bar = menu.createObject(mainWindow)
        bar.onCommand.connect(mainWindow.onCommand)
        mainWindow.visibilityChanged.connect(function(){
            bar.isFullscreen = (mainWindow.visibility === qWindowFullscreen)
        })
        if (isMacOS) {
            bar.windowActive = Qt.binding(function() {
                return mainWindow.active
            })
        }

        mainForm.pagViewer.onFileChanged.connect(function(v) {
            bar.hasPAGFile = true
        })
        mainForm.pagViewer.onFrameMetricsReady.connect(function(frame, m1, m2, m3) {
            currentFrame = frame
        })
        // TODO
        // mainForm.pagViewer.onFileNeedPassword.connect(function() {
        //     decryptMessageBox.visible = true
        // })
        // mainForm.pagViewer.onFileNeedLicense.connect(function(name, info) {
        //     certificateDecryptMessageBox.show(name, info)
        // })

        // TODO
        // pagTreeViewModel.onModelReset.connect(function() {
        //     mainForm.pagFileTreeView.expand(pagTreeViewModel.index(0,0))
        // })
        // checkUpdateModel.onCheckUpdateResult.connect(onGetCheckUpdateResult)
        // checkUpdateModel.updatePreviousVersion()

        timer.start()
        updateTimer.start()
    }
}

