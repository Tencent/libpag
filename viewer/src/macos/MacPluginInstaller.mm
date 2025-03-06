#import "MacPluginInstaller.h"
#import <inttypes.h>
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import "utils/Translate.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#define AE_LIB_PATH "/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/"
#define AE_CEP_PATH "/Library/Application Support/Adobe/CEP/extensions/"

#define VERSION_SEPARATE(version) ((version>>48)&0xFFFF), ((version>>32)&0xFFFF), ((version>>16)&0xFFFF), ((version>>0)&0xFFFF)

// TODO 完善代码

auto GetRoamingPath() -> std::string {
  NSArray* arr = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
  NSString* path = [[NSString alloc] initWithFormat:@"%@/", arr[0]];
  auto ret = std::string([path UTF8String]);
  [path release];
  return ret;
}

auto GetH264EncoderToolsExePath() -> std::string {
  NSString* path = [[NSBundle mainBundle] pathForResource:@"H264EncoderTools" ofType:@""];
  if (path != nil) {
    auto str = [path UTF8String];
    return str;
  } else {
    return GetRoamingPath() + "H264EncoderTools/H264EncoderTools";
  }
}

auto isExistAE() -> bool {
    NSString* aePath = @AE_LIB_PATH;
    BOOL isDirectory = NO;
    BOOL isExist = [[NSFileManager defaultManager] fileExistsAtPath:aePath isDirectory:&isDirectory];
    if (isExist && isDirectory) {
        return true;
    } else {
        return false;
    }
}

static auto GetBuildVersion(NSString* plistPath) -> int64_t {
    int64_t build = 0;
    NSMutableDictionary* dict = [[[NSMutableDictionary alloc] initWithContentsOfFile:plistPath] autorelease];
    if (dict == nil) {
        return build;
    }

    if (@available(macOS 10.15, *)) {
        NSString *buildString = dict[@"CFBundleVersion"];

        if (buildString != nil) {
            int64_t b0 = 0;
            int64_t b1 = 0;
            int64_t b2 = 0;
            int64_t b3 = 0;
            int status = sscanf([buildString UTF8String], "%" SCNd64 ".%" SCNd64 ".%" SCNd64 ".%" SCNd64, &b0, &b1, &b2, &b3);
            if (status >= 4) {
              NSLog(@"get 4 parts version");
            } else if (status >= 3) {
              NSLog(@"get 3 parts version");
            }
            build = (b0<<48) | (b1<<32) | (b2<<16) | (b3 << 0);
        }
    } else {
        NSLog(@"macOS < 10.15");
    }

    return build;
}

static auto PagGetAeBuildVersion(NSString* pluginName) -> int64_t {
    NSString * plistPath = [[[NSString alloc] initWithFormat:@"%s/%@.plugin/Contents/Info.plist", AE_LIB_PATH, pluginName] autorelease];
    int64_t build = GetBuildVersion(plistPath);
    return build;
}

static auto PagGetSoBuildVersion(NSString* pluginName) -> int64_t {
    NSString* resourcePath = [[NSBundle mainBundle] resourcePath];
    NSString * plistPath = [[[NSString alloc] initWithFormat:@"%@/%@.plugin/Contents/Info.plist", resourcePath, pluginName] autorelease];
    int64_t build = GetBuildVersion(plistPath);
    return build;
}

static auto ExcuteAppleScript(char* cmd) -> int {
    int ret = 0;

    NSString* fullScript = [NSString stringWithUTF8String:cmd];
    NSDictionary *errorInfo = [NSDictionary new];
    NSString *script =  [NSString stringWithFormat:@"do shell script \"%@\" with administrator privileges", fullScript];

    NSLog(@"fullScript=%@", fullScript);

    NSAppleScript *appleScript = [[[NSAppleScript new] initWithSource:script] autorelease];
    NSAppleEventDescriptor * eventResult = [appleScript executeAndReturnError:&errorInfo];

    if (!eventResult)
    {
        NSLog(@"errorInfo.count = %lu", errorInfo.count);

        NSArray *keys = [errorInfo allKeys];
        float length = [keys count];
        for (int i = 0; i < length; i ++) {
            id key = [keys objectAtIndex:i];
            id obj = [errorInfo objectForKey:key];
            NSLog(@"errorInfo[%d]: %@ -> %@", i, key, obj);
        }

        return -5;
    }
    return ret;
}

