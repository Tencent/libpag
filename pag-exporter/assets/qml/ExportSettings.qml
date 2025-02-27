import QtQuick 2.5
import QtQuick.Controls 2.1
import QtQuick.Window 2.13
import QtQuick.Layouts 1.3

PApplicationWindow {
    property var isPreview: false
    property var windowWidth : 666
    property var windowHeight: 715
    property var borderWidth: 1
    property var normalIconSize: 20
    property var bmpMarginRight: 11
    property var windowMarginRight: 12
    property var tabBarHeight: 47

    property var windowColor: Qt.rgba(20/255,20/255,30/255,1)
    property var conentColor: "#22222B"
    property var placeholderTips: qsTr("占位图·%1个")

    property var settingIconPath : isPreview ? "../images/export_settings.png" : "qrc:/images/export_settings.png"
    property var previewIconPath : isPreview ? "../images/export_preview.png" : "qrc:/images/export_preview.png"
    property var eportFoldIconPath : isPreview ? "../images/export_fold.png" : "qrc:/images/export_fold.png"
    property var eportOpenIconPath : isPreview ? "../images/export_open.png" : "qrc:/images/export_open.png"
    property var folderIconPath : isPreview ? "../images/export_folder.png" : "qrc:/images/export_folder.png"
    property var radioOnIconPath : isPreview ? "../images/export_radio_on.png" : "qrc:/images/export_radio_on.png"
    property var radioOffIconPath : isPreview ? "../images/export_radio_off.png" : "qrc:/images/export_radio_off.png"
    property var warnIconPath : isPreview ? "../images/exprot_warning.png" : "qrc:/images/exprot_warning.png"

    id: wind
    visible: true
    width: windowWidth
    height: windowHeight
    minimumWidth: windowWidth
    minimumHeight: windowHeight
    windowBackground: windowColor
    titlebarBackground: windowBackground
    canResize: false
    isWindows: true
    titleFontSize: 14
    maxBtnVisible: false
    minBtnVisible: false
    canMaxSize: false

    onClosing: function(closeEvent) {
        console.log("ExportSettings window close")
        exportSettingDialog.exit()
    }

    Rectangle{
        id: rootContainer
        anchors.fill: parent
        color: windowColor

        TabBar{
            id: tabBar
            width: parent.width
            height: tabBarHeight
            leftPadding: 24

            background: Rectangle{
                anchors.fill: parent
                color: windowColor
            }

            CustomTabButton{
                id: compositionTabBtn
                text: qsTr("预合成")
                width: virtualText0.width
                height: parent.height
                underLineVisible: tabBar.currentIndex == 0 ? true : false
                anchors.bottom: parent.bottom
            }

            /*
            CustomTabButton{
                id: placeHolderTabBtn
                text: placeholderTips
                width: virtualText2.width
                height: parent.height
                underLineVisible: tabBar.currentIndex == 1 ? true : false
                anchors.bottom: parent.bottom
            }*/
            
            CustomTabButton{
                id: layerEditableTabBtn
                text: qsTr("文本图层")
                width: virtualText1.width
                height: parent.height
                underLineVisible: tabBar.currentIndex == 1 ? true : false
                anchors.bottom: parent.bottom
            }
            
            CustomTabButton{
                id: imageEditableTabBtn
                text: qsTr("占位图")
                width: virtualText2.width
                height: parent.height
                underLineVisible: tabBar.currentIndex == 2 ? true : false
                anchors.bottom: parent.bottom
            }
            
            CustomTabButton{
                id: timeStretchTabBtn
                text: qsTr("时间伸缩")
                width: virtualText3.width
                height: parent.height
                underLineVisible: tabBar.currentIndex == 3 ? true : false
                anchors.bottom: parent.bottom
            }

            Text {
                id: virtualText0
                visible: false
                text: qsTr("预合成")
            }

            /*
            Text {
                id: virtualText1
                visible: false
                text: placeholderTips
            }*/
            
            Text {
                id: virtualText1
                visible: false
                text: qsTr("文本图层")
            }
            
            Text {
                id: virtualText2
                visible: false
                text: qsTr("占位图")
            }
            
            Text {
                id: virtualText3
                visible: false
                text: qsTr("时间伸缩")
            }

            Component.onCompleted:{
                rootContainer.resetPlaceHolderTabTitle()
            }
        }

        StackLayout{
            id: contentStackLayout
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: tabBar.bottom
            anchors.bottom: sureBtn.top
            anchors.bottomMargin: 24
            currentIndex: tabBar.currentIndex

            ExportCompositionPage{
                id: compositionPage
            }
/*
            ExportPlaceHolderPage{
                id: placeHolderPage
            }*/

            ExportLayerEditablePage{
                id: layerEditablePage
            }
            
            ExportImageEditablePage{
                id: imageEditablePage
            }

            ExportTimeStretchPage{
                id: timeStretchPage
            }
        }

        Rectangle{
            id: sureBtn
            width: 130
            height: 48
            color: "#1982EB"
            radius: 2
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            anchors.rightMargin: windowMarginRight

            Text{
                font.pixelSize: 18
                font.family: "PingFang SC"
                anchors.centerIn: parent
                text: qsTr("确认")
                font.bold: true
                color: "#FFFFFF"
                renderType: Text.NativeRendering
            }

            MouseArea{
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.PointingHandCursor
                onPressed: {
                    wind.close()
                }
            }
        }

        Connections{
            target: exportSettingDialog
            onBmpStatusChange:{
                console.log("onBmpStatusChange")
                rootContainer.resetPlaceHolderTabTitle()
                placeHolderTabBtn.resetTitleWidth()
                placeHolderPage.showOrHideRightPart()
            }
        }

        function resetPlaceHolderTabTitle(argument) {
            var placeHolderSize = exportSettingDialog.getPlaceHolderSize()
            var tip = placeholderTips.arg(placeHolderSize);
            placeHolderTabBtn.text = tip;
            console.log("qml placeholder size:" + placeHolderSize)
        }
    }
}
