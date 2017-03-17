//
//  SynthPlugin.cpp
//  SynthPlugin
//
//  Created by Tim Shull on 9/25/16.
//  Copyright © 2016 Tim Shull. All rights reserved.
//

#include "SynthPlugin.hpp"



#include "AUBase.h"
#include "AU.h"

AUDIOCOMPONENT_ENTRY(AUMusicDeviceFactory, MyFirstSynth);

static const UInt32 kMaxActiveNotes = 8;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const double twopi = 2.0 * 3.14159265358979;

inline double pow5(double x) { double x2 = x*x; return x2*x2*x; }

#pragma mark MyFirstSynth Methods

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static const AudioUnitParameterID kGlobalVolumeParam = 0;
static const CFStringRef kGlobalVolumeName = CFSTR("Volume");

static const AudioUnitParameterID kGlobalAttackParam = 1;
static const CFStringRef kGlobalAttackName = CFSTR("Attack");

static const AudioUnitParameterID kGlobalDecayParam = 2;
static const CFStringRef kGlobalDecayName = CFSTR("Decay");

static const AudioUnitParameterID kGlobalSustainParam = 3;
static const CFStringRef kGlobalSustainName = CFSTR("Sustain");

static const AudioUnitParameterID kGlobalReleaseParam = 4;
static const CFStringRef kGlobalReleaseName = CFSTR("Release");

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// MyFirstSynth::MyFirstSynth
//
// This synth has No inputs, One output
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MyFirstSynth::MyFirstSynth(ComponentInstance inComponentInstance)
: AUMonotimbralInstrumentBase(inComponentInstance, 0, 1)
{
    CreateElements();
    Globals()->UseIndexedParameters(5); // we're only defining one param
    Globals()->SetParameter(kGlobalVolumeParam, 1.0);
    Globals()->SetParameter(kGlobalAttackParam, 0.01);
    Globals()->SetParameter(kGlobalDecayParam, 0.75);
    Globals()->SetParameter(kGlobalSustainParam, 0.5);
    Globals()->SetParameter(kGlobalReleaseParam, 0.75);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// MyFirstSynth::~MyFirstSynth
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
MyFirstSynth::~MyFirstSynth()
{
}


void MyFirstSynth::Cleanup()
{
#if DEBUG_PRINT
    printf("MyFirstSynth::Cleanup\n");
#endif
}

OSStatus MyFirstSynth::Initialize()
{
#if DEBUG_PRINT
    printf("->MyFirstSynth::Initialize\n");
#endif
    AUMonotimbralInstrumentBase::Initialize();
    
    SetNotes(kNumNotes, kMaxActiveNotes, mTestNotes, sizeof(TestNote));
#if DEBUG_PRINT
    printf("<-MyFirstSynth::Initialize\n");
#endif
    return noErr;
}

OSStatus MyFirstSynth::GetParameterInfo(AudioUnitScope inScope,
                                        AudioUnitParameterID inParameterID,
                                        AudioUnitParameterInfo & outParameterInfo)
{
    if (inParameterID == kGlobalVolumeParam) {
        if (inScope != kAudioUnitScope_Global) return kAudioUnitErr_InvalidScope;
        
        outParameterInfo.flags = SetAudioUnitParameterDisplayType (0, kAudioUnitParameterFlag_DisplaySquareRoot);
        outParameterInfo.flags += kAudioUnitParameterFlag_IsWritable;
        outParameterInfo.flags += kAudioUnitParameterFlag_IsReadable;
        
        AUBase::FillInParameterName (outParameterInfo, kGlobalVolumeName, false);
        outParameterInfo.unit = kAudioUnitParameterUnit_LinearGain;
        outParameterInfo.minValue = 0;
        outParameterInfo.maxValue = 1.0;
        outParameterInfo.defaultValue = 1.0;
        return noErr;
    }
    else if (inParameterID == kGlobalAttackParam) {
        if (inScope != kAudioUnitScope_Global) return kAudioUnitErr_InvalidScope;
        
        outParameterInfo.flags = SetAudioUnitParameterDisplayType (0, kAudioUnitParameterFlag_DisplaySquareRoot);
        outParameterInfo.flags += kAudioUnitParameterFlag_IsWritable;
        outParameterInfo.flags += kAudioUnitParameterFlag_IsReadable;
        
        AUBase::FillInParameterName (outParameterInfo, kGlobalAttackName, false);
        outParameterInfo.unit = kAudioUnitParameterUnit_Seconds;
        outParameterInfo.minValue = 0.001;
        outParameterInfo.maxValue = 3.0;
        outParameterInfo.defaultValue = 0.01;
        return noErr;
    }
    else if (inParameterID == kGlobalDecayParam) {
        if (inScope != kAudioUnitScope_Global) return kAudioUnitErr_InvalidScope;
        
        outParameterInfo.flags = SetAudioUnitParameterDisplayType (0, kAudioUnitParameterFlag_DisplaySquareRoot);
        outParameterInfo.flags += kAudioUnitParameterFlag_IsWritable;
        outParameterInfo.flags += kAudioUnitParameterFlag_IsReadable;
        
        AUBase::FillInParameterName (outParameterInfo, kGlobalDecayName, false);
        outParameterInfo.unit = kAudioUnitParameterUnit_Seconds;
        outParameterInfo.minValue = 0;
        outParameterInfo.maxValue = 3.0;
        outParameterInfo.defaultValue = 0.75;
        return noErr;
    }
    else if (inParameterID == kGlobalSustainParam) {
        if (inScope != kAudioUnitScope_Global) return kAudioUnitErr_InvalidScope;
        
        outParameterInfo.flags = SetAudioUnitParameterDisplayType (0, kAudioUnitParameterFlag_DisplaySquareRoot);
        outParameterInfo.flags += kAudioUnitParameterFlag_IsWritable;
        outParameterInfo.flags += kAudioUnitParameterFlag_IsReadable;
        
        AUBase::FillInParameterName (outParameterInfo, kGlobalSustainName, false);
        outParameterInfo.unit = kAudioUnitParameterUnit_LinearGain;
        outParameterInfo.minValue = 0;
        outParameterInfo.maxValue = 1.0;
        outParameterInfo.defaultValue = 0.5;
        return noErr;
    }
    else if (inParameterID == kGlobalReleaseParam) {
        if (inScope != kAudioUnitScope_Global) return kAudioUnitErr_InvalidScope;
        
        outParameterInfo.flags = SetAudioUnitParameterDisplayType (0, kAudioUnitParameterFlag_DisplaySquareRoot);
        outParameterInfo.flags += kAudioUnitParameterFlag_IsWritable;
        outParameterInfo.flags += kAudioUnitParameterFlag_IsReadable;
        
        AUBase::FillInParameterName (outParameterInfo, kGlobalReleaseName, false);
        outParameterInfo.unit = kAudioUnitParameterUnit_Seconds;
        outParameterInfo.minValue = 0.001;
        outParameterInfo.maxValue = 3.0;
        outParameterInfo.defaultValue = 0.75;
        return noErr;
    }
    else {
        return kAudioUnitErr_InvalidParameter;
    }
}


#pragma mark TestNote Methods

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

bool TestNote::Attack(const MusicDeviceNoteParams &inParams)
{
#if DEBUG_PRINT
    printf("TestNote::Attack %08X %d\n", this, GetState());
#endif
    double sampleRate = SampleRate();
    phase = 0.;
    amp = 0.;
    maxamp = 0.4 * pow(inParams.mVelocity/127., 3.);
    susamp = GetGlobalParameter(kGlobalSustainParam) * maxamp;
    up_slope = maxamp / (GetGlobalParameter(kGlobalAttackParam) * sampleRate);
    decay_slope = (susamp-maxamp) / (GetGlobalParameter(kGlobalDecayParam) * sampleRate);
    dn_slope = -maxamp / (GetGlobalParameter(kGlobalReleaseParam) * sampleRate);
    fast_dn_slope = -maxamp / (0.005 * sampleRate);
    
    return true;
}

void TestNote::Release(UInt32 inFrame)
{
    SynthNote::Release(inFrame);
#if DEBUG_PRINT
    printf("TestNote::Release %08X %d\n", this, GetState());
#endif
}

void TestNote::FastRelease(UInt32 inFrame) // voice is being stolen.
{
    SynthNote::Release(inFrame);
#if DEBUG_PRINT
    printf("TestNote::Release %08X %d\n", this, GetState());
#endif
}

void TestNote::Kill(UInt32 inFrame) // voice is being stolen.
{
    SynthNote::Kill(inFrame);
#if DEBUG_PRINT
    printf("TestNote::Kill %08X %d\n", this, GetState());
#endif
}

OSStatus TestNote::Render(UInt64 inAbsoluteSampleFrame, UInt32 inNumFrames, AudioBufferList** inBufferList, UInt32 inOutBusCount)
{
    float *left, *right;
    /* ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~
     Changes to this parameter (kGlobalVolumeParam) are not being de-zippered;
     Left as an exercise for the reader
     ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ */
    float globalVol = GetGlobalParameter(kGlobalVolumeParam);
    int numChans = inBufferList[0]->mNumberBuffers;
    if (numChans > 2) return -1;
    left = (float*)inBufferList[0]->mBuffers[0].mData;
    right = numChans == 2 ? (float*)inBufferList[0]->mBuffers[1].mData : 0;
    
    double sampleRate = SampleRate();
    double freq = Frequency() * (twopi/sampleRate);
    
#if DEBUG_PRINT_RENDER
    printf("TestNote::Render %08X %d %g %g\n", this, GetState(), phase, amp);
#endif
    SynthNoteState state = GetState();
    
    switch (state)
    {
        case kNoteState_Attacked :
        case kNoteState_Sostenutoed :
        case kNoteState_ReleasedButSostenutoed :
        case kNoteState_ReleasedButSustained :
        {
            for (UInt32 frame=0; frame<inNumFrames; ++frame)
            {
                if (amp < maxamp && state == kNoteState_Attacked) {
                    amp += up_slope;
                    
                    if (amp >= maxamp) {
                        SetState(kNoteState_Sostenutoed);
                    }
                }
                else if (amp >= susamp && state == kNoteState_Sostenutoed) {
                    amp += decay_slope;
                    dn_slope = -amp / (GetGlobalParameter(kGlobalReleaseParam) * sampleRate);
                }
                
                //float out = pow5(sin(phase)) * amp * globalVol;
                float out = (sin(phase) > 0 ? 1 : -1) * amp * globalVol;
                phase += freq;
                if (phase > twopi) phase -= twopi;
                left[frame] += out;
                if (right) right[frame] += out;
            }
        }
            break;
        case kNoteState_Released :
        {
            UInt32 endFrame = 0xFFFFFFFF;
            for (UInt32 frame=0; frame<inNumFrames; ++frame)
            {
                if (amp > 0.0) amp += dn_slope;
                else if (endFrame == 0xFFFFFFFF) endFrame = frame;
                //float out = pow5(sin(phase)) * amp * globalVol;
                float out = (sin(phase) > 0 ? 1 : -1) * amp * globalVol;
                phase += freq;
                left[frame] += out;
                if (right) right[frame] += out;
            }
            if (endFrame != 0xFFFFFFFF) {
#if DEBUG_PRINT
                printf("TestNote::NoteEnded  %08X %d %g %g\n", this, GetState(), phase, amp);
#endif
                NoteEnded(endFrame);
            }
        }
            break;
        case kNoteState_FastReleased :
        {
            UInt32 endFrame = 0xFFFFFFFF;
            for (UInt32 frame=0; frame<inNumFrames; ++frame)
            {
                if (amp > 0.0) amp += fast_dn_slope;
                else if (endFrame == 0xFFFFFFFF) endFrame = frame;
                float out = pow5(sin(phase)) * amp * globalVol;
                phase += freq;
                left[frame] += out;
                if (right) right[frame] += out;
            }
            if (endFrame != 0xFFFFFFFF) {
#if DEBUG_PRINT
                printf("TestNote::NoteEnded  %08X %d %g %g\n", this, GetState(), phase, amp);
#endif
                NoteEnded(endFrame);
            }
        }
            break;
        default :
            break;
    }
    return noErr;
}
