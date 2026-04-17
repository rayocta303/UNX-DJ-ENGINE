/*******************************************************************************
 *  platform/ios/rcore_ios.m
 *
 *  iOS Platform Backend for Raylib 5.0
 *  Implements the external platform symbols required by rcore.c on PLATFORM_IOS.
 *
 *  Uses Apple EAGL (not EGL/ANGLE) for OpenGL ES 3.0.
 *  The main application loop is driven by CADisplayLink via
 *  emscripten_set_main_loop_arg() — same pattern used by AppController.m.
 ******************************************************************************/

#import <UIKit/UIKit.h>
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/gl.h>
#import <QuartzCore/QuartzCore.h>
#import <Foundation/Foundation.h>
#include <mach/mach_time.h>

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
    /* Time */
    mach_timebase_info(&_timebase);
    _startTime = mach_absolute_time();

    /* OpenGL ES 3 context */
    _glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    if (!_glContext)
        _glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    if (!_glContext || ![EAGLContext setCurrentContext:_glContext])
        return -1;

    /* Default framebuffer */
    glGenFramebuffers(1, &_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);

    /* Color renderbuffer */
    glGenRenderbuffers(1, &_colorRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, _colorRBO);

    /* Depth renderbuffer */
    CGSize sz    = [UIScreen mainScreen].bounds.size;
    float  scale = [UIScreen mainScreen].nativeScale;
    int    w     = (int)(sz.width  * scale);
    int    h     = (int)(sz.height * scale);
    glGenRenderbuffers(1, &_depthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, _depthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, _depthRBO);

    return 0;
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
    return (double)elapsed * (double)_timebase.numer
                           / ((double)_timebase.denom * 1e9);
}

Vector2 GetWindowScaleDPI(void) {
    float s = [UIScreen mainScreen].nativeScale;
    return (Vector2){ s, s };
}

void SwapScreenBuffer(void) {
    if (_glContext && _colorRBO) {
        glBindRenderbuffer(GL_RENDERBUFFER, _colorRBO);
        [_glContext presentRenderbuffer:GL_RENDERBUFFER];
    }
}

void PollInputEvents(void) {
    /* iOS is event-driven via UIKit; touch events arrive through UIResponder.
       No polling needed here — raylib's gesture system is fed by AppController. */
}

/* No-ops for desktop-only window management */
void MaximizeWindow(void)           {}
void MinimizeWindow(void)           {}
void SetWindowMinSize(int w, int h) { (void)w; (void)h; }
void SetWindowSize(int w, int h)    { (void)w; (void)h; }

/*
 * emscripten_set_main_loop_arg — the iOS main-loop adapter.
 * AppController.m calls InitWindow → raylib_main → this function.
 * We set up a CADisplayLink that fires at `fps` Hz (60 if fps <= 0).
 * simulate_infinite_loop is ignored; the iOS RunLoop IS the infinite loop.
 */
void emscripten_set_main_loop_arg(void (*func)(void *), void *arg,
                                  int fps, int simulate_infinite_loop)
{
    _loopFunc = func;
    _loopArg  = arg;

    _displayTarget = [_RLDisplayTarget shared];
    _displayLink   = [CADisplayLink displayLinkWithTarget:_displayTarget
                                               selector:@selector(step:)];
    _displayLink.preferredFramesPerSecond = (fps > 0) ? fps : 60;
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop]
                       forMode:NSDefaultRunLoopMode];
}
