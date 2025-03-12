#include "PAGPluginInstaller.h"
#include <filesystem>
#include <io.h>
#include <tchar.h>
#include <direct.h>
#include <shlobj.h>
#include <windows.h>
#include <QString>
#include <cJSON.h>
#include "common/version.h"
#include "utils/File.h"
#include "utils/Translate.h"

#define MAX_PATH_SIZE 1024
#define AE_LIB_PATH "C:\\PROGRA~1\\Adobe\\Common\\Plug-ins\\7.0\\MediaCore\\"

#if defined(WIN32)
// TODO delete later
#pragma warning(push)
#pragma warning(disable: 4996) 
#pragma warning(disable: 4267)
#endif


static LPCWSTR StringToLPCWSTR(std::string str) {
  int len = str.length();
  int lenbf = MultiByteToWideChar(CP_ACP, 0, str.c_str(), len, 0, 0);
  wchar_t* buffer = new wchar_t[lenbf+1];
  MultiByteToWideChar(CP_ACP, 0, str.c_str(), len, buffer, sizeof(wchar_t) * lenbf);
  buffer[lenbf] = 0;
  return buffer;
}

static std::string FormatStdString(std::string str) {
  return QString::fromStdString(str).toLocal8Bit().toStdString();
}

#define _TT(a) StringToLPCWSTR(FormatStdString(a))

static bool IsExistPath(const char* path) {
  if (0 == access(path, 0)) {
    return true;
  }
  return false;
}

static bool CheckFileExist(const char* path, const char* fileName) {
  char filePath[MAX_PATH_SIZE];
  snprintf(filePath, sizeof(filePath), "%s\\%s", path, fileName);
  return IsExistPath(filePath);
}

static void GetCurrentPath(char dstPath[], int size, bool onlyPath) {
    TCHAR szFilePath[MAX_PATH + 1] = { 0 };
    GetModuleFileName(NULL, szFilePath, MAX_PATH);
    if (onlyPath) {
      (_tcsrchr(szFilePath, _T('\\')))[0] = 0;
    }

    WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<LPCWCH>(szFilePath), -1, dstPath, size, "\0", 0);
}

static bool GetRoamingPath(char dstPath[], int size) {
    char* roamingPath = nullptr;
    size_t len;
    errno_t err = _dupenv_s(&roamingPath, &len, "APPDATA");
    if (err || roamingPath == nullptr) {
        return false;
    }

    snprintf(dstPath, size, "%s", roamingPath);

    free(roamingPath);
    return true;
}

namespace pag {
  std::string GetRoamingPath() {
    char* roamingPath = nullptr;
    size_t len;
    errno_t err = _dupenv_s(&roamingPath, &len, "APPDATA");
    if (err || roamingPath == nullptr) {
      return "";
    }
    std::string ret = roamingPath;
    free(roamingPath);
    return ret + "\\";
  }

  std::string GetH264EncoderToolsExePath() {
    char currentPath[MAX_PATH_SIZE];
    GetCurrentPath(currentPath, sizeof(currentPath), true);
    if (CheckFileExist(currentPath, "H264EncoderTools.exe")) {
      std::string ret = currentPath;
      return ret + "\\H264EncoderTools.exe";
    }
    else {
      return GetRoamingPath() + "H264EncoderTools\\H264EncoderTools.exe";
    }
  }
}

static bool GetCEPPath(char dstPath[], int size) {
  bool flag = GetRoamingPath(dstPath, size);
  if (!flag) {
    return false;
  }
  snprintf(dstPath, size, "%s\\Adobe\\CEP\\extensions\\com.tencent.pagconfig", dstPath);
  if (!IsExistPath(dstPath)) {
    mkdir(dstPath);
  }
  return true;
}

