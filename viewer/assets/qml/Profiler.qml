import PAG
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: element

    property int defaultWidth: 300
    property int defaultHeight: 300

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

    Connections {
        target: runTimeDataModel

        function onDataChanged() {
            currentFrameText.text = runTimeDataModel.currentFrame + "/" + runTimeDataModel.totalFrame;
        }
    }

    Connections {
        target: runTimeDataModel.chartDataModel

        function onItemsChanged() {
            graphCanvas.requestPaint();
        }
    }

    Column {
        spacing: 0
        anchors {
            fill: parent
            leftMargin: 12
            rightMargin: 12
        }

        Item {
            width: parent.width
            height: 12
        }

        /* Title */
        Rectangle {
            id: profilerTitle
            color: "#2D2D37"
            height: 25
            anchors.left: parent.left
            anchors.right: parent.right

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

        Item {
            width: parent.width
            height: 1
        }

        /* File Info */
        GridView {
            id: fileInfoView
            height: 93
            width: parent.width
            cellWidth: width / 4
            cellHeight: 46
            boundsBehavior: Flickable.StopAtBounds
            keyNavigationWraps: false
            anchors.left: parent.left
            anchors.right: parent.right

            model: runTimeDataModel ? runTimeDataModel.fileInfoModel : null

            delegate: Item {
                width: fileInfoView.cellWidth
                height: fileInfoView.cellHeight

                Rectangle {
                    height: fileInfoView.cellHeight - 1
                    width: fileInfoView.cellWidth - ((index + 1) % 4 === 0 ? 0 : 1)
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

        Item {
            width: parent.width
            height: 2
        }

        /* performance chart */
        Rectangle {
            id: performanceChart
            height: 90
            color: "#00000000"
            anchors.left: parent.left
            anchors.right: parent.right

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

                    property var chartDataModel: runTimeDataModel.chartDataModel

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
                        let height = (currentValue / maxValue) * maxHeight * 0.85 + (maxHeight * 0.05);
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
                            let index = parseInt((mouseX - graphCanvas.posX) / (graphCanvas.rectWidth + graphCanvas.spaceX));
                            let item = runTimeDataModel.chartDataModel.items[index];
                            if (index >= (runTimeDataModel.chartDataModel.currentIndex + 1) && index <= (runTimeDataModel.chartDataModel.currentIndex + 2)) {
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
                            let benchmarkTime = settings.templateAvgRenderingTime;
                            if (benchmarkTime === 30000) {
                                warnMessage += performanceWarn3;
                            }
                            benchmarkTime = Math.ceil(benchmarkTime / 100) * 100;
                            warnMessage = warnMessage.arg(benchmarkTime);

                            if (renderTime > benchmarkTime) {
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
                        let context = graphCanvas.getContext("2d");
                        let posX = graphCanvas.posX;
                        let posY = graphCanvas.posY;
                        context.clearRect(posX, posY, graphCanvas.width, graphCanvas.height);
                        let spaceY = 0;
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
                            let benchmarkTime = settings.templateAvgRenderingTime;
                            let benchmarkFirstFrameTime = settings.templateFirstFrameRenderingTime;
                            let renderColor_ = Qt.rgba(renderColor[0], renderColor[1], renderColor[2], alpha);
                            let imageColor_ = Qt.rgba(imageColor[0], imageColor[1], imageColor[2], alpha);
                            let presentColor_ = Qt.rgba(presentColor[0], presentColor[1], presentColor[2], alpha);
                            let renderTime = Number(item.renderTime);
                            let imageDecodeTime = Number(item.imageDecodeTime);
                            let presentTime = Number(item.presentTime);

                            if ((posX === 0 && renderTime > benchmarkFirstFrameTime) || (posX !== 0 && renderTime > benchmarkTime)) {
                                renderColor_ = Qt.rgba(warnColor[0], warnColor[1], warnColor[2], alpha);
                                imageColor_ = renderColor_;
                                presentColor_ = renderColor_;
                                spaceY -= 0.1;
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
                            posY += (spaceY + rectHeight);

                            rectHeight = getRectHeight(imageDecodeTime, maxTime, graphCanvas.height);
                            rect = {
                                x: posX,
                                y: graphCanvas.height - posY - rectHeight,
                                width: rectWidth,
                                height: rectHeight
                            };
                            drawRect(context, rect, imageColor_);
                            posY += (spaceY + rectHeight);

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

        Item {
            width: parent.width
            height: 4
        }

        /* Data Statistics */
        Rectangle {
            id: statistics
            height: 62
            color: "#00000000"
            anchors.left: parent.left
            anchors.right: parent.right

            ListView {
                id: dataListView

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

        Item {
            width: parent.width
            height: 20
        }
    }

    Component.onCompleted: {
        runTimeDataModel.chartDataSize = Math.max(60, (graphCanvas.width + 1) / 4);
    }

    onWidthChanged: {
        runTimeDataModel.chartDataSize = Math.max(60, (graphCanvas.width + 1) / 4);
    }
}
