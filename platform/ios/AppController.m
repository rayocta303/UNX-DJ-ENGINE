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

@interface RaylibView : UIView
@end

@implementation RaylibView
+ (Class)layerClass { return [CAEAGLLayer class]; }
@end

@implementation RaylibViewController

- (void)loadView {
    RaylibView *rv = [[RaylibView alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    CAEAGLLayer *eaglLayer = (CAEAGLLayer *)rv.layer;
    eaglLayer.opaque = YES;
    eaglLayer.drawableProperties = @{
        kEAGLDrawablePropertyRetainedBacking: @NO,
        kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8
    };
    self.view = rv;
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