//
// 从注册表中获取AE的安装目录
//
static bool GetAEInstallPath(char installPath[], int size)
{
    HKEY hKEY = nullptr;
    LPCTSTR key = _T("SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\App Paths\\AfterFX.exe");//主键值
    DWORD dataType;//数据类型
    DWORD dataSize;//数据长度
    TCHAR data[MAX_PATH_SIZE] = { 0 };

    auto ret0 = RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, NULL, KEY_READ, &hKEY);//打开主键
    if (ret0 != ERROR_SUCCESS)  // 如果无法打开hKEY,则中止程序的执行  
    {
        return false;
    }

    auto ret1 = RegQueryValueEx(hKEY, _T("Path"), NULL, &dataType, (LPBYTE)data, &dataSize); //获取数据
    RegCloseKey(hKEY); // 程序结束前要关闭已经打开的 hKEY

    if (ret1 != ERROR_SUCCESS) // 如果无法打开hKEY,则中止程序的执行  
    {
        return false;
    }

    WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<LPCWCH>(data), -1, installPath, size, "\0", 0);

    return true;
}

// 增加一条路径（排重）
static int AddAPath(char paths[][MAX_PATH_SIZE], int count, int maxPathCount, const char* path) {
    if (count >= maxPathCount) {
        return maxPathCount;
    }

    // 去掉最后一个'\'
    char newPath[MAX_PATH_SIZE] = { 0 };
    strncpy(newPath, path, MAX_PATH_SIZE);
    auto len = strlen(newPath);
    if (len > 0 && newPath[len - 1] == '\\') {
        newPath[len - 1] = '\0';
    }

    // 排重
    for (int i = 0; i < count; i++) {
        if (0 == strcmp(paths[i], newPath)) {
            return count; // 遇到相同的则结束
        }
    }

    // 增加一条(后面count+1了才算真正有效增加一条，现在只是暂存)
    strncpy(paths[count], newPath, MAX_PATH_SIZE);

    // 判断该路径是否存在
    strcat(newPath, "\\AfterFX.exe");
    if (0 != access(newPath, 0)) {
        return count;
    }

    return count + 1;
}

static int FindStringPos(const char* str, const char* subStr) {
    int len1 = strlen(str);
    int len2 = strlen(subStr);

    for (int j = 0; j < len1 - len2; j++) {
        int i;
        for (i = 0; i < len2; i++) {
            if (str[j + i] != subStr[i]) {
                break;
            }
        }
        if (i == len2) {
            return j;
        }
    }

    return -1;
}

// 增加N条路径（将1条扩展N条：2017、2018、2019、2020）
static int AddNPaths(char paths[][MAX_PATH_SIZE], int count, int maxPathCount, const char* path) {
  if (path == nullptr) {
    return count;
  }

  count = AddAPath(paths, count, maxPathCount, path);

  for (int year = 2017; year < 2100; year++) {
    const char* prefixStr[] = {
        "Adobe After Effects CC",
        "Adobe After Effects",
    };
    for (int k = 0; k < sizeof(prefixStr) / sizeof(prefixStr[0]); k++) {
      char subStr[MAX_PATH_SIZE];
      snprintf(subStr, MAX_PATH_SIZE, "%s %d", prefixStr[k], year);
      auto pos = FindStringPos(path, subStr);
      if (pos >= 0) {
        char newPath[MAX_PATH_SIZE] = { 0 };
        strncpy(newPath, path, MAX_PATH_SIZE);
        for (int y = 2017; y < 2100; y++) {
          snprintf(newPath + pos, MAX_PATH_SIZE - pos, "%s %d%s",
            prefixStr[0], y, path + pos + strlen(subStr));
          count = AddAPath(paths, count, maxPathCount, newPath);

          snprintf(newPath + pos, MAX_PATH_SIZE - pos, "%s %d%s",
            prefixStr[1], y, path + pos + strlen(subStr));
          count = AddAPath(paths, count, maxPathCount, newPath);
        }
        return count;
      }
    }
  }

  return count;
}

