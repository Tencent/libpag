/**
 * hvigor-wrapper.js
 * 工程级 hvigor 包装器脚本
 * 由 DevEco Studio 5.0.x 新建项目自动生成
 * 如需自定义缓存目录，仅需修改 hvigorHome / cacheDir 字段
 */
module.exports = {
    /* 构建工具及插件版本 */
    hvigorVersion: '2.4.2',
    dependencies: {
      '@ohos/hvigor-ohos-plugin': '2.4.2'
    },
  
    /* 构建运行时配置 */
    hvigorHome: '/Users/runner/.hvigor',   // CI 里改成可写路径
    cacheDir: '/Users/runner/.hvigor/cache',
    enableDaemon: true,
    daemonTimeout: 60 * 60 * 1000, // 1h
    logLevel: 'INFO',
    enableIncremental: true,
    enableParallel: true,
    enableTypeCheck: false
  };
  