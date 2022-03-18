import 'package:flutter/material.dart';

import 'flutter_pag_plugin.dart';

class PAGView extends StatefulWidget {
  double? width;
  double? height;

  // TODO: flutter资源路径，优先级比url高
  String? assetName;

  // TODO: 网络资源，动画链接
  String? url;

  // TODO: 初始化时播放进度
  double? initProgress;

  // TODO: 初始化后自动播放
  bool autoPlay;

  // TODO: 循环次数
  int? repeatCount;
  static const int REPEAT_COUNT_LOOP = -1; //无限循环
  static const int REPEAT_COUNT_DEFAULT = 1; //默认仅播放一次

  PAGView.network(this.url, {this.width, this.height, this.repeatCount, this.initProgress, this.autoPlay = false, Key? key}) : super(key: key);

  PAGView.asset(this.assetName, {this.width, this.height, this.repeatCount, this.initProgress, this.autoPlay = false, Key? key}) : super(key: key);

  @override
  PAGViewState createState() => PAGViewState();
}

class PAGViewState extends State<PAGView> {
  bool _hasLoadTexture = false;
  int _textureId = -1;

  double _rawWidth = 0;
  double _rawHeight = 0;

  @override
  void initState() {
    super.initState();
    newTexture();
  }

  void newTexture() async {
    int repeatCount = widget.repeatCount ?? PAGView.REPEAT_COUNT_DEFAULT;
    if (repeatCount <= 0 && repeatCount != PAGView.REPEAT_COUNT_LOOP) {
      repeatCount = PAGView.REPEAT_COUNT_DEFAULT;
    }

    dynamic r =
    await FlutterPagPlugin.getChannel().invokeMethod(
        'initPag',
        {'assetName': widget.assetName, 'url': widget.url, 'repeatCount': widget.repeatCount, 'initProgress': widget.initProgress ?? 0, 'autoPlay': widget.autoPlay});
    _textureId = r['textureId'];
    _rawWidth = r['width'] ?? 0;
    _rawHeight = r['height'] ?? 0;

    setState(() {
      _hasLoadTexture = true;
    });
  }

  void start() {
    FlutterPagPlugin.getChannel().invokeMethod('start', {'textureId': _textureId});
  }

  void stop() {
    FlutterPagPlugin.getChannel().invokeMethod('stop', {'textureId': _textureId});
  }

  void pause() {
    FlutterPagPlugin.getChannel().invokeMethod('pause', {'textureId': _textureId});
  }

  void setProgress(double progress) {
    FlutterPagPlugin.getChannel().invokeMethod('setProgress', {'textureId': _textureId, 'progress': progress});
  }

  Future<List<String>> getLayersUnderPoint(double x, double y) async {
    return (await FlutterPagPlugin.getChannel().invokeMethod('getLayersUnderPoint', {'textureId': _textureId, 'x': x, 'y': y}) as List)
        .map((e) => e.toString())
        .toList();
  }

  @override
  Widget build(BuildContext context) {
    if (_hasLoadTexture) {
      return Container(
        width: widget.width ?? (_rawWidth / 2),
        height: widget.height ?? (_rawHeight / 2),
        child: Texture(textureId: _textureId),
      );
    } else {
      return Container();
    }
  }

  @override
  void dispose() {
    super.dispose();
    FlutterPagPlugin.getChannel().invokeMethod('release', {'textureId': _textureId});
  }
}
