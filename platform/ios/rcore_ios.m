#import <UIKit/UIKit.h>
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES2/gl.h>
#import <QuartzCore/QuartzCore.h>
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#include <mach/mach_time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

/* ---- Raylib types we need ---- */
#ifndef RAYLIB_H
typedef struct { float x; float y; } Vector2;
#endif

/* ---- Platform State ---- */
static EAGLContext       *_glContext   = nil;
static GLuint             _fbo         = 0;
static GLuint             _colorRBO    = 0;
static GLuint             _depthRBO    = 0;

static mach_timebase_info_data_t _timebase;
static uint64_t           _startTime   = 0;

static void             (*_loopFunc)(void *) = NULL;
static void              *_loopArg          = NULL;

/* ---- CADisplayLink Target ---- */
@interface _RLDisplayTarget : NSObject
+ (instancetype)shared;
- (void)step:(CADisplayLink *)link;
@end

static _RLDisplayTarget *_displayTarget = nil;
static CADisplayLink    *_displayLink   = nil;

/* ---- Logging Helper ---- */
static FILE *_logFile = NULL;
void ios_log_callback(int logLevel, const char *text, va_list args) {
    if (!_logFile) return;
    const char *levelStr = "INFO";
    switch (logLevel) {
        case 2: levelStr = "INFO"; break;
        case 3: levelStr = "WARNING"; break;
        case 4: levelStr = "ERROR"; break;
        case 5: levelStr = "FATAL"; break;
        case 6: levelStr = "DEBUG"; break;
    }
    fprintf(_logFile, "[%s] ", levelStr);
    vfprintf(_logFile, text, args);
    fprintf(_logFile, "\n");
    fflush(_logFile);
    vprintf(text, args);
    printf("\n");
}

@implementation _RLDisplayTarget
+ (instancetype)shared {
    static _RLDisplayTarget *s = nil;
    if (!s) s = [[_RLDisplayTarget alloc] init];
    return s;
}
- (void)step:(CADisplayLink *)link {
    if (_loopFunc) _loopFunc(_loopArg);
}
@end

/* ============================================================
   iOS Path Helpers
   ============================================================ */

const char* ios_get_documents_path(void) {
    static char path[1024] = {0};
    if (path[0] == '\0') {
        NSString *docs = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
        if (docs) strcpy(path, [docs UTF8String]);
    }
    return path;
}

const char* ios_get_media_path(void) {
    return "/var/mobile/Media";
}

const char* ios_get_bundle_path(const char* filename) {
    static char path[1024];
    NSString* nsFilename = [NSString stringWithUTF8String:filename];
    NSString* baseName = [nsFilename stringByDeletingPathExtension];
    NSString* extension = [nsFilename pathExtension];
    NSString* directory = nil;
    if ([baseName containsString:@"/"]) {
        directory = [baseName stringByDeletingLastPathComponent];
        baseName = [baseName lastPathComponent];
    }
    NSString* nsPath = [[NSBundle mainBundle] pathForResource:baseName ofType:extension inDirectory:directory];
    if (nsPath) {
        strncpy(path, [nsPath UTF8String], sizeof(path) - 1);
        return path;
    }
    return filename;
}

void ios_init_audio_session(void) {
    AVAudioSession *session = [AVAudioSession sharedInstance];
    [session setCategory:AVAudioSessionCategoryPlayback error:nil];
    [session setActive:YES error:nil];
}

/* ============================================================
   Platform Function Implementations (Raylib)
   ============================================================ */