static int AddSlectedPath(char paths[][MAX_PATH_SIZE], int count, int maxPathCount, TCHAR* szBuffer) {
    char path[MAX_PATH_SIZE] = { 0 };

    WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<LPCWCH>(szBuffer), -1, path, MAX_PATH_SIZE, "\0", 0);

    count = AddNPaths(paths, count, maxPathCount, path);

    if (count <= 0) {
        char newPath[MAX_PATH_SIZE] = { 0 };
        snprintf(newPath, sizeof(newPath), "%s%s", path, "\\Adobe\\Adobe After Effects CC 2018\\Support Files");
        count = AddNPaths(paths, count, maxPathCount, newPath);
        snprintf(newPath, sizeof(newPath), "%s%s", path, "\\Adobe After Effects CC 2018\\Support Files");
        count = AddNPaths(paths, count, maxPathCount, newPath);
        snprintf(newPath, sizeof(newPath), "%s%s", path, "\\Support Files");
        count = AddNPaths(paths, count, maxPathCount, newPath);
    }

    return count;
}

static int UserSlectFilePath(char paths[][MAX_PATH_SIZE], int count, int maxPathCount) {

    do {

        // 第1步：用户选择AE安装目录
        TCHAR szBuffer[MAX_PATH] = { 0 };
        BROWSEINFO bi;
        ZeroMemory(&bi, sizeof(BROWSEINFO));
        bi.hwndOwner = NULL;
        bi.pszDisplayName = szBuffer;
        bi.lpszTitle = _TT(Utils::translate("选择AE安装目录（AfterFX.exe所在目录）:").c_str());
        bi.ulFlags = BIF_RETURNFSANCESTORS;
        LPITEMIDLIST idl = SHBrowseForFolder(&bi);
        if (NULL == idl)
        {
            return 0;
        }
        SHGetPathFromIDList(idl, szBuffer);

        // 第2步：加入选择的目录，并检测是否正确
        count = AddSlectedPath(paths, count, maxPathCount, szBuffer);
        if (count > 0) {
            break;
        }

        // 第3步：如果选择目录不正确，则弹窗提示是否重试
        auto ret = MessageBox(nullptr, _TT(Utils::translate("该目录下未发现AE，请重新选择！").c_str()), _TT(Utils::translate("AE安装目录选择错误 ").c_str()), MB_RETRYCANCEL);
        if (ret != IDRETRY) {
            break;
        }
    } while (1);

    return count;
}

// 预设AE常见的默认安装目录
static int AddAeDefaltPaths(char paths[][MAX_PATH_SIZE], int count, int maxPathCount) {

    count = AddNPaths(paths, count, maxPathCount, "C:\\Program Files\\Adobe\\Adobe After Effects CC 2018\\Support Files");
    count = AddNPaths(paths, count, maxPathCount, "D:\\Program Files\\Adobe\\Adobe After Effects CC 2018\\Support Files");
    count = AddNPaths(paths, count, maxPathCount, "E:\\Program Files\\Adobe\\Adobe After Effects CC 2018\\Support Files");
    count = AddNPaths(paths, count, maxPathCount, "F:\\Program Files\\Adobe\\Adobe After Effects CC 2018\\Support Files");

    count = AddNPaths(paths, count, maxPathCount, "C:\\Program Files (x86)\\Adobe\\Adobe After Effects CC 2018\\Support Files");
    count = AddNPaths(paths, count, maxPathCount, "D:\\Program Files (x86)\\Adobe\\Adobe After Effects CC 2018\\Support Files");
    count = AddNPaths(paths, count, maxPathCount, "E:\\Program Files (x86)\\Adobe\\Adobe After Effects CC 2018\\Support Files");
    count = AddNPaths(paths, count, maxPathCount, "F:\\Program Files (x86)\\Adobe\\Adobe After Effects CC 2018\\Support Files");

    count = AddNPaths(paths, count, maxPathCount, "C:\\Adobe\\Adobe After Effects CC 2018\\Support Files");
    count = AddNPaths(paths, count, maxPathCount, "D:\\Adobe\\Adobe After Effects CC 2018\\Support Files");
    count = AddNPaths(paths, count, maxPathCount, "E:\\Adobe\\Adobe After Effects CC 2018\\Support Files");
    count = AddNPaths(paths, count, maxPathCount, "F:\\Adobe\\Adobe After Effects CC 2018\\Support Files");

    count = AddNPaths(paths, count, maxPathCount, "C:\\Adobe After Effects CC 2018\\Support Files");
    count = AddNPaths(paths, count, maxPathCount, "D:\\Adobe After Effects CC 2018\\Support Files");
    count = AddNPaths(paths, count, maxPathCount, "E:\\Adobe After Effects CC 2018\\Support Files");
    count = AddNPaths(paths, count, maxPathCount, "F:\\Adobe After Effects CC 2018\\Support Files");

    return count;
}

