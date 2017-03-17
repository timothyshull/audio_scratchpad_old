//
//  main.m
//  ca_4_1
//
//  Created by Timothy Shull on 7/5/16.
//  Copyright (c) 2016 tim_shull. All rights reserved.
//

//#import <Foundation/Foundation.h>
#include <AudioToolbox/AudioToolbox.h>

const int kNumberRecordBuffers = 3;

#pragma mark user data struct
//#pragma mark user info struct
typedef struct MyRecorder {
    AudioFileID recordFile;
    SInt64 recordPacket;
    Boolean running;
} MyRecorder;

// Insert Listing 4.3 here
#pragma mark utility functions

static void CheckError(OSStatus error, const char *operation) {
    if (error == noErr)
        return;
    char errorString[20];
    // See if it appears to be a 4-char-code
    *(UInt32 *) (errorString + 1) = CFSwapInt32HostToBig((uint32_t) error);
    if (isprint(errorString[1]) && isprint(errorString[2]) && isprint(errorString[3]) && isprint(errorString[4])) {
        errorString[0] = errorString[5] = '\'';
        errorString[6] = '\0';
    } else // No, format it as an integer
        sprintf(errorString, "% d", (int) error);
    fprintf(stderr, "Error: %s (%s)\n", operation, errorString);
    exit(1);
}

OSStatus MyGetDefaultInputDeviceSampleRate(Float64 *outSampleRate) {
    OSStatus error;
    AudioDeviceID deviceID = 0;
    AudioObjectPropertyAddress propertyAddress;
    UInt32 propertySize;
    propertyAddress.mSelector = kAudioHardwarePropertyDefaultInputDevice;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = 0;
    propertySize = sizeof(AudioDeviceID);
    // AudioHardwareServiceGetPropertyData -> deprecated, replace with AudioObjectGetPropertyData
    error = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize, &deviceID);
    if (error) return error;
    propertyAddress.mSelector = kAudioDevicePropertyNominalSampleRate;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = 0;
    propertySize = sizeof(Float64);
    // AudioHardwareServiceGetPropertyData -> deprecated, replace with AudioObjectGetPropertyData
    error = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, NULL, &propertySize, outSampleRate);
    return error;
}

static int MyComputeRecordBufferSize(const AudioStreamBasicDescription *format, AudioQueueRef queue, float seconds) {
    int packets, frames, bytes;
    frames = (int) ceil(seconds * format->mSampleRate);
    if (format->mBytesPerFrame > 0) // 1
        bytes = frames * format->mBytesPerFrame;
    else {
        UInt32 maxPacketSize;
        if (format->mBytesPerPacket > 0) // 2
            // Constant packet size
            maxPacketSize = format->mBytesPerPacket;
        else { // Get the largest single packet size possible
            UInt32 propertySize = sizeof(maxPacketSize); // 3
            CheckError(AudioQueueGetProperty(queue, kAudioConverterPropertyMaximumOutputPacketSize, &maxPacketSize, &propertySize), "Couldn't get queue's maximum output packet size");
        }
        if (format->mFramesPerPacket > 0) packets = frames / format->mFramesPerPacket; // 4
        else
            // Worst-case scenario: 1 frame in a packet
            packets = frames; // 5
        // Sanity check
        if (packets == 0) packets = 1;
        bytes = packets * maxPacketSize; // 6
    }
    return bytes;
}


static void MyCopyEncoderCookieToFile(AudioQueueRef queue, AudioFileID theFile) {
    OSStatus error;
    UInt32 propertySize;
    error = AudioQueueGetPropertySize(queue, kAudioConverterCompressionMagicCookie, &propertySize);
    if (error == noErr && propertySize > 0) {
        Byte *magicCookie = (Byte *) malloc(propertySize);
        CheckError(AudioQueueGetProperty(queue, kAudioQueueProperty_MagicCookie, magicCookie, &propertySize), "Couldn't get audio queue's magic cookie");
        CheckError(AudioFileSetProperty(theFile, kAudioFilePropertyMagicCookieData, propertySize, magicCookie), "Couldn't set audio file's magic cookie");
        free(magicCookie);
    }
}



