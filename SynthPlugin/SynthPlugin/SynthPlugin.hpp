//
//  SynthPlugin.hpp
//  SynthPlugin
//
//  Created by Tim Shull on 9/25/16.
//  Copyright © 2016 Tim Shull. All rights reserved.
//

#ifndef SynthPlugin_hpp
#define SynthPlugin_hpp

#include "AUInstrumentBase.h"

static const UInt32 kNumNotes = 12;

struct TestNote : public SynthNote
{
    virtual ~TestNote() {}
    
    virtual bool Attack(const MusicDeviceNoteParams &inParams);
    virtual void Kill(UInt32 inFrame); // voice is being stolen.
    virtual void Release(UInt32 inFrame);
    virtual void FastRelease(UInt32 inFrame);
    virtual Float32 Amplitude() { return amp; } // used for finding quietest note for voice stealing.
    virtual OSStatus Render(UInt64 inAbsoluteSampleFrame, UInt32 inNumFrames, AudioBufferList** inBufferList, UInt32 inOutBusCount);
    
    double phase, amp, maxamp, susamp;
    double up_slope, decay_slope, dn_slope, fast_dn_slope;
};

class MyFirstSynth : public AUMonotimbralInstrumentBase
{
public:
    MyFirstSynth(ComponentInstance inComponentInstance);
    virtual ~MyFirstSynth();
    
    virtual OSStatus Initialize();
    virtual void Cleanup();
    virtual OSStatus Version() { return 0xFFFFFF; }
    
    virtual OSStatus GetParameterInfo(AudioUnitScope inScope,
                                      AudioUnitParameterID inParameterID,
                                      AudioUnitParameterInfo & outParameterInfo);
private:
    TestNote mTestNotes[kNumNotes];
};

#endif /* SynthPlugin_hpp */
