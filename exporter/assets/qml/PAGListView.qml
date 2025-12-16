import QtQuick 2.5
import QtQuick.Controls 2.3
import QtQuick.Window 2.13

Rectangle {
    id: root
    property var isPreview: false
    property var normalColor: '#22222C'
    property var secondRowColor: '#272730'
    property var warnIconSize: 20
    property var topMargin: 8
    property var itemBottomMargin: topMargin - 6
    property var itemRightMargin: 13
    property var secondLineMarginTop: 7
    property var normalMargin: 10
    property var fontSize: 16
    property var titleMarginTop: 4
    property var titleIconMarginTop: -2
    property var fontFamily: "PingFang SC"

    property string errorIconPath: isPreview ? "../images/export-wrong.png" : "qrc:/images/export-wrong.png"
    property string foldIconPath: isPreview ? "../images/folder-fold.png" : "qrc:/images/folder-fold.png"
    property string openIconPath: isPreview ? "../images/folder-unfold.png" : "qrc:/images/folder-unfold.png"
    property string rightArrowIconPath: isPreview ? "../images/right-arrow.png" : "qrc:/images/right-arrow.png"
    property string bmpIconPath: isPreview ? "../images/bmp.png" : "qrc:/images/bmp.png"
    property string warnIconPath: isPreview ? "../images/export-warning.png" : "qrc:/images/export-warning.png"
    property var model: null
    property bool showLocationBtn: false

    signal itemClicked(int index, var modelData)
    visible: true
    width: parent.width
    height: parent.height
    color: normalColor
    clip: true

    TextHelper {
        id: textHelper
    }

    Component {
        id: errorItemDelegate
        Rectangle {
            id: wrapper
            property int baseHeight: 44
            property int expandedHeight: {
                if (isFold) {
                    return baseHeight + suggestText.implicitHeight + locationBtn.height + secondLineMarginTop + itemBottomMargin;
                } else {
                    return baseHeight;
                }
            }

            width: errorListView.width
            height: expandedHeight
            color: index % 2 ? secondRowColor : normalColor

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    isFold = !isFold;
                    root.itemClicked(index, model);
                }
            }

            Image {
                id: warnIcon
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: 12
                anchors.topMargin: topMargin
                width: warnIconSize
                height: warnIconSize
                source: isError ? errorIconPath : warnIconPath
            }

            Image {
                id: compIcon
                anchors.left: warnIcon.right
                anchors.top: warnIcon.top
                anchors.leftMargin: 10
                width: warnIconSize
                height: warnIconSize
                source: bmpIconPath
            }

            Text {
                id: mainText
                property string fullText: {
                    if (hasLayerName) {
                        return compositionName + "  ▶  " + layerName + ": " + errorInfo;
                    } else {
                        return compositionName + "  ▶  " + errorInfo;
                    }
                }

                text: fullText
                anchors.left: compIcon.right
                anchors.right: foldIcon.left
                anchors.top: compIcon.top
                anchors.leftMargin: 7
                anchors.rightMargin: 7
                anchors.topMargin: titleMarginTop - 4

                font.pixelSize: fontSize
                font.family: fontFamily
                color: "white"
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                height: Math.max(compIcon.height, implicitHeight)
            }

            Image {
                id: foldIcon
                anchors.right: parent.right
                anchors.top: warnIcon.top
                anchors.rightMargin: itemRightMargin
                anchors.topMargin: 4 + titleIconMarginTop
                width: 8
                height: 8
                source: isFold ? openIconPath : foldIconPath
            }

            ErrorTextArea {
                id: suggestText
                text: suggestion
                visible: isFold
                targetListView: errorListView
                anchors.top: mainText.bottom
                anchors.left: compIcon.left
                anchors.right: root.showLocationBtn ? locationBtn.left : parent.right
                anchors.topMargin: secondLineMarginTop
                anchors.rightMargin: root.showLocationBtn ? 8 : itemRightMargin
                horizontalAlignment: Text.AlignLeft

                font.family: fontFamily
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                height: visible ? implicitHeight : 0
            }

            Rectangle {
                id: locationBtn
                width: 50
                height: 22
                color: "#1982EB"
                radius: 2
                anchors.right: parent.right
                anchors.rightMargin: itemRightMargin
                anchors.verticalCenter: suggestText.verticalCenter
                visible: isFold && root.showLocationBtn

                Text {
                    font.pixelSize: 12
                    font.family: fontFamily
                    anchors.centerIn: parent
                    text: qsTr("Locate")
                    font.bold: true
                    color: "#FFFFFF"
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    propagateComposedEvents: true
                    onClicked: {
                        if (typeof alertInfoModel !== 'undefined' && alertInfoModel) {
                            alertInfoModel.locateAlert(index);
                        }
                    }
                }
            }
        }
    }

    ListView {
        id: errorListView
        width: parent.width
        height: parent.height
        model: root.model || (typeof alertInfoModel !== 'undefined' ? alertInfoModel : null)
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

            background: Rectangle {
                color: "transparent"
            }

            contentItem: Rectangle {
                implicitWidth: 3
                implicitHeight: 100
                radius: 3
                color: "#383840"
            }
        }
    }
}
