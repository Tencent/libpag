import PAG
import QtQuick
import QtQuick.Controls

Item {
    id: element

    property alias graph: graph
    property alias chartCanvas: chartCanvas
    property alias currentFrameText: currentFrameView.text
    property alias performanceWarnLeftDialog: performanceWarnLeftDialog
    property alias performanceWarnRightDialog: performanceWarnRightDialog

    property var warn1: qsTr("UI material pre PAG file must be 0, Template material pre PAG file must be less than or equal to 2")
    property var warn2: qsTr("Video sequence frame comparison consumes performance")
    property var warn3: qsTr("Memory usage is too large, suggest optimizing memory cost to under 50M")
    property var warn4: qsTr("Memory usage is too large, suggest optimizing memory cost to under 10M")
    property var warn5: qsTr("Too many layers, Suggest combining same layers into composition")
    property var warn6: qsTr("Sticker scale is too large, suggest reducing sticker scale")
    property var warn7: qsTr("(Run performance benchmark, results will be more accurate)")
    property var warn8: qsTr("Rendering time too long, suggest optimizing time cost to under %1 us")

    width: 293
    height: 286

    Rectangle {
        id: performanceScene

        height:25
        color: "#2D2D37"
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.right: gridView.right
        anchors.rightMargin: 0
        anchors.left: gridView.left
        anchors.leftMargin: 1

        Label {
            text: qsTr("File Info")
            font.pixelSize: 8
            color: "#FFFFFF"
            anchors.left: parent.left
            anchors.leftMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            visible: true
        }

        Item {
            id: encryptionItem

            width: 40
            height: 25
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter

            Image {
                id: encryptionIcon

                width: 11
                height: 11

                anchors.right: encryptionLabel.left
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/image/un-encryption.png"
                fillMode: Image.PreserveAspectFit
            }

            Text {
                id: encryptionLabel

                text: qsTr("Unencrypted")
                font.pixelSize: 8
                color: "#FFFFFF"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Text {
            id: virtualText1

            text: qsTr("Password Encryption")
            font.pixelSize: 8
            color: "#FFFFFF"
            visible: false
        }

        Text {
            id: virtualText2

            text: qsTr("Certificate Encryption")
            font.pixelSize: 8
            color: "#FFFFFF"
            visible: false
        }

        Text {
            id: virtualText3

            text: qsTr("Unencrypted")
            font.pixelSize: 8
            color: "#FFFFFF"
            visible: false
        }

        Text {
            id: virtualText4

            text: qsTr("Enterprise Edition")
            font.pixelSize: 8
            color: "#FFFFFF"
            visible: false

        }
    }

    GridView {
        id: gridView

        height: 94
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationWraps: false
        anchors.top: parent.top
        anchors.topMargin: 12 + 25 + 1
        anchors.left: parent.left
        anchors.leftMargin: 11
        anchors.right: parent.right
        anchors.rightMargin: 12
        cellWidth: gridView.width / 4
        cellHeight: 47

        model: fileInfoModel

        delegate: Item {
            x: 5
            height: 46

            Column {
                id: column

                Rectangle {
                    id: rectangle1

                    x: 1
                    width: gridView.width / 4 - 1
                    height: 46
                    color: "#2D2D37"

                    Text {
                        id: nameText

                        text: name
                        font.pixelSize: 9
                        renderType: Text.NativeRendering
                        color: "#9b9b9b"
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.top: parent.top
                        anchors.topMargin: 4

                    }

                    Text {
                        id: valueText

                        text: value
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                        renderType: Text.NativeRendering
                        color: "#ffffff"
                        anchors.top: nameText.bottom
                        anchors.topMargin: 2
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 2
                        anchors.left: nameText.left
                        anchors.leftMargin: 0
                    }

                    Text {
                        text: ext
                        font.pixelSize: 9
                        renderType: Text.NativeRendering
                        color: "#9b9b9b"
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 5
                        anchors.left: valueText.right
                        anchors.leftMargin: 3
                    }

                    PerformanceWarnDialog {
                        id: performanceWarnDialog

                        x: width / 3
                        y: -height / 2
                        dialogType: PerformanceWarnDialog.DialogType.Center
                    }

                    Image {
                        id: performanceWarnImg

                        width: 11
                        height: 10
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        visible: element.isShowFormWarnIcon(name, value, ext)
                        source: "qrc:/image/performance-warn.png"

                        MouseArea {
                            anchors.fill: parent

                            onClicked: {
                                performanceWarnDialog.x = performanceWarnImg.x + performanceWarnImg.width - (performanceWarnDialog.width / 2)
                                performanceWarnDialog.y = performanceWarnImg.y + performanceWarnImg.height + 2
                                if(name === "Videos") {
                                    performanceWarnDialog.warnString1 = warn1;
                                    performanceWarnDialog.warnString2 = warn2;
                                    performanceWarnDialog.warnString3 = ""
                                    performanceWarnDialog.open()
                                } else {
                                    if(settings.currentBenchmarkSceneType === PAGBenchmarkModel.BENCHMARK_SCENE_TYPE_TEMPLATE){
                                        performanceWarnDialog.warnString1 = warn3;
                                    } else {
                                        performanceWarnDialog.warnString1 = warn4;
                                    }
                                    performanceWarnDialog.warnString2 = warn5;
                                    performanceWarnDialog.warnString3 = warn6;
                                    performanceWarnDialog.open()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: chart

        color: "#00000000"
        anchors.top: parent.top
        anchors.topMargin: 109 + 25 + 1
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        anchors.left: gridView.left
        anchors.leftMargin: 1
        anchors.right: gridView.right
        anchors.rightMargin: 0

        Rectangle {
            id: graph

            height: 90
            color: "#2D2D37"
            clip: true
            anchors.top: parent.top
            anchors.topMargin: 0
            anchors.right: parent.right
            anchors.left: parent.left

            Canvas {
                id: chartCanvas

                property int posX: 0
                property int posY: 0
                property int spaceX: 1
                property int rectWidth: 3
                property variant warnRenderColor: [224, 15, 66]

                width: graph.width
                height: 70
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 0
				renderStrategy: Canvas.Cooperative

                MouseArea {
                    anchors.fill:parent

                    onClicked: {
                        let sceneType = settings.currentBenchmarkSceneType
                        console.log("--- sceneType: ---",sceneType)

                        console.log(" mouseX: ", mouseX, "  mouseY: ", mouseY)
                        let index = parseInt((mouseX - chartCanvas.posX) / (chartCanvas.rectWidth + chartCanvas.spaceX));

                        let chartModel = runTimeModelManager.chartModel;
                        let item = chartModel.items[index];

                        if ((index === (chartModel.index + 1)) || (index === (chartModel.index + 2))) {
                            return;
                        }
                        let rectHeight = getHeight(item.renderValue, chartModel.maxValue, chartCanvas.height);
                        if ((mouseY - chartCanvas.posY) < (chartCanvas.height - rectHeight)) {
                            return;
                        }

                        let benchmarkTime = 0;
                        if (sceneType === PAGBenchmarkModel.BENCHMARK_SCENE_TYPE_TEMPLATE) {
                            benchmarkTime = settings.templateAvgRenderingTime;
                        } else {
                            benchmarkTime = settings.uiAvgRenderingTime;
                        }
                        let ceilNumber = Math.ceil(benchmarkTime/100)*100;
                        console.log("BenchmarkTime: ",benchmarkTime, " CeilNumber: ", ceilNumber)
                        let warnString1 = warn8.arg(ceilNumber);
                        if (benchmarkTime === 30000) {
                            warnString1 += warn7;
                        }

                        if (item.renderValue > benchmarkTime) {
                            if(mouseX < (element.width - performanceWarnLeftDialog.width)) {
                                performanceWarnLeftDialog.x = mouseX
                                performanceWarnLeftDialog.y = mouseY - performanceWarnLeftDialog.height/2+20
                                performanceWarnLeftDialog.warnString1 = warnString1;
                                performanceWarnLeftDialog.warnString2 = warn5;
                                performanceWarnLeftDialog.warnString3 = warn6;
                                performanceWarnLeftDialog.open()
                            } else {
                                performanceWarnRightDialog.x = mouseX-performanceWarnRightDialog.width
                                performanceWarnRightDialog.y = mouseY - performanceWarnRightDialog.height/2+20
                                performanceWarnRightDialog.warnString1 = warnString1;
                                performanceWarnRightDialog.warnString2 = warn5;
                                performanceWarnRightDialog.warnString3 = warn6;
                                performanceWarnRightDialog.open()
                            }
                            
                            pagViewer.isPlaying = false
                        }
                        
                    }
                }
            }

            PerformanceWarnDialog {
                id: performanceWarnLeftDialog

                x: width / 3
                y: -height / 2
                dialogType: PerformanceWarnDialog.DialogType.Left
            }

            PerformanceWarnDialog {
                id: performanceWarnRightDialog

                x: width / 3
                y: -height / 2
                dialogType: PerformanceWarnDialog.DialogType.Right
            }

            Text {
                id: currentFrameView

                text: qsTr("4/24")
                font.pixelSize: 9
                color: "#9B9B9B"
                renderType: Text.NativeRendering
                anchors.top: parent.top
                anchors.topMargin: 5
                anchors.right: parent.right
                anchors.rightMargin: 5
            }
        }

        /* Data Statistic */
        Rectangle {
            id: statistic

            color: "#00000000"
            anchors.top: graph.bottom
            anchors.topMargin: 4
            anchors.bottom: parent.bottom
            anchors.bottomMargin: -1
            anchors.left: parent.left
            anchors.leftMargin: 0
            anchors.right: parent.right

            ListView {
                id: listView

                boundsBehavior: Flickable.StopAtBounds
                interactive: false
                anchors.fill: parent
                spacing: 1

                model: runTimeModelManager.dataModel

                delegate: Item {
                    height: 20
                    anchors.rightMargin: 0
                    anchors.leftMargin: 0

                    Row {
                        /* Current Frame Time Cost */
                        Rectangle {
                            id: rectangle

                            width: (listView.width - 2) / 3
                            height: 20
                            color: "#2D2D37"

                            Text {
                                text: name
                                font.pixelSize: 10
                                renderType: Text.NativeRendering
                                color: colorCode
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                            }

                            Text {
                                text: current
                                font.pixelSize: 12
                                renderType: Text.NativeRendering
                                color: "white"
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                            }
                        }

                        Rectangle {
                            width: 1
                            height: 20
                            color: "#00000000"
                        }

                        /* Average Time Cost */
                        Rectangle {
                            width: (listView.width - 2) / 3
                            height: 20
                            color: "#2D2D37"

                            Text {
                                text: qsTr("AVG")
                                font.pixelSize: 10
                                renderType: Text.NativeRendering
                                color: colorCode
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 8

                            }

                            Text {
                                text: avg
                                font.pixelSize: 12
                                renderType: Text.NativeRendering
                                color: "white"
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: 8

                            }
                        }

                        Rectangle {
                            width: 1
                            height: 20
                            color: "#00000000"
                        }

                        /* Max Time Cost */
                        Rectangle {
                            width: (listView.width - 2) / 3
                            height: 20
                            color: "#2D2D37"

                            Text {
                                text: qsTr("MAX")
                                font.pixelSize: 10
                                renderType: Text.NativeRendering
                                color: colorCode
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 8

                            }

                            Text {
                                text: max
                                font.pixelSize: 12
                                renderType: Text.NativeRendering
                                color: "white"
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: 8

                            }
                        }
                    }
                }
            }
        }
    }

    function isShowFormWarnIcon(name, value, ext) {
        if ((name === "Graphics") && (value > 10) && (ext === "MB") && (settings.currentBenchmarkSceneType === PAGBenchmarkModel.BENCHMARK_SCENE_TYPE_UI)) {
            return true;
        }
        if ((name === "Graphics") && (value > 50) && (ext === "MB") && (settings.currentBenchmarkSceneType === PAGBenchmarkModel.BENCHMARK_SCENE_TYPE_TEMPLATE)) {
            return true;
        }
        if ((name === "Videos") && (value > 2) && (settings.currentBenchmarkSceneType === PAGBenchmarkModel.BENCHMARK_SCENE_TYPE_TEMPLATE)) {
            return true;
        }
        if ((name === "Videos") && (value > 0) && (settings.currentBenchmarkSceneType === PAGBenchmarkModel.BENCHMARK_SCENE_TYPE_UI)) {
            return true;
        }

        return false;
    }

    function changeEncryptedState(pagFilePath) {
        encryptionIcon.source = "qrc:/image/unencrypted.png";
        encryptionLabel.text = qsTr("Unencrypted");
        encryptionItem.width = virtualText3.width + 14
    }

    function pagFileChanged(pagFilePath) {
        if (!pagFilePath) {
            return
        }
        changeEncryptedState(pagFilePath)
    }
}
