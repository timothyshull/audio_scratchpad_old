//
//  main.m
//  ca_1_1
//
//  Created by Timothy Shull on 7/4/16.
//  Copyright © 2016 Timothy Shull. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        if (argc < 2) {
            NSLog(@"Usage: CAMetadata /full/path/to/audiofile\n");
            return -1;
        }
        
        NSString *audioFilePath = [[NSString stringWithUTF8String:argv[1]] stringByExpandingTildeInPath];
        
        NSURL *audioURL = [NSURL fileURLWithPath:audioFilePath];
        
        AudioFileID audioFile;
        OSStatus theErr = noErr;
        theErr = AudioFileOpenURL((__bridge CFURLRef) audioURL, kAudioFileReadPermission, 0, &audioFile);
        assert(theErr == noErr);
        UInt32 dictionarySize = 0;
        theErr = AudioFileGetPropertyInfo (audioFile, kAudioFilePropertyInfoDictionary, &dictionarySize, 0);
        assert(theErr == noErr);
        CFDictionaryRef dictionary;
        theErr = AudioFileGetProperty (audioFile, kAudioFilePropertyInfoDictionary, &dictionarySize, &dictionary);
        assert(theErr == noErr);
        NSLog(@"dictionary: %@", dictionary);
        CFRelease(dictionary);
        theErr = AudioFileClose(audioFile);
        assert(theErr == noErr);
    }
    return 0;
}
