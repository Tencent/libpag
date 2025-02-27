import QtQuick 2.12
import QtQuick.Templates 2.12 as T
import QtQuick.Controls 2.12
import QtQuick.Controls.impl 2.12
 
T.ComboBox {
    id:control
 
    //checked选中状态，down按下状态，hovered悬停状态
    property color backgroundTheme: "#383840"
    //下拉框背景色
    property color backgroundColor: backgroundTheme
    //边框颜色
    property color borderColor: "transparent"
    //item高亮颜色
    property color itemHighlightColor: "#0063E1"
    //item普通颜色
    property color itemNormalColor: backgroundTheme
    //每个item的高度
    property int itemHeight: height + 3
    //每个item文本的左右padding
    property int itemPadding: 10
    //下拉按钮左右距离
    property int indicatorPadding: 3
    //下拉按钮图标
    property url indicatorSource: isPreview ? "../images/down_arrow.png" : "qrc:/images/down_arrow.png"
    //圆角
    property int radius: 2
    //最多显示的item个数
    property int showCount: 4
    //文字颜色
    property color textColor: "white"
    //model数据左侧附加的文字
    property string textLeft: ""
    //model数据右侧附加的文字
    property string textRight: ""
 
    implicitWidth: 120
    implicitHeight: 30
    spacing: 0
    leftPadding: padding
    rightPadding: padding + indicator.width + spacing
    font{
        family: "PingFang SC"
        pixelSize: 14
    }
 
    //各item
    delegate: ItemDelegate {
        id: box_item
        height: control.itemHeight
        //Popup如果有padding，这里要减掉2*pop.padding
        width: control.width
        padding: 0
        contentItem: Text {
            text: control.textLeft+
                  (control.textRole
                   ? (Array.isArray(control.model)
                      ? modelData[control.textRole]
                      : model[control.textRole])
                   : modelData)+
                  control.textRight
            color: control.textColor
            leftPadding: control.itemPadding
            rightPadding: control.itemPadding
            font: control.font
            elide: Text.ElideRight
            renderType: Text.NativeRendering
            verticalAlignment: Text.AlignVCenter
        }
        hoverEnabled: control.hoverEnabled
        background: Rectangle{
            radius: control.radius
            color: (control.highlightedIndex === index)
                   ? control.itemHighlightColor
                   : control.itemNormalColor
            //item底部的线
            Rectangle{
                height: 1
                width: parent.width-2*control.radius
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                color: Qt.lighter(control.itemNormalColor)
            }
        }
    }
 
    //之前用的Shape，现在替换为Image
    //图标自己画比较麻烦，还是贴图方便，使用的时候换成自己图
    indicator: Item{
        id: box_indicator
        x: control.width - width
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 16
        height: control.height
        //按钮图标
        Image {
            id: box_indicator_img
            anchors.centerIn: parent
            width: parent.width
            height: parent.height
            source: control.indicatorSource
        }
    }
 
    //box显示item
    contentItem: T.TextField{
        //control的leftPadding会挤过来，不要设置control的padding
        leftPadding: control.itemPadding
        rightPadding: control.itemPadding
        text: control.editable
              ? control.editText
              : (control.textLeft+control.displayText+control.textRight)
        font: control.font
        color: control.textColor
        verticalAlignment: Text.AlignVCenter
        //默认鼠标选取文本设置为false
        selectByMouse: true
        //选中文本的颜色
        selectedTextColor: "green"
        //选中文本背景色
        selectionColor: "white"
        clip: true
        //renderType: Text.NativeRendering
        enabled: control.editable
        autoScroll: control.editable
        readOnly: control.down
        inputMethodHints: control.inputMethodHints
        validator: control.validator
        renderType: Text.NativeRendering
        background: Rectangle {
            visible: control.enabled && control.editable
            border.width: parent && parent.activeFocus ? 1 : 0
            border.color: control.itemHighlightColor
            color: "transparent"
        }
    }
 
    //box框背景
    background: Rectangle {
        implicitWidth: control.implicitWidth
        implicitHeight: control.implicitHeight
        radius: control.radius
        color: control.backgroundColor
        border.color: control.borderColor
    }
 
    //弹出框
    popup: T.Popup {
        //默认向下弹出，如果距离不够，y会自动调整（）
        y: control.height + 1
        width: control.width
        //根据showCount来设置最多显示item个数
        implicitHeight: control.delegateModel
                        ?((control.delegateModel.count<showCount)
                          ?contentItem.implicitHeight
                          :control.showCount*control.itemHeight)+2
                        :0
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            snapMode: ListView.SnapToItem
        }
    }
}
