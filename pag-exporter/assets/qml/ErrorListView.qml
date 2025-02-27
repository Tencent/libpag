import QtQuick 2.5
import QtQuick.Controls 2.3
import QtQuick.Window 2.13

Rectangle {
    property var isPreview: false
    property var normalColor: '#22222C'
    property var secondRowColor: '#272730'
    property var warnIconSize: 20
    property var topMargin: 8
    property var itemBottomMargin: topMargin - 6
    property var itemRightMargin: 13
    property var secondLineMarginTop: 7
    property var normalMargin : 10
    property var fontSize : 14
    property var titleMarginTop:errorListModel.isWindows()?4:0
    property var titleIconMarginTop:errorListModel.isWindows()?-2:0

    property var errorIconPath : isPreview ? "../images/exprot_wrong.png" : "qrc:/images/exprot_wrong.png"
    property var foldIconPath : isPreview ? "../images/export_fold.png" : "qrc:/images/export_fold.png"
    property var openIconPath : isPreview ? "../images/export_open.png" : "qrc:/images/export_open.png"
    property var rightArrowIconPath : isPreview ? "../images/right_arrow.png" : "qrc:/images/right_arrow.png"
    property var bmpIconPath : isPreview ?  "../images/export_bmp.png" : "qrc:/images/export_bmp.png"
    property var warnIconPath : isPreview ? "../images/exprot_warning.png" : "qrc:/images/exprot_warning.png"

    id:root
    visible: true
    width: parent.width
    height: parent.height
    color:normalColor;
    clip: true

    Component {
        id: errorItemDelegate
        Rectangle {
            property var sechondLineHeight: Math.max(suggestText.height, locationBtn.height)

            id: wrapper
            width: errorListView.width
            height: isFold ? (titleText.height + sechondLineHeight + topMargin + itemBottomMargin + secondLineMarginTop):(titleText.height + topMargin + itemBottomMargin)
            color: index%2 ? secondRowColor:normalColor

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    console.log("whole item clicked")
                    isFold = !isFold;
                }
            }

            Image {
                id:warnIcon
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: 12
                anchors.topMargin: topMargin
                width: warnIconSize
                height: warnIconSize
                source: isError?errorIconPath:warnIconPath
            }

            Image {
                id:compIcon
                anchors.left: warnIcon.right
                anchors.top: warnIcon.top
                anchors.leftMargin: 10
                width: warnIconSize
                height: warnIconSize
                source: bmpIconPath
            }

            ErrorTextArea {
                property var marginLeft: 7
                property var compoNameMaxWidth: parent.width - compIcon.x - compIcon.width - marginLeft - itemRightMargin - foldIcon.width - arrowIcon.width - normalMargin - 20
                property var finalShowCompoName: ExportUtils.elideText(compositionName, fontSize, compoNameMaxWidth)

                id: comNameText
                text: finalShowCompoName
                anchors.top: compIcon.top
                anchors.left: compIcon.right
                anchors.leftMargin: marginLeft
                anchors.topMargin: titleMarginTop - 4
                verticalAlignment: Text.AlignTop
                font.pixelSize: fontSize
                z: 100
            }

            Image {
                id:arrowIcon
                anchors.left: comNameText.right
                anchors.top: comNameText.top
                anchors.leftMargin: normalMargin
                anchors.topMargin: comNameText.height/2-height/2 + titleIconMarginTop
                width: 6
                height: 8
                source: rightArrowIconPath
            }

            ErrorTextArea {
                property var originOffsetX : compIcon.x;
                property var realOffsetX : arrowIcon.x + arrowIcon.width + 7;
                property var distance : realOffsetX - originOffsetX
                property var maxWidth: parent.width - compIcon.x - itemRightMargin - foldIcon.width;
                property var elideLayerName: ExportUtils.elideText(layerName, fontSize, maxWidth)
                property var errorTitleInfo : hasLayerName?(elideLayerName + ": " + errorInfo):errorInfo;
                property var indentData: ExportUtils.addFirstLineIndent(distance, errorTitleInfo, fontSize)
                property var finalLayerName : indentData.getLayerName()
                property var addSpaceCount: indentData.getAddSpaceCount()

                id: titleText
                text: finalLayerName
                anchors.top: comNameText.top
                anchors.left: compIcon.left
                anchors.right: foldIcon.left
                anchors.rightMargin: 7
                font.pixelSize: fontSize
                beginPosition: addSpaceCount
                z: 99

                Component.onCompleted:{
                    console.log("finalLayerName:" + finalLayerName + ", addSpaceCount:" + addSpaceCount)
                }
            }

            Image {
                id: foldIcon
                anchors.right: parent.right
                anchors.top: warnIcon.top
                anchors.rightMargin: itemRightMargin
                anchors.topMargin: 4 + titleIconMarginTop
                width: 8
                height: 8
                source: isFold?openIconPath:foldIconPath
            }

            ErrorTextArea {
                id: suggestText
                text: suggestion
                visible:isFold?true:false;
                anchors.topMargin: secondLineMarginTop
                anchors.top: titleText.bottom
                anchors.left: compIcon.left
                anchors.right: locationBtn.left
                anchors.rightMargin: 8
            }

            Rectangle{
                id: locationBtn
                width: 50
                height: 22
                color: "#1982EB"
                radius: 2
                anchors.right: parent.right
                anchors.rightMargin: itemRightMargin
                anchors.top: suggestText.top
                anchors.bottomMargin: itemBottomMargin
                visible: isFold

                Text{
                    font.pixelSize: 12
                    font.family: "PingFang SC"
                    anchors.centerIn: parent
                    text: qsTr("定位")
                    font.bold: true
                    color: "#FFFFFF"
                }

                MouseArea{
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    propagateComposedEvents: true
                    onClicked: {
                        console.log("localBtn clicked")  
                        errorListModel.locationError(index)
                    }
                }
            }
        }
    }

    ListView{
        id:errorListView
        width: parent.width
        height: parent.height
        model:errorListModel
        delegate: errorItemDelegate

        ScrollBar.vertical: ScrollBar {
            visible: errorListView.contentHeight > errorListView.height
            hoverEnabled: true
            active: hovered || pressed
            orientation: Qt.Vertical
            size: parent.height / errorListView.height
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            policy: ScrollBar.AsNeeded
    
            contentItem: Rectangle {
                implicitWidth: 3
                implicitHeight: 100
                radius: 3
                color: "#535359"
            }
        }
    }
}