static auto PagPluginCopyByCmd(NSString* pluginName) -> int {
    int ret = 0;
    NSString* srcPath = [[NSBundle mainBundle] pathForResource:pluginName ofType:@"plugin"];
    if (srcPath == nil) {
        return -1;
    }

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cp -r -f \'%s\' \'%s\'", [srcPath UTF8String], AE_LIB_PATH);
    ret = system(cmd);

    if (ret == 256) {
        ret = ExcuteAppleScript(cmd);
    }

    return ret;
}

static auto isXcodeDebug() -> bool {
    NSString* resourcePath = [[NSBundle mainBundle] resourcePath];
    if (([resourcePath rangeOfString:@"Debug"].length > 0) || ([resourcePath rangeOfString:@"debug"].length > 0)
        || ([resourcePath rangeOfString:@"Xcode"].length > 0) || ([resourcePath rangeOfString:@"xcode"].length > 0)) {
        return true;
    } else {
        return false;
    }
}

static auto HasNewVersion(NSString* pluginName) -> bool {
    int64_t srcBuildVersion = PagGetSoBuildVersion(pluginName);
    int64_t dstBuildVersion = PagGetAeBuildVersion(pluginName);
    if (srcBuildVersion <= dstBuildVersion) {
        return false;
    } else {
        return true;
    }
}


/*
 * 获取用户已经安装的 Adobe After Effects 的路径，可能存在多个版本
 * @return
 */
static auto GetAEAppsPath() -> NSMutableArray* {
  NSMutableArray *aeAppPathArr = [NSMutableArray array];
  NSFileManager *myFileManager = [NSFileManager defaultManager];

  BOOL isDir = NO;
  BOOL isExist = NO;

  for (int aeVersion = 2017; aeVersion <= 2030; aeVersion++) {
    NSString *path = [NSString stringWithFormat:@"/Applications/Adobe After Effects %d/Adobe After Effects %d.app", aeVersion, aeVersion];
    isExist = [myFileManager fileExistsAtPath:path isDirectory:&isDir];
    NSLog(@"aeAppPth:%@ isExist:%d, isDir:%d", path, isExist, isDir);
    if (isExist && isDir) {
      NSLog(@"aeAppPth:%@", path);
      [aeAppPathArr addObject:path];
    }
  }
  NSLog(@"aeAppPathArr count:%lu", static_cast<unsigned long>(aeAppPathArr.count));
  return aeAppPathArr;
}

/*
 * 删除AE APP 里面 QT 依赖文件
 */
static auto DeleteOldQtResourceInAE(char cmd[], int cmdSize) -> void {
  NSString* deleteShellPath = [[NSBundle mainBundle] pathForResource:@"delete_old_qt_resource" ofType:@"sh"];
  snprintf(cmd + strlen(cmd), cmdSize - strlen(cmd), "sh %s\n", [deleteShellPath UTF8String]);
}

/*
 * 拷贝 PAGExporter.plugin 依赖的 QT 资源到 AE APP
 */
static auto CopyQtResourceInAE(char cmd[], int cmdSize) -> auto {
  NSString* copyQtShellPath = [[NSBundle mainBundle] pathForResource:@"copy_qt_resource" ofType:@"sh"];
  snprintf(cmd + strlen(cmd), cmdSize - strlen(cmd), "sh %s\n", [copyQtShellPath UTF8String]);
}

