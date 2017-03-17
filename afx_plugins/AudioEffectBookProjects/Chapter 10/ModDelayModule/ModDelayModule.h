/*
	RackAFX(TM)
	Applications Programming Interface
	Derived Class Object Definition
	Copyright(c) Tritone Systems Inc. 2006-2012

	Your plug-in must implement the constructor,
	destructor and virtual Plug-In API Functions below.
*/

#pragma once

// base class
#include "plugin.h"
#include "DDLModule.h"
#include "WTOscillator.h"

// abstract base class for RackAFX filters
class CModDelayModule : public CPlugIn {
public:
    // RackAFX Plug-In API Member Methods:
    // The followung 5 methods must be impelemented for a meaningful Plug-In
    //
    // 1. One Time Initialization
    CModDelayModule();

    // 2. One Time Destruction
    virtual ~CModDelayModule(void);

    // 3. The Prepare For Play Function is called just before audio streams
    virtual bool __stdcall prepareForPlay();

    // 4. processAudioFrame() processes an audio input to create an audio output
    virtual bool __stdcall processAudioFrame(float *pInputBuffer, float *pOutputBuffer, UINT uNumInputChannels,
                                             UINT uNumOutputChannels);

    // 5. userInterfaceChange() occurs when the user moves a control.
    virtual bool __stdcall userInterfaceChange(int nControlIndex);


    // OPTIONAL ADVANCED METHODS ------------------------------------------------------------------------------------------------
    // These are more advanced; see the website for more details
    //
    // 6. initialize() is called once just after creation; if you need to use Plug-In -> Host methods
    //				   such as sendUpdateGUI(), you must do them here and NOT in the constructor
    virtual bool __stdcall initialize();

    // 7. joystickControlChange() occurs when the user moves a control.
    virtual bool __stdcall joystickControlChange(float fControlA, float fControlB, float fControlC, float fControlD,
                                                 float fACMix, float fBDMix);

    // 8. process buffers instead of Frames:
    // NOTE: set m_bWantBuffers = true to use this function
    virtual bool __stdcall processRackAFXAudioBuffer(float *pInputBuffer, float *pOutputBuffer, UINT uNumInputChannels,
                                                     UINT uNumOutputChannels, UINT uBufferSize);

    // 9. rocess buffers instead of Frames:
    // NOTE: set m_bWantVSTBuffers = true to use this function
    virtual bool __stdcall processVSTAudioBuffer(float **ppInputs, float **ppOutputs, UINT uNumChannels,
                                                 int uNumFrames);

    // 10. MIDI Note On Event
    virtual bool __stdcall midiNoteOn(UINT uChannel, UINT uMIDINote, UINT uVelocity);

    // 11. MIDI Note Off Event
    virtual bool __stdcall midiNoteOff(UINT uChannel, UINT uMIDINote, UINT uVelocity, bool bAllNotesOff);


    // 12. MIDI Modulation Wheel uModValue = 0 -> 127
    virtual bool __stdcall midiModWheel(UINT uChannel, UINT uModValue);

    // 13. MIDI Pitch Bend
    //					nActualPitchBendValue = -8192 -> 8191, 0 is center, corresponding to the 14-bit MIDI value
    //					fNormalizedPitchBendValue = -1.0 -> +1.0, 0 is at center by using only -8191 -> +8191
    virtual bool __stdcall midiPitchBend(UINT uChannel, int nActualPitchBendValue, float fNormalizedPitchBendValue);

    // 14. MIDI Timing Clock (Sunk to BPM) function called once per clock
    virtual bool __stdcall midiClock();


    // 15. all MIDI messages -
    // NOTE: set m_bWantAllMIDIMessages true to get everything else (other than note on/off)
    virtual bool __stdcall midiMessage(unsigned char cChannel, unsigned char cStatus, unsigned char cData1,
                                       unsigned char cData2);

    // 16. initUI() is called only once from the constructor; you do not need to write or call it. Do NOT modify this function
    virtual bool __stdcall initUI();


    // Add your code here: ----------------------------------------------------------- //
    CWTOscillator m_LFO;    // our LFO
    CDDLModule m_DDL;    // our delay line module

    // these will depend on the type of mod
    float m_fMinDelay_mSec;
    float m_fMaxDelay_mSec;

    // functions to update the member objects
    void updateLFO();

    void updateDDL();

    // cooking function for mod type
    void cookModType();

    // convert a LFO value to a delay offset value
    float calculateDelayOffset(float fLFOSample);

    // END OF USER CODE -------------------------------------------------------------- //


    // ADDED BY RACKAFX -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
    //  **--0x07FD--**

    float m_fModDepth_pct;
    float m_fModFrequency_Hz;
    float m_fFeedback_pct;
    float m_fChorusOffset;
    UINT m_uModType;
    enum {
        Flanger, Vibrato, Chorus
    };
    UINT m_uLFOType;
    enum {
        tri, sin
    };
    UINT m_uLFOPhase;
    enum {
        normal, quad, invert
    };

    // **--0x1A7F--**
    // ------------------------------------------------------------------------------- //

};




