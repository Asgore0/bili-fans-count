#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <UserNotifications/UserNotifications.h>
#import <ServiceManagement/ServiceManagement.h>
#import <math.h>
#import <stdlib.h>

// Keep one translation unit so the lightweight tests can exercise static helpers
// while the source remains navigable by domain.
#import "parts/BFCShared.inc"
#import "parts/BFCSettings.inc"
#import "parts/BFCBiliClient.inc"
#import "parts/BFCViews.inc"
#import "parts/BFCAppDelegate.inc"

int main(int argc, const char * argv[]) {
    (void)argc;
    (void)argv;
    @autoreleasepool {
        NSApplication *app = NSApplication.sharedApplication;
        AppDelegate *delegate = [AppDelegate new];
        app.delegate = delegate;
        [app run];
    }
    return 0;
}
