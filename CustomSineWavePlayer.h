//
// Created by Tim Shull on 9/26/16.
// Copyright (c) 2016 Tim Shull. All rights reserved.
//

#ifndef AUDIOEXPERIMENTS_CUSTOMSINEWAVEPLAYER_H
#define AUDIOEXPERIMENTS_CUSTOMSINEWAVEPLAYER_H

#include <AudioToolbox/AudioToolbox.h>
#include <iostream>

OSStatus render(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,
        UInt32 inBusNumber,
        UInt32 inNumberFrames,
        AudioBufferList *ioData);

struct CustomSineWavePlayer {
    AudioUnit outputUnit;
    double startingFrameCount;
    double sineFrequency;

    CustomSineWavePlayer() : startingFrameCount{0}, outputUnit{0}, sineFrequency{440.0} {
    }

    CustomSineWavePlayer(double freq) : startingFrameCount{0}, outputUnit{0}, sineFrequency{freq} {
    }

    void init();

    static void check_error(OSStatus error, std::string operation);
};


#endif //AUDIOEXPERIMENTS_CUSTOMSINEWAVEPLAYER_H
