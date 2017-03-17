//
//  main.swift
//  ca_3_1_s
//
//  Created by Timothy Shull on 7/5/16.
//  Copyright © 2016 Timothy Shull. All rights reserved.
//

import Foundation
import AudioToolbox

var fileTypeAndFormat : AudioFileTypeAndFormatID = AudioFileTypeAndFormatID(); // 1
//        fileTypeAndFormat.mFileType = kAudioFileAIFFType;
//        fileTypeAndFormat.mFileType = kAudioFileWAVEType;
//        fileTypeAndFormat.mFileType = kAudioFileCAFType;
fileTypeAndFormat.mFileType = kAudioFileMPEG4Type;
fileTypeAndFormat.mFormatID = kAudioFormatLinearPCM;
var audioErr : OSStatus = noErr; // 2
var infoSize : UInt32 = 0;
var fileTypeSize : UInt32 = UInt32(2 * sizeof(UInt32)); // Cannot calculate size directly, find a better solution
//audioErr = AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_AvailableStreamDescriptionsForFormat, sizeof(fileTypeAndFormat), &fileTypeAndFormat, &infoSize);
audioErr = AudioFileGetGlobalInfoSize(kAudioFileGlobalInfo_AvailableStreamDescriptionsForFormat, fileTypeSize, &fileTypeAndFormat, &infoSize);
assert(audioErr == noErr);
//var asbds : UnsafePointer<AudioStreamBasicDescription> = malloc(infoSize); // 4
var asbds : UnsafeMutablePointer<AudioStreamBasicDescription> = UnsafeMutablePointer.alloc(Int(infoSize));
//audioErr = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AvailableStreamDescriptionsForFormat, sizeof(fileTypeAndFormat), &fileTypeAndFormat, &infoSize, asbds);
audioErr = AudioFileGetGlobalInfo(kAudioFileGlobalInfo_AvailableStreamDescriptionsForFormat, fileTypeSize, &fileTypeAndFormat, &infoSize, asbds);
assert(audioErr == noErr);
var asbdCount : Int = Int(infoSize) / sizeof(AudioStreamBasicDescription); // 6

for i in 0..<asbdCount {
    var format4cc : UInt32 = CFSwapInt32HostToBig(asbds[i].mFormatID); // 7
    let aStr = String(format: "% d: mFormatId: %4.4s, mFormatFlags: %d, mBitsPerChannel: %d", i, format4cc, asbds[i].mFormatFlags, asbds[i].mBitsPerChannel)
    print(aStr)
}