//
// 从注册表中获取AE的安装目录列表(多个AE版本)
//
static int GetAEInstallPaths(char installPaths[][MAX_PATH_SIZE], int maxPathCount){
    int count = 0;

    // 第1步：预设AE常见的默认安装目录
    count = AddAeDefaltPaths(installPaths, count, maxPathCount);

    //
    // 第2步：从"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\App Paths\\AfterFX.exe"里获取AE安装路径
    //
    char path[MAX_PATH_SIZE] = { 0 };
    if (GetAEInstallPath(path, MAX_PATH_SIZE)) {
        count = AddNPaths(installPaths, count, maxPathCount, path);
    }

    //
    // 第3步：从"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"里获取多个AE版本的安装路径
    //
    HKEY hRootKey = nullptr;
    LPCTSTR rootKeyName = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"); //主键值

    // 打开主键
    if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, rootKeyName, NULL, KEY_READ, &hRootKey)) {
        return count; // 如果无法打开hKEY,则中止程序的执行  
    }

    for (DWORD index = 0; true; index++) {
        TCHAR subKeyName[256] = { 0 };
        DWORD dwKeySize = 256;
        if (ERROR_SUCCESS != RegEnumKeyEx(hRootKey, index, subKeyName, &dwKeySize, 0, NULL, NULL, NULL)) {
            break;
        }

        if (subKeyName[0] == _T('\0')) {
            continue;
        }

        HKEY hSubKey;
        TCHAR newKeyName[MAX_PATH_SIZE] = { 0 };
        lstrcpy(newKeyName, rootKeyName);
        lstrcat(newKeyName, _T("\\"));
        lstrcat(newKeyName, subKeyName);

        if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, newKeyName, 0, KEY_ALL_ACCESS, &hSubKey)) {
            continue;
        }

        DWORD dataType = 0;//数据类型
        DWORD dataSize = 0;//数据长度
        TCHAR data[MAX_PATH_SIZE] = { 0 };
        if (ERROR_SUCCESS == RegQueryValueEx(hSubKey, _T("InstallLocation"), 0, &dataType, (LPBYTE)data, &dataSize)) {

            if (dataType == REG_SZ && dataSize > 16) {
                char path[MAX_PATH_SIZE] = { 0 };
                WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<LPCWCH>(data), -1, path, MAX_PATH_SIZE, "\0", 0);

                if (nullptr != strstr(path, "Adobe After Effects")
                    && nullptr != strstr(path, "\\Support Files")) {
                    strncpy(installPaths[count], path, MAX_PATH_SIZE);
                    count = AddNPaths(installPaths, count, maxPathCount, path); // 增加一条安装路径
                }
            }
        }

        RegCloseKey(hSubKey); // 程序结束前要关闭已经打开的 hKEY
    }

    RegCloseKey(hRootKey); // 程序结束前要关闭已经打开的 hKEY

    //
    // 第4步：用户选择AE安装目录
    //
    if (count <= 0) {
        count = UserSlectFilePath(installPaths, count, maxPathCount);
    }

    return count;
}

static bool IsDebugMode() {
    //
    // 通过可执行文件路径是否包含"debug"和"release"字样来判断是调试还是正式运行
    // 因为安装路径不会包含"debug"和"release"字样
    //
    char currentPath[MAX_PATH_SIZE];
    GetCurrentPath(currentPath, sizeof(currentPath), true);
    if (strstr(currentPath, "debug") != nullptr
        || strstr(currentPath, "Debug") != nullptr
        || strstr(currentPath, "release") != nullptr
        || strstr(currentPath, "Release") != nullptr) {
        return true;
    }
    return false;
}

