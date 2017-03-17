//
//  main.m
//  ca_3_1
//
//  Created by Timothy Shull on 7/5/16.
//  Copyright © 2016 Timothy Shull. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        AudioFileTypeAndFormatID fileTypeAndFormat; // 1
//        fileTypeAndFormat.mFileType = kAudioFileAIFFType;
//        fileTypeAndFormat.mFileType = kAudioFileWAVEType;
//        fileTypeAndFormat.mFileType = kAudioFileCAFType;
        fileTypeAndFormat.mFileType = kAudioFileMPEG4Type;
        fileTypeAndFormat.mFormatID = kAudioFormatLinearPCM;
        OSStatus audioErr = noErr; // 2
        UInt32 infoSize = 0;
        audioErr = AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_AvailableStreamDescriptionsForFormat, sizeof(fileTypeAndFormat), &fileTypeAndFormat, &infoSize);
        assert(audioErr == noErr);
        AudioStreamBasicDescription *asbds = malloc(infoSize); // 4
        audioErr = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AvailableStreamDescriptionsForFormat, sizeof(fileTypeAndFormat), &fileTypeAndFormat, &infoSize, asbds);
        assert(audioErr == noErr);
        int asbdCount = infoSize / sizeof (AudioStreamBasicDescription); // 6
        
        for (int i = 0; i < asbdCount; i++) {
            UInt32 format4cc = CFSwapInt32HostToBig(asbds[i]. mFormatID); // 7
            NSLog(@"% d: mFormatId: %4.4s, mFormatFlags: %d, mBitsPerChannel: %d", i, (char*)& format4cc, asbds[i]. mFormatFlags, asbds[i]. mBitsPerChannel);
        }
        free(asbds); // 9
        
        
        
    }
    return 0;
}
