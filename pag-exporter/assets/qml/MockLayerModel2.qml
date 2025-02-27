import QtQuick 2.12
import Qt.labs.qmlmodels 1.0

TableModel {
    id: compositionModel
    TableModelColumn { display: "itemChecked" }
    TableModelColumn { display: "layerName" }
    TableModelColumn { display: "savePath" }
    TableModelColumn { display: "itemHeight" }
    TableModelColumn { display: "itemBackColor" }

    // Each row is one type of fruit that can be ordered
    rows: [
        {
            layerName: "主题合成_bmp",
            savePath: "/Users/Mike/Documents",
            isFolder: false,
            isUnFold: false,
            layerLevel: 0,
            itemChecked: true,
            itemBackColor: "#22222B",
            itemHeight: 35
        },
        {
            layerName: "主题合成_bmp",
            savePath: "/Users/Mike/Documents",
            isFolder: false,
            isUnFold: false,
            layerLevel: 0,
            itemChecked: false,
            itemBackColor: "#27272F",
            itemHeight: 35
        },
        {
            layerName: "星光",
            savePath: "/Users/Mike/Documents",
            isFolder: true,
            isUnFold: true,
            layerLevel: 0,
            itemChecked: false,
            itemBackColor: "#27272F",
            itemHeight: 35
        },
        {
            layerName: "星光_子合成1",
            savePath: "/Users/Mike/Documents",
            isFolder: true,
            isUnFold: true,
            layerLevel: 1,
            itemChecked: false,
            itemBackColor: "#27272F",
            itemHeight: 35
        },
        {
            layerName: "星光_子合成2",
            savePath: "/Users/Mike/Documents",
            isFolder: true,
            isUnFold: true,
            layerLevel: 2,
            itemChecked: false,
            itemBackColor: "#27272F",
            itemHeight: 35
        },
        {
            layerName: "星光_子合成3",
            savePath: "/Users/Mike/Documents",
            isFolder: true,
            isUnFold: true,
            layerLevel: 3,
            itemChecked: false,
            itemBackColor: "#27272F",
            itemHeight: 35
        },
        {
            layerName: "星光_子合成2_子合成2_子合成2_子合成2_子合成2_子合成2_子合成2_子合成2_子合成2_子合成2",
            savePath: "/Users/Mike/Documents",
            isFolder: false,
            isUnFold: false,
            layerLevel: 4,
            itemChecked: false,
            itemBackColor: "#27272F",
            itemHeight: 35
        },
        {
            layerName: "主题合成_bmp",
            savePath: "/Users/Mike/Downloads/Mike/Downloads/Mike/Downloads/Mike/Downloads/Mike/Downloads/Mike/Downloads/Mike/Downloads/Mike/Downloads/Mike/Downloads/Mike/Downloads/Mike/Downloads/",
            isFolder: false,
            isUnFold: false,
            layerLevel: 0,
            itemChecked: true,
            itemBackColor: "#22222B",
            itemHeight: 35
        },
        {
            layerName: "主题合成_bmp",
            savePath: "/Users/Mike/Downloads",
            isFolder: false,
            isUnFold: false,
            layerLevel: 0,
            itemChecked: false,
            itemBackColor: "#27272F",
            itemHeight: 35
        },
        {
            layerName: "主题合成a",
            savePath: "/Users/Mike/Documents",
            isFolder: false,
            isUnFold: false,
            layerLevel: 0,
            itemChecked: true,
            itemBackColor: "#22222B",
            itemHeight: 35
        },
        {
            layerName: "主题合成b",
            savePath: "/Users/Mike/Documents",
            isFolder: false,
            isUnFold: false,
            layerLevel: 0,
            itemChecked: true,
            itemBackColor: "#22222B",
            itemHeight: 35
        },
        {
            layerName: "主题合成c",
            savePath: "/Users/Mike/Documents",
            isFolder: false,
            isUnFold: false,
            layerLevel: 0,
            itemChecked: true,
            itemBackColor: "#22222B",
            itemHeight: 35
        },
        {
            layerName: "主题合成d",
            savePath: "/Users/Mike/Documents",
            isFolder: false,
            isUnFold: false,
            layerLevel: 0,
            itemChecked: true,
            itemBackColor: "#22222B",
            itemHeight: 35
        },
        {
            layerName: "主题合成e",
            savePath: "/Users/Mike/Documents",
            isFolder: false,
            isUnFold: false,
            layerLevel: 0,
            itemChecked: true,
            itemBackColor: "#22222B",
            itemHeight: 35
        }
    ]
}