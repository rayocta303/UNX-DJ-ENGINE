#import <UIKit/UIKit.h>
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES2/gl.h>
#import <QuartzCore/QuartzCore.h>
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#include <mach/mach_time.h>
#include <unistd.h>

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
    return filename;
}

/* ---- Raylib types we need (avoid including full raylib.h) ---- */
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
   Platform Function Implementations
   ============================================================ */

int InitPlatform(void) {
    const char *docsPath = ios_get_documents_path();
    if (docsPath && docsPath[0] != '\0') {
        char logPath[1024];
        snprintf(logPath, sizeof(logPath), "%s/log.txt", docsPath);
        _logFile = fopen(logPath, "w");
        if (_logFile) {
            fprintf(_logFile, "=== XDJ-UNX iOS LOG START ===\n");
            dup2(fileno(_logFile), STDOUT_FILENO);
            dup2(fileno(_logFile), STDERR_FILENO);
            void SetTraceLogCallback(void (*callback)(int, const char *, va_list));
            SetTraceLogCallback(ios_log_callback);
        }
    }

    AVAudioSession *session = [AVAudioSession sharedInstance];
    NSError *error = nil;
    [session setCategory:AVAudioSessionCategoryPlayback error:&error];
    [session setActive:YES error:&error];

    NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
    chdir([resourcePath UTF8String]);

    mach_timebase_info(&_timebase);
    _startTime = mach_absolute_time();

    _glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    if (!_glContext)
        _glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    
    if (!_glContext || ![EAGLContext setCurrentContext:_glContext]) return -1;

    // Safer way to get the window and root view on modern iOS
    UIWindow *window = nil;
    if (@available(iOS 13.0, *)) {
        for (UIScene *scene in [UIApplication sharedApplication].connectedScenes) {
            if ([scene isKindOfClass:[UIWindowScene class]]) {
                window = ((UIWindowScene *)scene).windows.firstObject;
                if (window) break;
            }
        }
    }
    if (!window) window = [UIApplication sharedApplication].keyWindow;
    
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

int GetScreenWidth(void) {
    return (int)[UIScreen mainScreen].bounds.size.width;
}

int GetScreenHeight(void) {
    return (int)[UIScreen mainScreen].bounds.size.height;
}

int GetRenderWidth(void) {
    return (int)([UIScreen mainScreen].bounds.size.width * [UIScreen mainScreen].nativeScale);
}

int GetRenderHeight(void) {
    return (int)([UIScreen mainScreen].bounds.size.height * [UIScreen mainScreen].nativeScale);
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