//
// 提高权限执行cmd命令
//
static int CMDPrivileges(char* cmd) {
    char tmpFilePath[MAX_PATH_SIZE] = {0};

    if (cmd == nullptr || strlen(cmd) <= 0) {
        return 0;
    }

    // 第0步, 准备：获取roaming路径，用于存放临时文件
    auto flag = GetRoamingPath(tmpFilePath, sizeof(tmpFilePath));
    if (!flag) {
        return -1;
    }
    snprintf(tmpFilePath + strlen(tmpFilePath), sizeof(tmpFilePath) - strlen(tmpFilePath), "\\PrivilegesCopyWinTmp.bat");


    // 第1步, 将预设的提权脚本文件临时复制一份
    char str[1024] = {0};
    snprintf(str, sizeof(str), "copy .\\PrivilegesCopyWin.bat \"%s\" /Y \n", tmpFilePath);
    system(str);

    // 第2步, 将要执行的cmd命令追加写入提权脚本文件末尾
    FILE* fp = fopen(tmpFilePath, "at+");
    if (fp != nullptr) {
        fprintf(fp, "\n%s\n", cmd);
        fclose(fp);
    } else {
        return -1;
    }

    // 第3步, 执行提权脚本执行cmd命令
    int ret = system(tmpFilePath);
    return ret;
}

static std::string GetPAGExporterVersion() {
  if (AEPluginVersion == "~PluginVersion~") {
    return std::string("1.0.0"); // 暂时需要手动更新
  } else {
    return AEPluginVersion;
  }
}

static bool GetPluginVersionPath(char pluginVersionPath[], int size) {
  char roamingPath[MAX_PATH_SIZE] = { 0 };
  auto flag = GetRoamingPath(roamingPath, sizeof(roamingPath));
  if (!flag) {
    return false;
  }
  snprintf(pluginVersionPath, size, "%s\\PAGExporter\\PluginVersion.json", roamingPath);
  return true;
}

static void StringReplaceChar(char str[], char oldChar, char newChar) {
  int len = strlen(str);
  for (int i = 0; i < len; i++) {
    if (str[i] == oldChar) {
      str[i] = newChar;
    }
  }
}

static void StorePluginVersion(char aeInstallPaths[][MAX_PATH_SIZE], int countPaths) {
  char pluginVersionPath[MAX_PATH_SIZE] = { 0 };
  auto flag = GetPluginVersionPath(pluginVersionPath, sizeof(pluginVersionPath));
  if (!flag) {
    return;
  }

  auto text = new char[countPaths * (MAX_PATH_SIZE + 64) + 1024];
  text[0] = '\0';

  auto version = GetPAGExporterVersion();

  for (int i = 0; i < countPaths; i++) {
    if (i == 0) {
      sprintf(text + strlen(text), "{");
    }
    sprintf(text + strlen(text), "\n    \"%s\": \"%s\"", aeInstallPaths[i], version.c_str());
    if (i < countPaths - 1) {
      sprintf(text + strlen(text), ",");
    } else {
      sprintf(text + strlen(text), "\n}\n");
    }
  }

  StringReplaceChar(text, '\\', '/');

  FileIO::WriteTextFile(pluginVersionPath, text);
  delete text;
}

