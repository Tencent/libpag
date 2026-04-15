import PAG
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: element

    required property var contentView

    property int defaultWidth: 300
    property int defaultHeight: 300
    property int sectionSpacing: 2

    property int contentHeight: mainColumn.height + 24

    property alias graph: graph
    property alias graphCanvas: graphCanvas
    property alias currentFrameText: currentFrameText

    property var performanceWarn1: qsTr("Rendering time too long, suggest optimizing time cost to under %1 us")
    property var performanceWarn2: qsTr("Memory usage is too large, suggest optimizing memory cost to under 50M")
    property var performanceWarn3: qsTr("(Run performance benchmark, results will be more accurate)")
    property var performanceWarn4: qsTr("Using too many video sequences can slow performance")

    property var performanceTip1: qsTr("Too many layers, Suggest combining same layers into composition")
    property var performanceTip2: qsTr("Sticker scale is too large, suggest reducing sticker scale")
    property var performanceTip3: qsTr("Limit video sequences to two or fewer")

    width: defaultWidth
    height: defaultHeight
    implicitHeight: contentHeight

    Connections {
        target: runTimeDataModel
        enabled: runTimeDataModel !== null

        function onDataChanged() {
            currentFrameText.text = runTimeDataModel.currentFrame + "/" + runTimeDataModel.totalFrame;
        }
    }

    Connections {
        target: runTimeDataModel ? runTimeDataModel.chartDataModel : null
        enabled: runTimeDataModel !== null && runTimeDataModel.chartDataModel !== null

        function onItemsChanged() {
            graphCanvas.requestPaint();
        }
    }

    Connections {
        target: runTimeDataModel ? runTimeDataModel.frameDisplayInfoModel : null
        enabled: runTimeDataModel !== null && runTimeDataModel.frameDisplayInfoModel !== null

        function onItemsChanged() {
            if (runTimeDataModel && !runTimeDataModel.hasFrameTimeline) {
                pagxStatsColumn.totalTime = statistics.calculateTotalTime();
            }
        }
    }

    /* Main Column Container */
    Column {
        id: mainColumn
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12

        /* Title */
        Rectangle {
            id: profilerTitle
            color: "#2D2D37"
            height: 25
            width: parent.width

            RowLayout {
                anchors.fill: parent
                spacing: 0

                Item {
                    width: 10
                    height: 1
                }

                Label {
                    id: titleLabel
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("File Info")
                    color: "#FFFFFF"
                    font.pixelSize: 10
                    visible: true
                }

                Item {
                    Layout.fillWidth: true
                    height: 1
                }
            }
        }

        /* Spacer between Title and FileInfo */
        Item {
            width: 1
            height: 1
        }

        /* File Info */
        GridView {
            id: fileInfoView
            height: 93
            width: parent.width
            cellWidth: runTimeDataModel && runTimeDataModel.nodeStatsModel.totalCount > 0 ? width / 3 : width / 4
            cellHeight: 46
            boundsBehavior: Flickable.StopAtBounds
            keyNavigationWraps: false

            model: runTimeDataModel ? runTimeDataModel.fileInfoModel : null

            delegate: Item {
                width: fileInfoView.cellWidth
                height: fileInfoView.cellHeight

                Rectangle {
                    property int columnsPerRow: runTimeDataModel && runTimeDataModel.nodeStatsModel.totalCount > 0 ? 3 : 4
                    height: fileInfoView.cellHeight - 1
                    width: fileInfoView.cellWidth - ((index + 1) % columnsPerRow === 0 ? 0 : 1)
                    color: "#2D2D37"
                    anchors {
                        left: parent.left
                        top: parent.top
                        rightMargin: 1
                        bottomMargin: 1
                    }
                    Text {
                        id: nameText
                        color: "#9b9b9b"
                        text: name
                        font.pixelSize: 9
                        renderType: Text.NativeRendering
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                    }

                    Text {
                        id: valueText
                        color: "#ffffff"
                        text: value
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 14
                        renderType: Text.NativeRendering
                        anchors.top: nameText.bottom
                        anchors.topMargin: 2
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 2
                        anchors.left: nameText.left
                        anchors.leftMargin: 0
                    }

                    Text {
                        id: extText
                        color: "#9b9b9b"
                        text: unit
                        font.pixelSize: 9
                        renderType: Text.NativeRendering
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 5
                        anchors.left: valueText.right
                        anchors.leftMargin: 3
                    }

                    PerformanceWarnDialog {
                        id: performanceWarnDialog
                    }

                    Image {
                        id: performanceWarnImage
                        width: 11
                        height: 10
                        visible: {
                            if (name === "Graphics" && Number(value) > 50 && unit === "MB") {
                                performanceWarnDialog.clearAllTips();
                                performanceWarnDialog.warnMessage = performanceWarn2;
                                performanceWarnDialog.addTip(performanceTip1);
                                performanceWarnDialog.addTip(performanceTip2);
                                return true;
                            }
                            if (name === "Videos" && Number(value) > 2) {
                                performanceWarnDialog.clearAllTips();
                                performanceWarnDialog.warnMessage = performanceWarn4;
                                performanceWarnDialog.addTip(performanceTip3);
                                return true;
                            }
                            return false;
                        }
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        source: "qrc:/images/performance-warn.png"
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                performanceWarnDialog.x = performanceWarnImage.x + performanceWarnImage.width - (performanceWarnDialog.width / 2);
                                performanceWarnDialog.y = performanceWarnImage.y + performanceWarnImage.height + 2;
                                performanceWarnDialog.setToTop();
                                performanceWarnDialog.open();
                            }
                        }
                    }
                }
            }
        }

        /* Spacer before Performance Chart (only visible in PAG mode) */
        Item {
            width: 1
            height: sectionSpacing
            visible: performanceChart.visible
        }

        /* Performance Chart (PAG mode only) */
        Rectangle {
            id: performanceChart
            height: 90
            width: parent.width
            visible: runTimeDataModel && runTimeDataModel.hasFrameTimeline
            color: "#00000000"

            Rectangle {
                id: graph
                height: 90
                color: "#2D2D37"
                clip: true
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right

                Canvas {
                    id: graphCanvas

                    property var chartDataModel: runTimeDataModel ? runTimeDataModel.chartDataModel : null

                    property var warnColor: [224 / 255.0, 15 / 255.0, 66 / 255.0]
                    property var renderColor: [0 / 255.0, 150 / 255.0, 216 / 255.0]
                    property var imageColor: [116 / 255.0, 173 / 255.0, 89 / 255.0]
                    property var presentColor: [221 / 255.0, 178 / 255.0, 89 / 255.0]
                    property int rectWidth: 3
                    property int spaceX: 1
                    property int posX: 0
                    property int posY: 0

                    height: 70
                    width: graph.width
                    anchors.bottom: parent.bottom
                    renderStrategy: Canvas.Cooperative

                    function getRectHeight(currentValue, maxValue, maxHeight) {
                        if (currentValue <= 0) {
                            return 1;
                        }
                        let height = (currentValue / maxValue) * maxHeight * 0.85 + 1;
                        return height;
                    }

                    function drawRect(context, rect, color) {
                        context.save();
                        context.fillStyle = color;
                        context.fillRect(rect.x, rect.y, rect.width, rect.height);
                        context.restore();
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (!runTimeDataModel || !runTimeDataModel.chartDataModel) {
                                return;
                            }
                            let index = parseInt((mouseX - graphCanvas.posX) / (graphCanvas.rectWidth + graphCanvas.spaceX));
                            let item = runTimeDataModel.chartDataModel.items[index];
                            if (index >= (runTimeDataModel.chartDataModel.currentIndex + 1) && index <= (runTimeDataModel.chartDataModel.currentIndex + 2)) {
                                return;
                            }
                            if (!item) {
                                return;
                            }
                            let maxTime = Number(runTimeDataModel.chartDataModel.maxTime);
                            let renderTime = Number(item.renderTime);
                            let presentTime = Number(item.presentTime);
                            let imageDecodeTime = Number(item.imageDecodeTime);
                            let renderRectHeight = graphCanvas.getRectHeight(renderTime, maxTime, graphCanvas.height);
                            let presentRectHeight = graphCanvas.getRectHeight(presentTime, maxTime, graphCanvas.height);
                            let imageDecodeRectHeight = graphCanvas.getRectHeight(imageDecodeTime, maxTime, graphCanvas.height);
                            let rectHeight = renderRectHeight + presentRectHeight + imageDecodeRectHeight;

                            if ((mouseY - graphCanvas.posY) < (graphCanvas.height - rectHeight)) {
                                return;
                            }

                            let warnMessage = performanceWarn1;
                            let benchmarkTime = Math.max(Number(settings.templateAvgRenderingTime), 30000);
                            if (benchmarkTime === 30000) {
                                warnMessage += performanceWarn3;
                            }
                            benchmarkTime = Math.ceil(benchmarkTime / 100) * 100;
                            warnMessage = warnMessage.arg(benchmarkTime);
                            let totalTime = renderTime + imageDecodeTime + presentTime;

                            if (totalTime > benchmarkTime) {
                                if (mouseX < (element.width - performanceWarnDialog.width)) {
                                    performanceWarnDialog.x = mouseX;
                                    performanceWarnDialog.y = mouseY - performanceWarnDialog.height / 2 + 20;
                                    performanceWarnDialog.setToLeft();
                                } else {
                                    performanceWarnDialog.x = mouseX - performanceWarnDialog.width;
                                    performanceWarnDialog.y = mouseY - performanceWarnDialog.height / 2 + 20;
                                    performanceWarnDialog.setToRight();
                                }
                                performanceWarnDialog.clearAllTips();
                                performanceWarnDialog.warnMessage = warnMessage;
                                performanceWarnDialog.addTip(performanceTip1);
                                performanceWarnDialog.addTip(performanceTip2);
                                performanceWarnDialog.open();
                            }
                        }
                    }

                    onPaint: {
                        if (!chartDataModel) {
                            return;
                        }
                        let context = graphCanvas.getContext("2d");
                        let posX = graphCanvas.posX;
                        let posY = graphCanvas.posY;
                        context.clearRect(posX, posY, graphCanvas.width, graphCanvas.height);
                        let alpha = 1;

                        for (let i = 0; i < chartDataModel.size; i++) {
                            if (i <= chartDataModel.currentIndex) {
                                alpha = 1;
                            } else if (i <= chartDataModel.currentIndex + 2) {
                                posX += (spaceX + rectWidth);
                                continue;
                            } else {
                                alpha = 0.43;
                            }

                            let item = chartDataModel.items[i];
                            let benchmarkTime = Math.max(Number(settings.templateAvgRenderingTime), 30000);
                            let benchmarkFirstFrameTime = Math.max(Number(settings.templateFirstFrameRenderingTime), 60000);
                            let renderColor_ = Qt.rgba(renderColor[0], renderColor[1], renderColor[2], alpha);
                            let imageColor_ = Qt.rgba(imageColor[0], imageColor[1], imageColor[2], alpha);
                            let presentColor_ = Qt.rgba(presentColor[0], presentColor[1], presentColor[2], alpha);
                            let renderTime = Number(item.renderTime);
                            let imageDecodeTime = Number(item.imageDecodeTime);
                            let presentTime = Number(item.presentTime);
                            let totalTime = renderTime + imageDecodeTime + presentTime;

                            if ((posX === 0 && totalTime > benchmarkFirstFrameTime) || (posX !== 0 && totalTime > benchmarkTime)) {
                                renderColor_ = Qt.rgba(warnColor[0], warnColor[1], warnColor[2], alpha);
                                imageColor_ = renderColor_;
                                presentColor_ = renderColor_;
                            }

                            let maxTime = Number(chartDataModel.maxTime);
                            let rectHeight = getRectHeight(presentTime, maxTime, graphCanvas.height);
                            let rect = {
                                x: posX,
                                y: graphCanvas.height - posY - rectHeight,
                                width: rectWidth,
                                height: rectHeight
                            };
                            drawRect(context, rect, presentColor_);
                            posY += rectHeight;

                            rectHeight = getRectHeight(imageDecodeTime, maxTime, graphCanvas.height);
                            rect = {
                                x: posX,
                                y: graphCanvas.height - posY - rectHeight,
                                width: rectWidth,
                                height: rectHeight
                            };
                            drawRect(context, rect, imageColor_);
                            posY += rectHeight;

                            rectHeight = getRectHeight(renderTime, maxTime, graphCanvas.height);
                            rect = {
                                x: posX,
                                y: graphCanvas.height - posY - rectHeight,
                                width: rectWidth,
                                height: rectHeight
                            };
                            drawRect(context, rect, renderColor_);
                            posY = 0;

                            posX += (spaceX + rectWidth);
                        }
                    }
                }

                PerformanceWarnDialog {
                    id: performanceWarnDialog
                }

                Text {
                    id: currentFrameText
                    text: "0/0"
                    font.pixelSize: 9
                    anchors.top: parent.top
                    anchors.topMargin: 5
                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    color: "#9B9B9B"
                    renderType: Text.NativeRendering
                }
            }
        }

        /* Spacer before Statistics */
        Item {
            width: 1
            height: sectionSpacing
            visible: statistics.visible
        }

        /* Data Statistics */
        Rectangle {
            id: statistics
            height: runTimeDataModel && !runTimeDataModel.hasFrameTimeline ? pagxStatsColumn.height : 62
            width: parent.width
            color: "#00000000"

            function calculateTotalTime() {
                if (!runTimeDataModel || !runTimeDataModel.frameDisplayInfoModel)
                    return 0;
                var items = runTimeDataModel.frameDisplayInfoModel.items;
                if (!items)
                    return 0;
                var total = 0;
                for (var i = 0; i < items.length; i++) {
                    total += Number(items[i].current);
                }
                return total;
            }

            Column {
                id: pagxStatsColumn
                visible: runTimeDataModel && !runTimeDataModel.hasFrameTimeline
                width: parent.width
                spacing: 1

                property real totalTime: statistics.calculateTotalTime()

                // Total time row
                Rectangle {
                    height: 24
                    width: parent.width
                    color: "#2D2D37"
                    Text {
                        text: "Total"
                        font.pixelSize: 11
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        color: "#FFFFFF"
                        renderType: Text.NativeRendering
                    }
                    Text {
                        text: {
                            var t = pagxStatsColumn.totalTime;
                            if (t >= 1000000) {
                                return (t / 1000000).toFixed(2) + " s";
                            } else if (t >= 1000) {
                                return (t / 1000).toFixed(2) + " ms";
                            }
                            return t + " μs";
                        }
                        font.pixelSize: 13
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        color: "#FFFFFF"
                        renderType: Text.NativeRendering
                    }
                }

                // Stacked progress bar
                Rectangle {
                    height: 20
                    width: parent.width
                    color: "#2D2D37"

                    Row {
                        id: progressBarRow
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 1

                        Repeater {
                            model: runTimeDataModel ? runTimeDataModel.frameDisplayInfoModel : null
                            Rectangle {
                                height: parent.height
                                width: {
                                    var total = Math.max(pagxStatsColumn.totalTime, 1);
                                    var cur = Number(current);
                                    return Math.max((progressBarRow.width - 2) * cur / total, 0);
                                }
                                color: colorCode
                                radius: 2
                            }
                        }
                    }
                }

                // Detail rows
                Repeater {
                    model: runTimeDataModel ? runTimeDataModel.frameDisplayInfoModel : null
                    Rectangle {
                        height: 20
                        width: pagxStatsColumn.width
                        color: "#2D2D37"

                        Rectangle {
                            width: 4
                            height: 12
                            radius: 2
                            color: colorCode
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                        }

                        Text {
                            text: name
                            font.pixelSize: 10
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 18
                            color: "#9B9B9B"
                            renderType: Text.NativeRendering
                        }

                        Text {
                            text: {
                                var total = Math.max(pagxStatsColumn.totalTime, 1);
                                var cur = Number(current);
                                return Math.round(cur * 100 / total) + "%";
                            }
                            font.pixelSize: 10
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: "#9B9B9B"
                            renderType: Text.NativeRendering
                        }

                        Text {
                            text: current + " μs"
                            font.pixelSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            color: "#FFFFFF"
                            renderType: Text.NativeRendering
                        }
                    }
                }
            }

            ListView {
                id: dataListView
                visible: runTimeDataModel && runTimeDataModel.hasFrameTimeline

                width: parent.width
                boundsBehavior: ListView.StopAtBounds
                interactive: false
                anchors.fill: parent
                spacing: 1

                model: runTimeDataModel ? runTimeDataModel.frameDisplayInfoModel : null

                delegate: Item {
                    height: 20
                    Row {
                        spacing: 1
                        Rectangle {
                            height: 20
                            width: (dataListView.width - 2) / 3
                            color: "#2D2D37"
                            Text {
                                text: name
                                font.pixelSize: 10
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                color: colorCode
                                renderType: Text.NativeRendering
                            }
                            Text {
                                text: current
                                font.pixelSize: 12
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                color: "white"
                                renderType: Text.NativeRendering
                            }
                        }
                        Rectangle {
                            height: 20
                            width: (dataListView.width - 2) / 3
                            color: "#2D2D37"
                            Text {
                                text: "AVG"
                                font.pixelSize: 10
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                color: colorCode
                                renderType: Text.NativeRendering
                            }
                            Text {
                                text: avg
                                font.pixelSize: 12
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                color: "white"
                                renderType: Text.NativeRendering
                            }
                        }
                        Rectangle {
                            height: 20
                            width: (dataListView.width - 2) / 3
                            color: "#2D2D37"
                            Text {
                                text: "MAX"
                                font.pixelSize: 10
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                color: colorCode
                                renderType: Text.NativeRendering
                            }
                            Text {
                                text: max
                                font.pixelSize: 12
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                color: "white"
                                renderType: Text.NativeRendering
                            }
                        }
                    }
                }
            }
        }

        /* Spacer before Node Statistics (only visible in PAGX mode) */
        Item {
            width: 1
            height: sectionSpacing
            visible: nodeStatistics.visible
        }

        /* Node Statistics for PAGX */
        Rectangle {
            id: nodeStatistics
            visible: runTimeDataModel && runTimeDataModel.nodeStatsModel.totalCount > 0
            height: visible ? nodeStatsContent.height : 0
            width: parent.width
            color: "#00000000"

            Column {
                id: nodeStatsContent
                width: parent.width
                spacing: 1

                // Title row
                Rectangle {
                    height: 24
                    width: parent.width
                    color: "#2D2D37"
                    Text {
                        text: qsTr("Node Distribution")
                        font.pixelSize: 11
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        color: "#FFFFFF"
                        renderType: Text.NativeRendering
                    }
                    Text {
                        text: runTimeDataModel && runTimeDataModel.nodeStatsModel ? runTimeDataModel.nodeStatsModel.totalCount + " " + qsTr("nodes") : ""
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        color: "#9B9B9B"
                        renderType: Text.NativeRendering
                    }
                }

                // Stacked bar showing distribution
                Rectangle {
                    height: 20
                    width: parent.width
                    color: "#2D2D37"

                    Row {
                        id: nodeBarRow
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 1

                        Repeater {
                            model: runTimeDataModel ? runTimeDataModel.nodeStatsModel : null
                            Rectangle {
                                height: parent.height
                                width: {
                                    var total = runTimeDataModel && runTimeDataModel.nodeStatsModel ? Math.max(runTimeDataModel.nodeStatsModel.totalCount, 1) : 1;
                                    return Math.max((nodeBarRow.width - 10) * count / total, 0);
                                }
                                color: colorCode
                                radius: 2
                            }
                        }
                    }
                }

                // Detail rows
                Repeater {
                    model: runTimeDataModel ? runTimeDataModel.nodeStatsModel : null
                    Rectangle {
                        height: 20
                        width: nodeStatsContent.width
                        color: "#2D2D37"

                        Rectangle {
                            width: 4
                            height: 12
                            radius: 2
                            color: colorCode
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                        }

                        Text {
                            text: name
                            font.pixelSize: 10
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 18
                            color: "#9B9B9B"
                            renderType: Text.NativeRendering
                        }

                        Text {
                            text: {
                                var total = runTimeDataModel && runTimeDataModel.nodeStatsModel ? Math.max(runTimeDataModel.nodeStatsModel.totalCount, 1) : 1;
                                return Math.round(count * 100 / total) + "%";
                            }
                            font.pixelSize: 10
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: "#9B9B9B"
                            renderType: Text.NativeRendering
                        }

                        Text {
                            text: count
                            font.pixelSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            color: "#FFFFFF"
                            renderType: Text.NativeRendering
                        }
                    }
                }
            }
        }

        /* Spacer before FPS Info (only visible in PAGX mode) */
        Item {
            width: 1
            height: sectionSpacing
            visible: fpsInfo.visible
        }

        /* Theoretical FPS for PAGX */
        Rectangle {
            id: fpsInfo
            visible: runTimeDataModel && !runTimeDataModel.hasFrameTimeline
            height: visible ? 24 : 0
            width: parent.width
            color: "#2D2D37"

            Text {
                text: qsTr("Theoretical FPS")
                font.pixelSize: 10
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 8
                color: "#9B9B9B"
                renderType: Text.NativeRendering
            }

            Text {
                text: {
                    var totalTime = pagxStatsColumn.totalTime;
                    if (totalTime <= 0)
                        return "—";
                    var fps = 1000000 / totalTime;  // totalTime is in μs
                    if (fps >= 1000)
                        return "> 1000";
                    return fps.toFixed(1);
                }
                font.pixelSize: 13
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: fpsUnit.left
                anchors.rightMargin: 4
                color: {
                    var totalTime = pagxStatsColumn.totalTime;
                    if (totalTime <= 0)
                        return "#FFFFFF";
                    var fps = 1000000 / totalTime;
                    if (fps >= 60)
                        return "#74AD59";  // Green for 60+ FPS
                    if (fps >= 30)
                        return "#DDB259";  // Yellow for 30-60 FPS
                    return "#E00F42";  // Red for < 30 FPS
                }
                renderType: Text.NativeRendering
            }

            Text {
                id: fpsUnit
                text: "FPS"
                font.pixelSize: 9
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 8
                color: "#9B9B9B"
                renderType: Text.NativeRendering
            }
        }
    }

    Component.onCompleted: {
        if (runTimeDataModel) {
            runTimeDataModel.chartDataSize = Math.max(60, (graphCanvas.width + 1) / 4);
        }
    }

    onWidthChanged: {
        if (runTimeDataModel) {
            runTimeDataModel.chartDataSize = Math.max(60, (graphCanvas.width + 1) / 4);
        }
    }
}
