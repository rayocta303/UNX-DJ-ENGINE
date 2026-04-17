#include "raylib.h"
#import <UIKit/UIKit.h>

extern int raylib_main(int argc, char *argv[]);

// This is the standard Raylib iOS AppController
// It handles the main loop via CADisplayLink

@interface AppController : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@interface RaylibViewController : UIViewController
@end

@implementation AppController

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.window.rootViewController = [[RaylibViewController alloc] init];
    [self.window makeKeyAndVisible];
    return YES;
}

@end

@implementation RaylibViewController

- (void)loadView {
    // Raylib's internal iOS logic will attach to the root view controller's view
    self.view = [[UIView alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
}

- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }

@end

@implementation AppController (Raylib)

- (void)applicationDidBecomeActive:(UIApplication *)application {
    static BOOL started = NO;
    if (!started) {
        started = YES;
        // The raylib_main will call InitWindow and emscripten_set_main_loop
        // which on iOS starts the CADisplayLink and returns.
        raylib_main(0, NULL);
    }
}

@end

// Entry point for iOS
int main(int argc, char *argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppController class]));
    }
}