static int CopyPlugins() {
    int ret = 0;
    int num = 0;
    char cmd[1024 * 8] = { 0 };
    char h264EncoderToolsPath[MAX_PATH] = {0};

    bool enablePluginTextBackground = true;
    bool enablePluginImageFillRule = true;
    bool enablePluginPagExporter = true;

    char currentPath[MAX_PATH_SIZE] = { 0 };
    GetCurrentPath(currentPath, sizeof(currentPath), true); // 当前路径，也就是PAGViewer的安装路径
    printf("currentPath:%s\n", currentPath);

    // 检测是否有需要安装的AE插件
    std::string qtDllDir = CheckFileExist(currentPath, "QTDLL\\Qt5Core.dll") ? "QTDLL" : "QTdll";
    auto qt5CoreName = (qtDllDir + "\\Qt5Core.dll").c_str();
    bool hasQtResource = CheckFileExist(currentPath, qt5CoreName);
    bool hasPluginTextBackground = CheckFileExist(currentPath, "TextBackground.aex");
    bool hasPluginImageFillRule = CheckFileExist(currentPath, "ImageFillRule.aex");
    bool hasH264EncoderTools = CheckFileExist(currentPath, "H264EncoderTools.exe");
    bool hasExporterAex = CheckFileExist(currentPath, "PAGExporter.aex");
    bool hasPluginPagExporter = hasExporterAex && hasQtResource;
    bool hasPluginCEP = CheckFileExist(currentPath, "com.tencent.pagconfig");
    printf("qt5CoreName:%s\n", qt5CoreName);
    printf(
        "hasQtResource:%d, hasPluginTextBackground:%d, hasPluginImageFillRule:%d, "
        "hasPluginPagExporter:%d, hasPluginCEP:%d, hasExporterAex:%d\n",
        hasQtResource, hasPluginTextBackground, hasPluginImageFillRule, hasPluginPagExporter,
        hasPluginCEP, hasExporterAex);

    // 获取AE的安装目录（因为可能安装了多个AE版本，所以可能有多个目录）
#define MAX_PATH_COUNT 32
    char aeInstallPaths[MAX_PATH_COUNT][MAX_PATH_SIZE];
    auto countPaths = 0;
    if (enablePluginPagExporter && hasPluginPagExporter) {
        countPaths = GetAEInstallPaths(aeInstallPaths, MAX_PATH_COUNT);
    }
    char cepPath[MAX_PATH_SIZE] = { 0 };

    bool hasAeCommonPath = (0 == access(AE_LIB_PATH, 0)); // 是否有AE Common目录
    bool hasAeInstallPath = (countPaths > 0); // 是否有AE 安装目录
    bool hasCEPPath = hasPluginCEP && GetCEPPath(cepPath, sizeof(cepPath));

    char logPath[MAX_PATH_SIZE] = "installAEPluginLog.txt";
    char roamingPath[MAX_PATH_SIZE] = {0};
    auto flag = GetRoamingPath(roamingPath, sizeof(roamingPath));
    if (flag) {
      snprintf(logPath, sizeof(logPath), "%s\\installAEPluginLog.txt", roamingPath);
    }

    // 安装效果插件TextBackground
    if (enablePluginTextBackground && hasAeCommonPath && hasPluginTextBackground) {
        num++;
      snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "copy /Y \"%s\\%s\" \"%s\" > %s\n",
          currentPath, "TextBackground.aex", AE_LIB_PATH, logPath);
    }

    // 安装效果插件ImageFillRule
    if (enablePluginImageFillRule && hasAeCommonPath && hasPluginImageFillRule) {
        num++;
        snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "copy /Y \"%s\\%s\" \"%s\" > %s\n",
                 currentPath, "ImageFillRule.aex", AE_LIB_PATH, logPath);
    }

    // 安装导出插件PAGExporter
    if (enablePluginPagExporter && hasAeInstallPath && hasPluginPagExporter) {
        num++;
        for (int index = 0; index < countPaths; index++) {
            // 拷贝导出插件到AE安装目录的Plg-ins子目录下
            snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "copy /Y \"%s\\%s\" \"%s\\Plug-ins\\\" > %s\n",
                currentPath, "PAGExporter.aex", aeInstallPaths[index], logPath);

            // 拷贝QT dll到AE安装目录
            snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "xcopy /Y /E /R \"%s\\%s\" \"%s\" > %s\n",
                currentPath, qtDllDir.c_str(), aeInstallPaths[index], logPath);
        }
    }

    if (hasPluginCEP && hasCEPPath) {
        num++;
        snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "xcopy /Y /E /R \"%s\\%s\\\" \"%s\" > %s\n",
            currentPath, "com.tencent.pagconfig", cepPath, logPath);
    }

    if (hasH264EncoderTools) {
      auto dstPath = pag::GetRoamingPath() + "H264EncoderTools\\";
      if (access(dstPath.c_str(), 0) != 0) {
        _mkdir(dstPath.c_str());
      }

      num++;
      snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "copy /Y \"%s\\%s\" \"%s\" > %s\n",
        currentPath, "H264EncoderTools.exe", dstPath.c_str(), logPath);
      snprintf(h264EncoderToolsPath,sizeof(h264EncoderToolsPath),"%sH264EncoderTools.exe",dstPath.c_str());
    }

    // 如果有AE插件需要安装的话，执行cmd操作
    // 否则不需要执行
    if (num > 0) {
        //snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "\n pause \n");
        ret = CMDPrivileges(cmd);
    }

    bool hasError = false; // 避免多次重复弹窗，所以设立一个标记，如果已经弹过，则不再重复

    if (enablePluginPagExporter) {
        if (!hasPluginPagExporter) {
            hasError = true;
            MessageBox(nullptr, _TT(Utils::translate("安装失败：没有找到AE导出插件: PAGExporter.aex 和 QTdll 。(论坛提问：https://bbs.pag.art/) ").c_str()), _TT(Utils::translate("AE插件安装失败").c_str()), MB_OK);
        } else if (!hasAeInstallPath) {
            hasError = true;
            MessageBox(nullptr, _TT(Utils::translate("安装失败：没有找到AE安装目录。(论坛提问：https://bbs.pag.art/) ").c_str()), _TT(Utils::translate("AE插件安装失败").c_str()), MB_OK);
        } else if(!hasH264EncoderTools) {
          hasError = true;
          MessageBox(nullptr, _TT(Utils::translate("安装失败：没有找到H264编码工具。(论坛提问：https://bbs.pag.art/) ").c_str()), _TT(Utils::translate("AE插件安装失败").c_str()), MB_OK);
        }
    }

    if (!hasError && enablePluginTextBackground) {
        if (!hasPluginTextBackground) {
            hasError = true;
            MessageBox(nullptr, _TT(Utils::translate("安装失败：没有找到AE效果插件: TextBackground.aex 。(论坛提问：https://bbs.pag.art/) ").c_str()), _TT(Utils::translate("AE插件安装失败").c_str()), MB_OK);
        } else if (!hasAeCommonPath) {
            hasError = true;
            MessageBox(nullptr, _TT(Utils::translate("安装失败：没有找到AE公共目录。(论坛提问：https://bbs.pag.art/) ").c_str()), _TT(Utils::translate("AE插件安装失败").c_str()), MB_OK);
        }
    }

    if (!hasError && enablePluginImageFillRule) {
        if (!hasPluginImageFillRule) {
            hasError = true;
            MessageBox(nullptr, _TT(Utils::translate("安装失败：没有找到AE效果插件: ImageFillRule.aex  。(论坛提问：https://bbs.pag.art/) ").c_str()), _TT(Utils::translate("AE插件安装失败").c_str()), MB_OK);
        } else if (!hasAeCommonPath) {
            hasError = true;
            MessageBox(nullptr, _TT(Utils::translate("安装失败：没有找到AE公共目录。(论坛提问：https://bbs.pag.art/)").c_str()), _TT(Utils::translate("AE插件安装失败").c_str()), MB_OK);
        }
    }

    if(hasH264EncoderTools && !std::filesystem::exists(h264EncoderToolsPath)) {
      int nums=5;
      while(nums--) {
        char currentPathChar[MAX_PATH] = {0};
        snprintf(currentPathChar,sizeof(currentPathChar),"%s\\H264EncoderTools.exe",currentPath);
        std::filesystem::path sourcePath = currentPathChar;
        std::filesystem::path targetPath = h264EncoderToolsPath;
        try {
          // 复制文件
          std::filesystem::copy_file(sourcePath, targetPath, std::filesystem::copy_options::overwrite_existing);
        } catch (const std::filesystem::filesystem_error& e) {
          printf("copy %s to %s faile,errorInfo:%s\n",currentPathChar,h264EncoderToolsPath,e.what());
        }
        if(!std::filesystem::exists(h264EncoderToolsPath)) {
          continue;
        }else {
          break;
        }
      }
      if(!std::filesystem::exists(h264EncoderToolsPath)) {
        MessageBox(nullptr, _TT(Utils::translate("安装失败：没有找到H264编码工具。(论坛提问：https://bbs.pag.art/) ").c_str()), _TT(Utils::translate("AE插件安装失败").c_str()), MB_OK);
      }
    }
    if (!hasError && num > 0 && ret != -1) {
        MessageBox(nullptr, _TT(Utils::translate("安装成功！ ").c_str()), _TT(Utils::translate("AE插件安装成功 ").c_str()), MB_OK);
        StorePluginVersion(aeInstallPaths, countPaths);
    }

    return ret;
}

