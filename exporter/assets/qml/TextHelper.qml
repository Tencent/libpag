import QtQuick 2.15

QtObject {
    id: textHelper

    function elideText(text, fontSize, maxWidth, fontFamily) {
        if (!text || maxWidth <= 0)
            return text;

        var metrics = textMetrics;
        metrics.font.pixelSize = fontSize || 14;
        metrics.font.family = fontFamily || "PingFang SC";
        metrics.text = text;

        if (metrics.width <= maxWidth) {
            return text;
        }

        var left = 0;
        var right = text.length;
        var result = text;

        while (left < right) {
            var mid = Math.floor((left + right + 1) / 2);
            var testText = text.substring(0, mid) + "...";
            metrics.text = testText;

            if (metrics.width <= maxWidth) {
                result = testText;
                left = mid;
            } else {
                right = mid - 1;
            }
        }

        return result;
    }

    function addFirstLineIndent(indentDistance, text, fontSize, fontFamily) {
        if (!text || indentDistance <= 0) {
            return {
                layerName: text,
                addSpaceCount: 0
            };
        }

        var metrics = textMetrics;
        metrics.font.pixelSize = fontSize || 14;
        metrics.font.family = fontFamily || "PingFang SC";

        var space = " ";
        var finalText = "";
        var addSpaceCount = 0;

        while (true) {
            metrics.text = finalText;
            if (metrics.width >= indentDistance) {
                break;
            }
            finalText += space;
            addSpaceCount++;

            if (addSpaceCount > 100) {
                break;
            }
        }

        finalText += text;

        return {
            layerName: finalText,
            addSpaceCount: addSpaceCount
        };
    }

    function getTextWidth(text, fontSize, fontFamily) {
        if (!text)
            return 0;

        var metrics = textMetrics;
        metrics.font.pixelSize = fontSize || 14;
        metrics.font.family = fontFamily || "PingFang SC";
        metrics.text = text;

        return metrics.width;
    }

    property TextMetrics textMetrics: TextMetrics {
        id: textMetrics
        font.pixelSize: 14
        font.family: "PingFang SC"
        text: ""
    }
}