static auto DeletePlugin(char cmd[], int cmdSize, const char* pluginName, const char* path = AE_LIB_PATH) -> void {
    snprintf(cmd + strlen(cmd), cmdSize - strlen(cmd), "if [ -d \'%s%s\' ]; then \n", path, pluginName);
    snprintf(cmd + strlen(cmd), cmdSize - strlen(cmd), "    rm -r -f \'%s%s\' \n", path, pluginName);
    snprintf(cmd + strlen(cmd), cmdSize - strlen(cmd), "fi \n");
}

static int CopyPlugins(bool pagHasNewVersion) {
    int ret = 0;
    int num = 0;

    char cmd[8192] = {0};

    if (pagHasNewVersion) {
        snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "if ! [ -d \'%s\' ]; then \n", AE_LIB_PATH);
        snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "    mkdir -p \'%s\' \n", AE_LIB_PATH);
        snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "fi \n");
    }

    if (pagHasNewVersion) {
        NSString* pagPath = [[NSBundle mainBundle] pathForResource:@"PAGExporter" ofType:@"plugin"];
        if (pagPath != nil) {
            DeleteOldQtResourceInAE(cmd, sizeof(cmd));

            DeletePlugin(cmd, sizeof(cmd), "PAGExporter.plugin");
            snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "cp -r -f \'%s\' \'%s\' \n", [pagPath UTF8String], AE_LIB_PATH);
            num++;
        }
    }

    if (pagHasNewVersion) {
        NSString* pagPath = [[NSBundle mainBundle] pathForResource:@"ImageFillRule" ofType:@"plugin"];
        if (pagPath != nil) {
            DeletePlugin(cmd, sizeof(cmd), "ImageFillRule.plugin");
            snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "cp -r -f \'%s\' \'%s\' \n", [pagPath UTF8String], AE_LIB_PATH);
            num++;
        }
    }

    if (pagHasNewVersion) {
        NSString* cepPath = [[NSBundle mainBundle] pathForResource:@"com.tencent.pagconfig" ofType:@""];
        if (cepPath != nil) {
            DeletePlugin(cmd, sizeof(cmd), "com.tencent.pagconfig", AE_CEP_PATH);
            snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "cp -r -f \'%s\' \'%s\' \n", [cepPath UTF8String], AE_CEP_PATH);
            num++;
        }
    }

    if (pagHasNewVersion) {
        NSString* pagPath = [[NSBundle mainBundle] pathForResource:@"TextBackground" ofType:@"plugin"];
        if (pagPath != nil) {
            DeletePlugin(cmd, sizeof(cmd), "TextBackground.plugin");
            snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "cp -r -f \'%s\' \'%s\' \n", [pagPath UTF8String], AE_LIB_PATH);
            num++;
        }
    }

    {
      NSString* toolsPath = [[NSBundle mainBundle] pathForResource:@"H264EncoderTools" ofType:@""];
      auto dstPath = GetRoamingPath() + "H264EncoderTools/";
      if (toolsPath != nil) {
        char dircmd[2 * 1024] = {0};
        snprintf(dircmd + strlen(dircmd), sizeof(dircmd) - strlen(dircmd), "if ! [ -d \'%s\' ]; then \n", dstPath.c_str());
        snprintf(dircmd + strlen(dircmd), sizeof(dircmd) - strlen(dircmd), "    mkdir -p \'%s\' \n", dstPath.c_str());
        snprintf(dircmd + strlen(dircmd), sizeof(dircmd) - strlen(dircmd), "fi \n");
        system(dircmd);

        snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "cp -r -f \'%s\' \'%s\' \n", [toolsPath UTF8String], dstPath.c_str());
        num++;
      }
    }

    if (num > 0) {
        ret = ExcuteAppleScript(cmd);
    }

    return ret;
}


