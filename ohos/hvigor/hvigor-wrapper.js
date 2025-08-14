/**
 * hvigor-wrapper.js
 * 工程级 hvigor 包装器脚本
 * 由 DevEco Studio 5.0.x 新建项目自动生成
 * 如需自定义缓存目录，仅需修改 hvigorHome / cacheDir 字段
 */
module.exports = {
    hvigorVersion: '1.6.0',
    dependencies: {
        '@ohos/hvigor-ohos-plugin': '1.6.0'
    },

    hvigorHome: process.env.HVIGOR_HOME || path.join(os.homedir(), '.hvigor'),
    cacheDir: process.env.HVIGOR_CACHE_DIR || path.join(process.env.HVIGOR_HOME || path.join(os.homedir(), '.hvigor'), 'cache'),
    enableDaemon: true,
    daemonTimeout: 60 * 60 * 1000,
    logLevel: 'INFO',
    enableIncremental: true,
    enableParallel: true,
    enableTypeCheck: false
};