int InitPlatform(void) {
    /* Set up Logging to Documents/log.txt */
    const char *docsPath = ios_get_documents_path();
    if (docsPath && docsPath[0] != '\0') {
        char logPath[1024];
        snprintf(logPath, sizeof(logPath), "%s/log.txt", docsPath);
        
        // Open for writing - this will create the file if it doesn't exist
        _logFile = fopen(logPath, "w"); 
        if (_logFile) {
            fprintf(_logFile, "=== XDJ-UNX iOS LOG START ===\n");
            fprintf(_logFile, "Documents Path: %s\n", docsPath);
            fflush(_logFile);
            
            // Redirect stdout and stderr to our log file
            dup2(fileno(_logFile), STDOUT_FILENO);
            dup2(fileno(_logFile), STDERR_FILENO);
            
            // Register raylib log callback
            void SetTraceLogCallback(void (*callback)(int, const char *, va_list));
            SetTraceLogCallback(ios_log_callback);
            
            printf("[rcore_ios] Logging initialized to: %s\n", logPath);
        } else {
            NSLog(@"[rcore_ios] FAILED to open log file at: %s", logPath);
        }
    } else {
        NSLog(@"[rcore_ios] FAILED to get documents path for logging");
    }

    ios_init_audio_session();

    NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
    chdir([resourcePath UTF8String]);

    mach_timebase_info(&_timebase);
    _startTime = mach_absolute_time();

    _glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    if (!_glContext)
        _glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    
    if (!_glContext || ![EAGLContext setCurrentContext:_glContext]) return -1;

    UIWindow *window = nil;
    if (@available(iOS 13.0, *)) {
        for (UIScene *scene in [UIApplication sharedApplication].connectedScenes) {
            if ([scene isKindOfClass:[UIWindowScene class]]) {
                window = ((UIWindowScene *)scene).windows.firstObject;
                if (window) break;
            }
        }
    }
    if (!window) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        window = [UIApplication sharedApplication].keyWindow;
#pragma clang diagnostic pop
    }
    
    UIView *rootView = window.rootViewController.view;
    if (!rootView) return -1;
    CAEAGLLayer *glLayer = (CAEAGLLayer *)rootView.layer;

    glGenFramebuffers(1, &_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);

    glGenRenderbuffers(1, &_colorRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRBO);
    [_glContext renderbufferStorage:GL_RENDERBUFFER fromDrawable:(id<EAGLDrawable>)glLayer];
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _colorRBO);

    CGSize sz    = [UIScreen mainScreen].bounds.size;
    float  scale = [UIScreen mainScreen].nativeScale;
    int    w     = (int)(sz.width  * scale);
    int    h     = (int)(sz.height * scale);
    glGenRenderbuffers(1, &_depthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, _depthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthRBO);

    return 0;
}

Vector2 GetWindowScaleDPI(void) {
    float s = [UIScreen mainScreen].nativeScale;
    return (Vector2){ s, s };
}

void ClosePlatform(void) {
    [_displayLink invalidate];
    _displayLink  = nil;
    _displayTarget = nil;
    if (_fbo)      { glDeleteFramebuffers(1,  &_fbo);      _fbo      = 0; }
    if (_colorRBO) { glDeleteRenderbuffers(1, &_colorRBO); _colorRBO = 0; }
    if (_depthRBO) { glDeleteRenderbuffers(1, &_depthRBO); _depthRBO = 0; }
    [EAGLContext setCurrentContext:nil];
    _glContext = nil;
}

double GetTime(void) {
    uint64_t now     = mach_absolute_time();
    uint64_t elapsed = now - _startTime;
    return (double)elapsed * (double)_timebase.numer / ((double)_timebase.denom * 1e9);
}

void SwapScreenBuffer(void) {
    if (_glContext && _colorRBO) {
        glBindRenderbuffer(GL_RENDERBUFFER, _colorRBO);
        [_glContext presentRenderbuffer:GL_RENDERBUFFER];
    }
}

void PollInputEvents(void) {}
void MaximizeWindow(void)           {}
void MinimizeWindow(void)           {}
void SetWindowMinSize(int w, int h) { (void)w; (void)h; }
void SetWindowSize(int w, int h)    { (void)w; (void)h; }

void emscripten_set_main_loop_arg(void (*func)(void *), void *arg,
                                   int fps, int simulate_infinite_loop)
{
    _loopFunc = func;
    _loopArg  = arg;
    _displayTarget = [_RLDisplayTarget shared];
    _displayLink   = [CADisplayLink displayLinkWithTarget:_displayTarget selector:@selector(step:)];
    if (@available(iOS 15.0, *)) {
        float f = (fps > 0) ? (float)fps : 60.0f;
        _displayLink.preferredFrameRateRange = CAFrameRateRangeMake(f, f, f);
    } else {
        _displayLink.preferredFramesPerSecond = (fps > 0) ? fps : 60;
    }
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
}