static void StoreCurrentPathForPlugin() {
  if (IsDebugMode()) {
    return;
  }

  char roamingPath[MAX_PATH_SIZE] = { 0 };
  auto flag = GetRoamingPath(roamingPath, sizeof(roamingPath));
  if (!flag) {
    return;
  }

  char dstPath[MAX_PATH_SIZE] = { 0 };
  snprintf(dstPath, sizeof(dstPath), "%s\\PAGExporter\\PAGViewerPath.txt", roamingPath);

  FILE* fp = fopen(dstPath, "wt");
  if (fp != nullptr) {
    char currentPath[MAX_PATH_SIZE];
    GetCurrentPath(currentPath, sizeof(currentPath), false);
    fprintf(fp, "%s", currentPath);
    fclose(fp);
  }
}

static int64_t VersionToInt64(const char* str) {
  int b0 = 0;
  int b1 = 0;
  int b2 = 0;
  int b3 = 0;
  int status = sscanf(str, "%d.%d.%d.%d", &b0, &b1, &b2, &b3);
  int64_t build = (static_cast<int64_t>(b0) << 48) |
    (static_cast<int64_t>(b1) << 32) |
    (static_cast<int64_t>(b2) << 16) |
    (static_cast<int64_t>(b3) << 0);
  return build;
}

