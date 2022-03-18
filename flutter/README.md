# 项目介绍
为Flutter打造的PAG动画组件，以外接纹理的方式实现。

# 快速上手
Flutter侧通过PagView来使用动画

### 引用
```
flutter_pag_plugin:
  hosted:
    name: flutter_pag_plugin
    url: http://pub.oa.com
  version: ^1.0.5
```

### 使用本地资源
```
PagView.asset(
    "assets/xxx.pag", //flutter侧资源路径
    repeatCount: PagView.REPEAT_COUNT_LOOP, // 循环次数
    initProgress: 0.25, // 初始进度
    key: pagKey,  // 利用key进行主动调用
    autoPlay: true, // 是否自动播放
  )
```
### 使用网络资源
```
PagView.url(
    "xxxx", //网络链接
    repeatCount: PagView.REPEAT_COUNT_LOOP, // 循环次数
    initProgress: 0.25, // 初始进度
    key: pagKey,  // 利用key进行主动调用
    autoPlay: true, // 是否自动播放
  )
```
### 通过key获取state进行主动调用
```
  final GlobalKey<PagViewState> pagKey = GlobalKey<PagViewState>();
  
  //传入key值
  PagView.url(key:pagKey）
  
  //播放
  pagKey.currentState?.start();
  
  //暂停
  pagKey.currentState?.pause();  
  
  //停止
  pagKey.currentState?.stop();  
  
  //设置进度
  pagKey.currentState?.setProgress(xxx);
  
  //获取坐标位置的图层名list
  pagKey.currentState?.getLayersUnderPoint(x,y);
```

# 团队介绍
腾讯科技-IEG互动娱乐事业群-用户平台部-研发中心-前端研发组
心悦终端团队
