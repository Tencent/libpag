#ifndef PLATFORM_MACOS_PAG_UPDATER_H_
#define PLATFORM_MACOS_PAG_UPDATER_H_

#include <string>
#include <QWindow>

class PAGUpdater
{
public:
    static void initUpdater();
    static void checkUpdates(bool showUI, std::string feedUrl);
    static void changeTitleBarColor(WId winId, double red, double green, double blue);
};

#endif // PLATFORM_MACOS_PAG_UPDATER_H_