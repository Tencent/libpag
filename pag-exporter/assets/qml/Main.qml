import QtQuick 2.5
import QtQuick.Window 2.13
import Qt.labs.platform 1.0
import PAG 1.0
import Qt.labs.settings 1.0
import Qt.labs.platform 1.1 as Lab11

PApplicationWindow {
    id: wind
    visible: true
    width: 342
    height: 567
    hasMenu: true
    minimumHeight: 567 + windowTitleBarHeight
    minimumWidth: 342 + windowPadding
    canResize: true
    isWindows: true
    titleFontSize: 14
    maxBtnVisible: false
    minBtnVisible: false
    windowBackground: Qt.rgba(20/255,20/255,30/255,1)
    titlebarBackground: windowBackground

    Settings {
        id: settings
        property bool isShowVideoFrames: true
        property double lastX:0
        property double lastY:0
        property int templateAvgRenderingTime:30000
        property int templateFirstFrameRenderingTime:60000
        property int uiAvgRenderingTime:30000
        property int uiFirstFrameRenderingTime:60000
    }

    property int windowHeightEditPanelOpen: 650;
    property int windowResizeDuration:200;
    property int qWindowFullscreen:5;
    property int qWindowAutomatic:1;
    property int qWindowMaximize:4;
    property int windowTitleBarHeight: Qt.platform.os=='windows'?32:0;
	property int windowPadding:Qt.platform.os=='windows'?2:0;
    property int currentFrame:0;
    property string filePath;

    // Behavior on height { NumberAnimation { duration: windowResizeDuration; easing.type: Easing.InCurve } }
    // Behavior on width  { NumberAnimation { duration: windowResizeDuration; easing.type: Easing.InCurve } }
    // Behavior on x { NumberAnimation { duration: windowResizeDuration; easing.type: Easing.InCurve } }
    // Behavior on y { NumberAnimation { duration: windowResizeDuration; easing.type: Easing.InCurve } }




    MainForm {
        id: form
        objectName: "main"
        controlForm {
            btnBackground {
                checked: form.isBackgroundOn
                onCheckedChanged: {
                    toggleBackground(form.controlForm.btnBackground.checked);
                }
            }
            btnNext.onClicked: pagViewer.nextFrame()
            btnPrevious.onClicked: pagViewer.previousFrame()
            btnPanels.onClicked: {
                toggleEditPanel(form.controlForm.btnPanels.checked);
            }

            sliderProgress {
                //value: form.controlForm.progress
                onValueChanged: {
                    if(form.controlForm.progress == form.controlForm.sliderProgress.value)
                        return;
                    form.controlForm.progress = form.controlForm.sliderProgress.value;
                    updateProgressText();
                }
                onPressedChanged : {
                    var pressed = form.controlForm.sliderProgress.pressed;
                    if(pressed) {
                        form.controlForm.lastPlayStatus = form.pagViewer.isPlaying;
                        form.pagViewer.isPlaying = false;
                    }
                    else {
                        form.pagViewer.isPlaying = form.controlForm.lastPlayStatus;
                        form.controlForm.lastPlayStatus = false;
                    }
                }
            }

            onIsPlayingChanged: {
                if(controlForm.isPlaying == pagViewer.isPlaying) return;
                pagViewer.isPlaying = controlForm.isPlaying;
            }

            onProgressChanged: {
                if(form.controlForm.sliderProgress.value != form.controlForm.progress){
                    form.controlForm.sliderProgress.value = form.controlForm.progress;
                }
                
                updateProgressText();
            } 

        }

        pagViewer {
            showVideoFrames: settings.isShowVideoFrames
            onIsPlayingChanged: function(v) {
                if(controlForm.isPlaying==pagViewer.isPlaying) return;
                controlForm.isPlaying = pagViewer.isPlaying;
            }

            onProgressChanged: progress => {
                if(form.controlForm.progress === progress)
                    return;
                form.controlForm.progress = progress
            }

            onFileChanged: filePath => {

                var path = form.pagViewer.filePath;
                //    Get file name
                //       c:\path\to\file.ext
                //    -> c:/path/to/file.ext
                //    -> file.ext
                console.log("file changed filePath:",filePath)
                path = path.replace(/\\/ig,'/').match(/\/([^\/]+)$/)[1];
//                wind.title = path;

                form.hasPAGFile = true;
                form.controlForm.hasPAGFile = true;
                form.centerItem.color = form.pagViewer.backgroundColor;
                updateProgressText();

                var px = wind.x, py = wind.y, sw = wind.width, sh = wind.height;
                var preferredSize = pagViewer.preferredSize;

                console.log("preferredSize:",preferredSize.width,preferredSize.height);

                var height = controlForm.height + windowTitleBarHeight + preferredSize.height;
                wind.height = height;
                var y = Math.max(50,py-(height - sh)/2);
                if(settings.lastY==y){
                    y+=25;
                }
                settings.lastY = y;
                wind.y = y;

                var minWidth = wind.minimumWidth;
                var width = Math.max(minWidth, preferredSize.width );
                // if(form.isEditPanelOpen){
                //     var rightPanelWidth = rightItem.width == 0 ? form.panelDefaultWidth:rightItem.width;
                //     width += rightPanelWidth + form.splitHandleWidth;
                // }
                console.log("rightErrorPanel visible:",form.rightErrorPanel.visible);
                if(form.rightErrorPanel.visible) {
                    console.log("add rightErrorPanel width:",form.rightErrorPanel.width);
                    width += form.rightErrorPanel.width;
                }
                wind.width = width + windowPadding;
                console.log("window width:",width);
                console.log("window size:",wind.width,wind.height);

                var x = Math.max(0, px - ( width - sw )/2);
                if(settings.lastX==x){
                    x+=25;
                }
                settings.lastX = x;
                wind.x = x;
            }

        }

        centerItem {
            onWidthChanged: resizePAGViewer()
            onHeightChanged: resizePAGViewer()
        }

        rightItem {
            width:0
        }
    }

    Lab11.FileDialog {
        id: openPagDialog
        visible: false
        title: qsTr("打开 PAG 文件")
        nameFilters: [ "PAG files (*.pag)"]
        fileMode: Lab11.FileDialog.OpenFile
        onAccepted: {
            filePath = openPagDialog.file;
            form.pagViewer.setFile(filePath);
        }
    }

    property var taskType;
	property var exportLocation;
    Lab11.FileDialog {
        id: taskFileDialog
        visible: false
        title: qsTr("保存")
        fileMode: Lab11.FileDialog.SaveFile
        onAccepted: {
            var path = taskFileDialog.file.toString();
            if(taskType == FileTaskFactory.TaskType_APNG){
                path = path.replace(".png","");
            }
            else if(taskType == FileTaskFactory.TaskType_PNGs) {
                path = path.replace("_PNG.png","");
            }
            var task = form.taskFactory.createTask(taskType, path);
            if (task) {
                taskConnections.target = task;
                form.popup.active = true;
                form.popup.item.task = task;
                task.start();
            }
        }
    }

    function getFilePath(str) {
        str = str.replace(/\\/ig,'/');
        return (str.slice(0,str.lastIndexOf("/") + 1))
    }

    function getFileName(str) {
        str = str.replace(/\\/ig,'/');
        return (str.slice(str.lastIndexOf("/")+1, str.lastIndexOf(".")))
    }

	function getNativeFilePath(str) {
        str = str.replace(/\\/ig,'/');
		var schema = "file://";
		if(Qt.platform.os=="windows"){
			schema+="/";
		}
		return schema + str;
	}

    function onCommand(command) {
        var isMacOS = Qt.platform.os==="osx";
        //文件
        if(command === 'open-preferences'){
            settingsWindow.visible = true;
            settingsWindow.raise();
        }
        else if(command === 'open-pag-file'){
            if(form.pagViewer.filePath) {
                var path = form.pagViewer.filePath;
                openPagDialog.folder = getFilePath(path);
            }
            else {
                openPagDialog.folder = StandardPaths.writableLocation(StandardPaths.DocumentsLocation);
            }
            openPagDialog.open();
        }
        else if(command === "close-window") {
            wind.close();
        }
        else if(command === "begin-profile") {
            var task = form.taskFactory.createTask(FileTaskFactory.TaskType_CMS);
            if (task) {
                taskConnections.target = task;
                form.popup.active = true;
                console.log(form.popup.item);
                form.popup.item.task = task;
                task.start();
            }
        }
        else if(command === "export-png-sequence") {
            taskType = FileTaskFactory.TaskType_PNGs;
            var exportPath = form.pagViewer.filePath;
            taskFileDialog.folder =  getNativeFilePath(getFilePath(exportPath));
            taskFileDialog.currentFile = getNativeFilePath(exportPath.replace(".pag","_PNG.png"));
            taskFileDialog.open();
        }
        else if(command === "export-apng") {
            taskType = FileTaskFactory.TaskType_APNG;
            var exportPath = form.pagViewer.filePath.replace(".pag","_APNG.png");
            taskFileDialog.folder =  getNativeFilePath(getFilePath(exportPath));
            taskFileDialog.currentFile = getNativeFilePath(exportPath);
            taskFileDialog.open();
        }
        else if(command === "export-frame-as-png") {
            taskType = FileTaskFactory.TaskType_Frame;
            var exportPath = form.pagViewer.filePath.replace(".pag","_"+(currentFrame+1)+".png");
            taskFileDialog.folder =  getNativeFilePath(getFilePath(exportPath));
            taskFileDialog.currentFile = getNativeFilePath(exportPath);
            taskFileDialog.open();
        }
        //播放
        else if(command === "first-frame") {
            form.pagViewer.firstFrame();
        }
        else if(command === "last-frame") {
            form.pagViewer.lastFrame();
        }
        else if(command === "previous-frame") {
            form.pagViewer.previousFrame();
        }
        else if(command === "next-frame") {
            form.pagViewer.nextFrame();
        }
        else if(command === "pause-or-play") {
            form.pagViewer.isPlaying = !form.pagViewer.isPlaying;
        }

        //视图
        else if(command === "toggle-background") {
            toggleBackground();
        }
        else if(command === "toggle-edit-panel") {
            toggleEditPanel();
        }

        //窗口
        else if(command === "minimize-window") {
            wind.showMinimized();
        }
        else if(command === "zoom-window") {
            wind.visibility = wind.visibility != qWindowMaximize ? qWindowMaximize : qWindowAutomatic;
        }
        else if(command === "fullscreen-window") {
            wind.visibility = wind.visibility != qWindowMaximize ?qWindowMaximize : qWindowAutomatic;
        }
        //帮助
        else if(command === "show-help") {
            Qt.openUrlExternally("https://pag.art/#pag-player");
        }
    }

    function toggleEditPanel(willOpen) {
        if(willOpen === undefined)
            willOpen = !form.isEditPanelOpen;
        if(form.controlForm.btnPanels.checked !== willOpen) {
            form.controlForm.btnPanels.checked = willOpen;
        }
        var widthChange = form.rightItem.width === 0 ? form.panelDefaultWidth: form.rightItem.width;
        widthChange += form.splitHandleWidth;
        widthChange *= willOpen ? 1: -1;
        panelShow.running = willOpen;
        panelHide.running = !willOpen;
        var windWidthChange = widthChange;
        var playerWidthChange = 0;
        if(wind.isMaximized){
            windWidthChange = 0;
            playerWidthChange = widthChange;
        }
        form.playerMinWidth = form.centerItem.width - playerWidthChange;
        var width=wind.width+windWidthChange;
        if(width<wind.minimumWidth){
            width = wind.minimumWidth;
        }
        wind.width = width;
        if(willOpen && wind.height < windowHeightEditPanelOpen) {
            var heightChange = windowHeightEditPanelOpen - wind.height;
            wind.height = windowHeightEditPanelOpen;
            wind.y -= heightChange / 2;
            wind.y = Math.max(80,wind.y);
        }
        settings.isEditPanelOpen = willOpen;
    }

    function toggleBackground(turnOn) {
        if(turnOn === undefined) {
            turnOn = !form.isBackgroundOn;
        }
        form.isBackgroundOn = turnOn;
    }

    function updateProgressText() {
        var duration = form.pagViewer.duration;
        var current = form.pagViewer.progress * duration;
        form.controlForm.txtProgress.text = msToTime(current);
        form.controlForm.txtDurationInSecond.text = msToTime(duration);
    }


    function msToTime(duration) {
        var milliseconds = parseInt((duration%1000)/100)
            , seconds = Math.round((duration/1000)%60)
            , minutes = parseInt((duration/(1000*60))%60)
            , hours = parseInt((duration/(1000*60*60))%24);

        minutes = (minutes < 10) ? "0" + minutes : minutes;
        seconds = (seconds < 10) ? "0" + seconds : seconds;

        return minutes + ":" + seconds;
    }

    function resizePAGViewer(){
        var pH = form.pagViewer.pagHeight,
            pW = form.pagViewer.pagWidth,
            wH = form.centerItem.height - form.controlForm.height,
            wW = form.centerItem.width
        var finalHeight = 1,finalWidth = 1;
        var pagIsNarrower = pH/pW > wH/wW;
        if(pagIsNarrower) {
            finalHeight = wH;
            finalWidth = finalHeight / pH * pW;
        }
        else {
            finalWidth = wW;
            finalHeight = finalWidth / pW * pH;
        }
        form.pagViewer.width = finalWidth
        form.pagViewer.height = finalHeight
        form.pagViewer.x = (wW-finalWidth)/2;
        form.pagViewer.y = (wH-finalHeight)/2;
    }

    Component.onCompleted: {

        if(settings.isFirstRun) {
            console.log("First Run");
            settings.isFirstRun = false;
            settings.isUseBeta = Qt.application.version.indexOf("beta")>0;
            settingsWindow.useBeta = settings.isUseBeta;
        }


        resizePAGViewer()

        form.pagViewer.onFrameMetricsReady.connect(function(frame,m1,m2,m3){ currentFrame = frame; });

        timer.start();
    }

}
