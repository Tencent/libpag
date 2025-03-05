#ifndef MAC_MAC_UPDATER_H_
#define MAC_MAC_UPDATER_H_

#include <string>
#include <QWindow>

class MacUpdater
{
public:
    static void initUpdater();
    static void checkUpdates(bool showUI, std::string feedUrl);
    static void changeTitleBarColor(WId winId, double red, double green, double blue);
};

#endif // MAC_MAC_UPDATER_H_