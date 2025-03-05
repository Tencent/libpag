#import "MacUpdater.h"
#import <QWindow>
#import <Cocoa/Cocoa.h>
#import <Sparkle/Sparkle.h>
#import "rendering/PAGWindow.h"
#import "utils/TranslateUtils.h"

@interface PAGUpdaterDelegate : NSObject<SPUUpdaterDelegate>
@property BOOL showUI;
@property(nonatomic, copy) NSString* feedUrl;
@end 

@implementation PAGUpdaterDelegate

-(NSString *)feedURLStringForUpdater:(SPUUpdater *)updater {
    return self.feedUrl;
}

- (void)updaterDidNotFindUpdate:(SPUUpdater *)updater {
    if(!self.showUI){
        return;
    }
    NSAlert *anAlert = [[NSAlert alloc] init];
    anAlert.messageText = @" ";
    anAlert.informativeText = @(Translate("您当前使用的 PAG 已经是最新版本。").c_str());
    [anAlert runModal];
}

- (bool)updaterDidFindValidUpdate:(SPUUpdater *)updater {
    for (int i = 0; i < PAGWindow::AllWindows.count(); i++) {
        auto window = PAGWindow::AllWindows[i];
        auto root = window->getEngine()->rootObjects().first();
        if (root) {
            QMetaObject::invokeMethod(root, "setHasNewVersion", Q_ARG(QVariant, true));
        }
    }
    return self.showUI;
}

- (void)updater:(SPUUpdater *)updater didAbortWithError:(NSError *)error {
    if(!self.showUI){
        return;
    }
    NSAlert *anAlert = [[NSAlert alloc] init];
    anAlert.messageText = @" ";
    anAlert.informativeText = @(Translate("查找更新时遇到一些问题，请稍后再试。").c_str());
    [anAlert runModal];
}
@end

static PAGUpdaterDelegate* suDelegate = nil;
static SPUStandardUpdaterController* updaterController = nil;

void MacUpdater::initUpdater() {
    if (!suDelegate) {
        suDelegate = [[PAGUpdaterDelegate alloc] init];
    }
    
    if (!updaterController) {
        updaterController = [[SPUStandardUpdaterController alloc] 
            initWithStartingUpdater:NO 
            updaterDelegate:suDelegate 
            userDriverDelegate:nil];
        SPUUpdater *updater = updaterController.updater;
        updater.automaticallyChecksForUpdates = NO;
        updater.automaticallyDownloadsUpdates = NO;
    }
}

void MacUpdater::checkUpdates(bool showUI, std::string feedUrl) {
    if (!updaterController) {
        qDebug() << "Error: Updater not initialized";
        return;
    }
    
    @autoreleasepool {
        if (suDelegate) {
            suDelegate.showUI = showUI;
            suDelegate.feedUrl = [NSString stringWithUTF8String:feedUrl.c_str()];
        }
        
        SPUUpdater *updater = updaterController.updater;
        if (updater) {
            if (![updater canCheckForUpdates]) {
                [updaterController startUpdater];
            }

            if ([updater canCheckForUpdates]) {
                if (showUI) {
                    [updaterController checkForUpdates:nil];
                } else {
                    [updater checkForUpdatesInBackground];
                }
            } else {
                qDebug() << "Error: Updater is not ready to check for updates";
            }
        }
    }
}

void MacUpdater::changeTitleBarColor(WId winId, double red, double green, double blue) 
{
    if (winId == 0) return;
    NSView* view = (NSView*)winId;
    NSWindow* window = [view window];
    if (@available(macOS 10.15, *)) {
        window.titleVisibility = NSWindowTitleHidden;
        window.styleMask |= NSWindowStyleMaskFullSizeContentView;
        window.titlebarAppearsTransparent = true;
        window.contentView.wantsLayer = true;
    }
    window.colorSpace = [NSColorSpace extendedSRGBColorSpace];
    window.backgroundColor = [NSColor colorWithRed:red green:green blue:blue alpha:1.];
}