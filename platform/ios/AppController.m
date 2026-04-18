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
    eaglLayer.contentsScale = [UIScreen mainScreen].nativeScale;
    self.view = rv;
}

- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }

@end

#import <AVFoundation/AVFoundation.h>

@implementation AppController (Audio)

- (void)applicationDidBecomeActive:(UIApplication *)application {
    static BOOL started = NO;
    if (!started) {
        started = YES;
        
        // --- NATIVE iOS AUDIO CONFIGURATION ---
        AVAudioSession *session = [AVAudioSession sharedInstance];
        NSError *error = nil;
        
        // 1. Set Category to Playback (Ensures audio continues in background/silent mode)
        [session setCategory:AVAudioSessionCategoryPlayback 
                 withOptions:AVAudioSessionCategoryOptionMixWithOthers | AVAudioSessionCategoryOptionAllowBluetooth
                       error:&error];
        
        // 2. Set Low Latency Buffer (5ms)
        [session setPreferredIOBufferDuration:0.005 error:&error];
        
        // 3. Set Sample Rate to 48kHz (Match Pioneer Engine)
        [session setPreferredSampleRate:48000 error:&error];
        
        // 4. Activate
        [session setActive:YES error:&error];
        
        if (error) {
            NSLog(@"[IOS_AUDIO] Error configuring AVAudioSession: %@", error.localizedDescription);
        } else {
            NSLog(@"[IOS_AUDIO] AVAudioSession configured successfully (48kHz, 5ms buffer)");
        }
        
        // The raylib_main will call InitWindow and starts the CADisplayLink.
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
