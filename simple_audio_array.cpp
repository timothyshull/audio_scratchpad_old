//
//  main.cpp
//  simple_audio_array
//
//  Created by Tim Shull on 10/8/16.
//  Copyright © 2016 Tim Shull. All rights reserved.
//

#include <AudioToolbox/AudioToolbox.h>
#include <vector>

int main(int argc, const char *argv[]) {
    CFStringRef filePath = CFStringCreateWithCString(nullptr, argv[1], kCFStringEncodingUTF8);
    CFURLRef inputFileURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, filePath, kCFURLPOSIXPathStyle, false);

    ExtAudioFileRef fileRef;
    ExtAudioFileOpenURL(inputFileURL, &fileRef);


    AudioStreamBasicDescription audioFormat;
    audioFormat.mSampleRate = 44100;
    audioFormat.mFormatID = kAudioFormatLinearPCM;
    audioFormat.mFormatFlags = kLinearPCMFormatFlagIsFloat;
    audioFormat.mBitsPerChannel = sizeof(Float32) * 8;
    audioFormat.mChannelsPerFrame = 1;
    audioFormat.mBytesPerFrame = audioFormat.mChannelsPerFrame * sizeof(Float32);
    audioFormat.mFramesPerPacket = 1;
    audioFormat.mBytesPerPacket = audioFormat.mFramesPerPacket * audioFormat.mBytesPerFrame;

    ExtAudioFileSetProperty(fileRef, kExtAudioFileProperty_ClientDataFormat, sizeof(AudioStreamBasicDescription), &audioFormat);

    int numSamples = 1024;
    UInt32 sizePerPacket = audioFormat.mBytesPerPacket;
    UInt32 packetsPerBuffer = static_cast<UInt32>(numSamples);
    UInt32 outputBufferSize = packetsPerBuffer * sizePerPacket;

    UInt8 *outputBuffer = static_cast<UInt8 *>(malloc(sizeof(UInt8 *) * outputBufferSize));

    AudioBufferList convertedData;

    convertedData.mNumberBuffers = 1;
    convertedData.mBuffers[0].mNumberChannels = audioFormat.mChannelsPerFrame;
    convertedData.mBuffers[0].mDataByteSize = outputBufferSize;
    convertedData.mBuffers[0].mData = outputBuffer;

    UInt32 frameCount = static_cast<UInt32>(numSamples);
    float *samplesAsCArray;
//    int j = 0;

    SInt64 length;
    UInt32 size = sizeof(SInt64);
    ExtAudioFileGetProperty(fileRef, kExtAudioFileProperty_FileLengthFrames, &size, &length);

    std::vector<double> fileVector;
//    fileVector.reserve(length);
//    double floatDataArray[882000];
    
    // there is a bug somewhere in here
    // fileVector ends up with size of 169984
    // could be including file header
    // could be cast to float * from mData
    while (frameCount > 0) {
        ExtAudioFileRead(fileRef, &frameCount, &convertedData);
        if (frameCount > 0) {
            AudioBuffer audioBuffer = convertedData.mBuffers[0];
            samplesAsCArray = static_cast<float *>(audioBuffer.mData);
            for (int i = 0; i < 1024; i++) {
                fileVector.push_back(static_cast<double>(samplesAsCArray[i]));
//                printf("\n%f", floatDataArray[j]);
//                j++;
            }
        }
    }
    
    std::vector<double> returnVector{fileVector.begin() + 5000, fileVector.begin() + 5010};
    return 0;
}
