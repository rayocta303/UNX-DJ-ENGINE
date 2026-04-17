#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#include <string.h>
#include <stdlib.h>

// Returns the absolute path to a file within the app bundle
const char* ios_get_bundle_path(const char* filename) {
    static char path[1024];
    NSString* nsFilename = [NSString stringWithUTF8String:filename];
    NSString* baseName = [nsFilename stringByDeletingPathExtension];
    NSString* extension = [nsFilename pathExtension];
    
    // Check if filename has subdirectories
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
    
    return filename; // Fallback
}

void ios_init_audio_session() {
    AVAudioSession *session = [AVAudioSession sharedInstance];
    [session setCategory:AVAudioSessionCategoryPlayback error:nil];
    [session setActive:YES error:nil];
}