static auto isAeOpened() -> bool {
    int fd[2];
    int backfd;
    char buf[1024] = {0};

    pipe(fd);
    backfd = dup(STDOUT_FILENO);
    dup2(fd[1], STDOUT_FILENO);

    int ret = system("ps -ef |grep \'.app/Contents/MacOS/After Effects\' |grep -v \"grep\" |grep -v \"launchedbyvulcan\" |wc -l");

    read(fd[0], buf, 1024);
    dup2(backfd, STDOUT_FILENO);

    if (ret == 0) {
        int count = 0;
        sscanf(buf, "%d", &count);
        NSLog(@"isAeOpened count: %d", count);
        if (count > 0) {
            return true;
        }
    }

    return false;
}

auto MacPluginInstaller::copyFileByCmd(char* originPath, char* targetPath) -> int {
    int ret = 0;
    char cmd[1024];

    snprintf(cmd, sizeof(cmd), "cp -r -f \'%s\' \'%s\'", originPath, targetPath);
    ret = system(cmd);

    if (ret == 256) {
        ret = ExcuteAppleScript(cmd);
    }

    return ret;
}

auto MacPluginInstaller::InstallPlugin(std::string pluginName) -> int {
    int ret = 0;

    if (isXcodeDebug()) {
        NSLog(@"Xcode Debug 状态下不更新AE插件！");
        return 0;
    }

    NSString* name = [NSString stringWithUTF8String:pluginName.c_str()];

    int64_t srcBuildVersion = PagGetSoBuildVersion(name);
    int64_t dstBuildVersion = PagGetAeBuildVersion(name);
    if (srcBuildVersion <= dstBuildVersion) {
        NSLog(@"Adobe After Effects 插件(%@)已是最新(%lld.%lld.%lld.%lld >= %lld.%lld.%lld.%lld)，无需更新 ！",
            name, VERSION_SEPARATE(dstBuildVersion), VERSION_SEPARATE(srcBuildVersion));
        return 0;
    }

    ret = PagPluginCopyByCmd(name);

    if (ret == 0) {
        NSAlert *alert = [NSAlert alertWithMessageText:@"提示:"
                                  defaultButton:@"OK"
                                  alternateButton:nil
                                  otherButton:nil
                                  informativeTextWithFormat:@"Adobe After Effects 插件(%@)已更新至%lld.%lld.%lld.%lld！",
                                      name, VERSION_SEPARATE(srcBuildVersion)];
        [alert runModal];
    } else {
        NSAlert *alert = [NSAlert alertWithMessageText:@"警告:"
                                  defaultButton:@"OK"
                                  alternateButton:nil
                                  otherButton:nil
                                  informativeTextWithFormat:@"Adobe After Effects 插件(%@)更新(%lld.%lld.%lld.%lld->%lld.%lld.%lld.%lld)失败(error code:%d)，请联系开发者 ！",
                                      name, VERSION_SEPARATE(dstBuildVersion), VERSION_SEPARATE(srcBuildVersion), ret];
        [alert runModal];
    }

    return ret;
}

