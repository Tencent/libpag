import PAG
import QtQuick

ProfilerForm {
    PAGRunTimeModelManager {
        id: runTimeModelManager

        objectName: "runTimeModelManager"
        chartSize: (chartCanvas.width + 1) / 4

        chartModel.onItemsChange: {
            chartCanvas.requestPaint();
        }

        onDataChange: {
            currentFrameText = (runTimeModelManager.currentFrame + 1) + qsTr("/") + runTimeModelManager.totalFrame;
        }
    }

    chartCanvas {
        onPaint: {
            let ctx = chartCanvas.getContext("2d");
            let posX = chartCanvas.posX;
            let posY = chartCanvas.posY;
            let rectWidth = chartCanvas.rectWidth;
            let spaceX = chartCanvas.spaceX;
            let spaceY = 0.1;
            let chartModel = runTimeModelManager.chartModel;
            ctx.clearRect(posX, posY, chartCanvas.width, chartCanvas.height);
            let alpha = 1;
            for (let i = 0; i < chartModel.size; i++) {
                if (i <= chartModel.index) {
                    alpha = 1;
                } else if (i <= (chartModel.index + 2)) {
                    posX += spaceX + rectWidth;
                    continue;
                } else {
                    alpha = 0.43;
                }
                let renderColor = Qt.rgba(0, 150 / 255, 216 / 255, alpha);
                let imageColor = Qt.rgba(116 / 255, 173 / 255, 89 / 255, alpha);
                let presentColor = Qt.rgba(221 / 255, 178 / 255, 89 / 255, alpha);
                //draw last line
                let item = chartModel.items[i];

                let benchmarkTime = 0;
                let benchmarkFirstFrameTime = 0;
                let sceneType = settings.currentBenchmarkSceneType;
                if (sceneType === PAGBenchmarkModel.BENCHMARK_SCENE_TYPE_TEMPLATE) {
                    benchmarkTime = settings.templateAvgRenderingTime;
                    benchmarkFirstFrameTime = settings.templateFirstFrameRenderingTime;
                } else {
                    benchmarkTime = settings.uiAvgRenderingTime;
                    benchmarkFirstFrameTime = settings.uiFirstFrameRenderingTime;
                }
                if ((posX === 0) && (item.renderValue > benchmarkFirstFrameTime)) { //画首帧
                    renderColor = Qt.rgba(chartCanvas.warnRenderColor[0]/255, chartCanvas.warnRenderColor[1]/255, chartCanvas.warnRenderColor[2]/255,alpha);
                    imageColor = renderColor;
                    presentColor = renderColor;
                    spaceY = -0.1;
                } else if (item.renderValue > benchmarkTime) {
                    renderColor = Qt.rgba(chartCanvas.warnRenderColor[0]/255, chartCanvas.warnRenderColor[1]/255, chartCanvas.warnRenderColor[2]/255,alpha);
                    imageColor = renderColor;
                    presentColor = renderColor;
                    spaceY = -0.1;
                }

                let rectHeight = getHeight(item.presentValue, chartModel.maxValue, chartCanvas.height);
                drawRect(ctx,{x: posX, y: chartCanvas.height - posY - rectHeight, width: rectWidth, height: rectHeight}, presentColor);
                posY += rectHeight + spaceY;
                rectHeight = getHeight(item.imageValue, chartModel.maxValue, chartCanvas.height);
                drawRect(ctx,{x: posX, y: chartCanvas.height - posY - rectHeight, width: rectWidth, height: rectHeight}, imageColor);
                posY += rectHeight + spaceY;
                rectHeight = getHeight(item.renderValue, chartModel.maxValue, chartCanvas.height);
                drawRect(ctx,{x: posX, y: chartCanvas.height - posY - rectHeight, width: rectWidth, height: rectHeight}, renderColor);
                posX += spaceX + rectWidth;
                posY = 0;
            }
        }
    }

    function drawRect(ctx, rect, color) {
        ctx.save();
        ctx.fillStyle = color;
        ctx.fillRect(rect.x, rect.y, rect.width, rect.height);
        ctx.restore();
    }

    function getHeight(value, max, height) : int {
        let minHeight = 0.5;
        let r = Math.max(value / max * height, minHeight)

        return r;
    }
}