// Insert Listings 4.20 and 4.21 here
// Insert Listing 4.22 here
// Insert Listing 4.23 here
#pragma mark record callback function

// Replace with Listings 4.24-4.26
static void MyAQInputCallback(void *inUserData,
        AudioQueueRef inQueue,
        AudioQueueBufferRef inBuffer,
        const AudioTimeStamp *inStartTime,
        UInt32 inNumPackets,
        const AudioStreamPacketDescription *inPacketDesc) {
    MyRecorder *recorder = (MyRecorder *) inUserData;
    if (inNumPackets > 0) { // Write packets to a file
        CheckError(AudioFileWritePackets(recorder->recordFile, FALSE, inBuffer->mAudioDataByteSize, inPacketDesc, recorder->recordPacket, &inNumPackets, inBuffer->mAudioData), "AudioFileWritePackets failed"); // Increment the packet index
        recorder->recordPacket += inNumPackets;
    }
    if (recorder->running)
        CheckError(AudioQueueEnqueueBuffer(inQueue, inBuffer, 0, NULL), "AudioQueueEnqueueBuffer failed");
}

#pragma mark main function

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        // Set up format

        // Insert Listings 4.4-4.7 here
        MyRecorder recorder = {0};
        AudioStreamBasicDescription recordFormat;
        memset(&recordFormat, 0, sizeof(recordFormat));
        recordFormat.mFormatID = kAudioFormatMPEG4AAC;
        recordFormat.mChannelsPerFrame = 2;
        MyGetDefaultInputDeviceSampleRate(&recordFormat.mSampleRate);
        UInt32 propSize = sizeof(recordFormat);
        CheckError(AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, 0, NULL, &propSize, &recordFormat), "AudioFormatGetProperty failed");

        // Set up queue
        // Insert Listings 4.8-4.9 here
        AudioQueueRef queue = {0};
        CheckError(AudioQueueNewInput(&recordFormat, MyAQInputCallback, &recorder, NULL, NULL, 0, &queue), "AudioQueueNewInput failed");
        UInt32 size = sizeof(recordFormat);
        CheckError(AudioQueueGetProperty(queue, kAudioConverterCurrentOutputStreamDescription, &recordFormat, &size), "Couldn't get queue's format");

        // Set up file
        // Insert Listings 4.10-4.11 here
        CFURLRef myFileURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("/Users/skull/Desktop/output.caf"), kCFURLPOSIXPathStyle, false);
        CheckError(AudioFileCreateWithURL(myFileURL, kAudioFileCAFType, &recordFormat, kAudioFileFlags_EraseFile, &recorder.recordFile), "AudioFileCreateWithURL failed");
        CFRelease(myFileURL);
        MyCopyEncoderCookieToFile(queue, recorder.recordFile);

        // Other setup as needed
        // Insert Listings 4.12-4.13 here
        int bufferByteSize = MyComputeRecordBufferSize(&recordFormat, queue, 0.5);
        int bufferIndex;
        for (bufferIndex = 0; bufferIndex < kNumberRecordBuffers; ++bufferIndex) {
            AudioQueueBufferRef buffer;
            CheckError(AudioQueueAllocateBuffer(queue, (UInt32) bufferByteSize, &buffer), "AudioQueueAllocateBuffer failed");
            CheckError(AudioQueueEnqueueBuffer(queue, buffer, 0, NULL), "AudioQueueEnqueueBuffer failed");
        }

        // Start queue
        // Insert Listings 4.14-4.15 here

        recorder.running = TRUE;
        CheckError(AudioQueueStart(queue, NULL), "AudioQueueStart failed");
        printf(" Recording, press < return > to stop:\n");
        getchar();

        // Stop queue
        // Insert Listings 4.16-4.18 here
        printf("* recording done *\n");
        recorder.running = FALSE;
        CheckError(AudioQueueStop(queue, TRUE), "AudioQueueStop failed");
        MyCopyEncoderCookieToFile(queue, recorder.recordFile);
        AudioQueueDispose(queue, TRUE);
        AudioFileClose(recorder.recordFile);
    }

    return 0;

}