static bool HasNewVersion() {
  bool hasAeCommonPath = (0 == access(AE_LIB_PATH, 0)); // 是否有AE Common目录
  if (!hasAeCommonPath) {
    return false; // 如果没有AE公共目录，意味着没有安装AE，也就不用提示安装AE插件
  }

  char configPath[MAX_PATH_SIZE] = { 0 };
  auto flag = GetPluginVersionPath(configPath, sizeof(configPath));
  if (!flag) {
    return false;
  }

  auto text = FileIO::ReadTextFile(configPath);
  if (text.length() <= 0) {
    return true;
  }
  auto cjson = cJSON_Parse(text.c_str());
  if (cjson == nullptr) {
    return true;
  }

  auto hasNew = false;
  auto hasOld = false;
  auto curVersionString = GetPAGExporterVersion();
  auto curVerion = VersionToInt64(curVersionString.c_str());
  int num = cJSON_GetArraySize(cjson);
  for (int i = 0; i < num; i++) {
    auto c = cJSON_GetArrayItem(cjson, i);
    if (c == nullptr) {
      break;
    }
    if (c->type == cJSON_String) {
      auto path = c->string;
      bool exist = (0 == access(path, 0));
      if (!exist) {
        break;
      }
      auto version = VersionToInt64(c->valuestring);
      if (curVerion > version) {
        hasOld = true;
        break;
      }
      else {
        hasNew = true;
      }
    }
  }
  cJSON_Delete(cjson);
  return hasOld || !hasNew;
}

auto PAGPluginInstaller::HasUpdate() -> bool {
  // Implement the function
  return true;
}

auto PAGPluginInstaller::InstallPlugins(bool bForceInstall) -> bool {
  return true;
}

auto PAGPluginInstaller::UninstallPlugins() -> bool {
  return true;
}

#if defined(WIN32)
#pragma warning(pop)
#endif