auto MacPluginInstaller::InstallPlugins(bool bForceInstall) -> int {
    int ret = 0;
    bool bCopyPagPlugin = false;

    if (bForceInstall) {
        bCopyPagPlugin = true;
    } else {
        bCopyPagPlugin = HasNewVersion(@"PAGExporter");

        if (!bCopyPagPlugin) {
            NSAlert *alert = [[NSAlert new] autorelease];
            [alert addButtonWithTitle:@(Utils::Translate("确定").c_str())];
            [alert setMessageText:@(Utils::Translate("当前 AE 插件并无更新").c_str())];
            [alert runModal];
            return 0;
        }

        NSAlert *alert = [[NSAlert new] autorelease];
        [alert addButtonWithTitle:@(Utils::Translate("更新").c_str())];
        [alert addButtonWithTitle:@(Utils::Translate("取消").c_str())];
        [alert setMessageText:@(Utils::Translate("更新 Adobe After Effects 插件").c_str())];
        [alert setInformativeText:@(Utils::Translate("发现新版本 Adobe After Effects 插件，是否更新？").c_str())];
        [alert setAlertStyle:NSWarningAlertStyle];
        NSModalResponse returnCode = [alert runModal];
        if (returnCode == NSAlertSecondButtonReturn) {
            return 0;
        }
    }

    while (isAeOpened()) {
        NSAlert *alert = [[NSAlert new] autorelease];
        [alert addButtonWithTitle:@(Utils::Translate("重试").c_str())];
        [alert addButtonWithTitle:@(Utils::Translate("取消").c_str())];
        [alert setMessageText:@(Utils::Translate("请关闭 Adobe After Effects").c_str())];
        [alert setInformativeText:@(Utils::Translate("检测到 Adobe After Effects 已经打开，请关闭软件后重试。").c_str())];
        [alert setAlertStyle:NSWarningAlertStyle];
        NSModalResponse returnCode = [alert runModal];
        if (returnCode == NSAlertSecondButtonReturn) {
            return 0;
        }
    }

    ret = CopyPlugins(bCopyPagPlugin);

    if (ret == 0) {
        NSAlert *alert = [[NSAlert new] autorelease];
        [alert addButtonWithTitle:@(Utils::Translate("确定").c_str())];
        [alert setMessageText:@(Utils::Translate("Adobe After Effects 插件安装成功！").c_str())];
        [alert runModal];
    } else {
        NSAlert *alert = [[NSAlert new] autorelease];
        [alert addButtonWithTitle:@(Utils::Translate("确定").c_str())];
        [alert setMessageText:@(Utils::Translate("警告").c_str())];
        [alert setInformativeText:@(Utils::Translate("Adobe After Effects 插件安装失败(error code: %1)，请联系开发者！", std::to_string(ret).c_str()).c_str())];
        [alert runModal];
    }

    return ret;
}

auto MacPluginInstaller::UninstallPlugins() -> int {
  while (isAeOpened()) {
    NSAlert *alert = [[NSAlert new] autorelease];
    [alert addButtonWithTitle:@(Utils::Translate("重试").c_str())];
    [alert addButtonWithTitle:@(Utils::Translate("取消").c_str())];
    [alert setMessageText:@(Utils::Translate("请关闭 Adobe After Effects").c_str())];
    [alert setInformativeText:@(Utils::Translate("检测到 Adobe After Effects 已经打开，请关闭软件后重试。").c_str())];
    [alert setAlertStyle:NSWarningAlertStyle];
    NSModalResponse returnCode = [alert runModal];
    if (returnCode == NSAlertSecondButtonReturn) {
      return 0;
    }
  }

  char cmd[8192] = {0};
  DeleteOldQtResourceInAE(cmd, sizeof(cmd));
  DeletePlugin(cmd, sizeof(cmd), "PAGExporter.plugin");
  DeletePlugin(cmd, sizeof(cmd), "ImageFillRule.plugin");
  DeletePlugin(cmd, sizeof(cmd), "TextBackground.plugin");
  DeletePlugin(cmd, sizeof(cmd), "com.tencent.pagconfig", AE_CEP_PATH);

  int ret = ExcuteAppleScript(cmd);

  if (ret == 0) {
    NSAlert *alert = [[NSAlert new] autorelease];
    [alert addButtonWithTitle:@(Utils::Translate("确定").c_str())];
    [alert setMessageText:@(Utils::Translate("导出插件删除成功！").c_str())];
    [alert runModal];
  } else {
    NSAlert *alert = [[NSAlert new] autorelease];
    [alert addButtonWithTitle:@(Utils::Translate("确定").c_str())];
    [alert setMessageText:@(Utils::Translate("警告").c_str())];
    [alert setInformativeText:@(Utils::Translate("导出插件删除失败，请联系开发者！").c_str())];
    [alert runModal];
  }

  return ret;
}

auto MacPluginInstaller::HasUpdate() -> bool {
    return HasNewVersion(@"PAGExporter");
}

#pragma clang diagnostic pop
