
import 'dart:async';

import 'package:flutter/services.dart';

class FlutterPagPlugin {
  static const MethodChannel _channel =
      const MethodChannel('flutter_pag_plugin');

  static Future<String?> get platformVersion async {
    final String? version = await _channel.invokeMethod('getPlatformVersion');
    return version;
  }

  static MethodChannel getChannel() => _channel;
}
