//
// Created by Tim Shull on 9/26/16.
// Copyright (c) 2016 Tim Shull. All rights reserved.
//

#include "CustomSineWavePlayer.h"


void CustomSineWavePlayer::check_error(OSStatus error, std::string operation) {
    if (error == noErr) return;

    char str[20];
    // see if it appears to be a 4-char-code
    *(str + 1) = static_cast<char>(CFSwapInt32HostToBig(static_cast<uint32_t>(error)));
    if (isprint(str[1]) && isprint(str[2]) && isprint(str[3]) && isprint(str[4])) {
        str[0] = str[5] = '\'';
        str[6] = '\0';
    } else
        std::cerr << error << std::endl;

    std::cerr << "Error: " << operation << "(" << str << ")" << std::endl;

    exit(1);
}

void CustomSineWavePlayer::init() {
    AudioComponentDescription outputcd = {0};
    outputcd.componentType = kAudioUnitType_Output;
    outputcd.componentSubType = kAudioUnitSubType_DefaultOutput;
    outputcd.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent comp = AudioComponentFindNext(nullptr, &outputcd);
    if (comp == nullptr) {
        std::cerr << "can't get output unit" << std::endl;
        exit(-1);
    }
    OSStatus err;
    err = AudioComponentInstanceNew(comp, &outputUnit);
    check_error(err, "Couldn't open component for outputUnit");

    // register render callback
    AURenderCallbackStruct input;
    input.inputProc = render;
    input.inputProcRefCon = this;
    err = AudioUnitSetProperty(outputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
            &input, sizeof(input));
    check_error(err, "AudioUnitSetProperty failed");

    // initialize unit
    err = AudioUnitInitialize(outputUnit);
    check_error(err, "Couldn't initialize output unit");
}

OSStatus render(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData) {
    CustomSineWavePlayer *player = (CustomSineWavePlayer *) inRefCon;

    double j = player->startingFrameCount;
    double cycleLength = 44100. / player->sineFrequency;
    for (int frame = 0; frame < inNumberFrames; ++frame) {
        Float32 *data = static_cast<Float32 *>(ioData->mBuffers[0].mData);
        data[frame] = static_cast<Float32>(sin(2 * M_PI * (j / cycleLength)));

        // copy to right channel too
        data = static_cast<Float32 *>(ioData->mBuffers[1].mData);
        data[frame] = static_cast<Float32>(sin(2 * M_PI * (j / cycleLength)));

        j += 1.0;
        if (j > cycleLength)
            j -= cycleLength;
    }

    player->startingFrameCount = j;
    return noErr;
}