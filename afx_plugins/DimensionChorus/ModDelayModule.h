/*
	Socket(R) Rapid Plug-In Development (RaPID) Client
	Applications Programming Interface
	Derived Class Object Definition

	Your plug-in must implement the constructor,
	destructor and virtual Plug-In API Functions below.
*/

#pragma once

#include "pluginconstants.h"
#include "WTOscillator.h"
#include "DDLModule.h"
#include "plugin.h"


// abstract base class for DSP filters
class CModDelayModule : public CPlugIn {
public:    // Plug-In API Functions
    // Plug-In API Member Methods:
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
    // 6. joystickControlChange() occurs when the user moves a control.
    virtual bool __stdcall joystickControlChange(float fControlA, float fControlB, float fControlC, float fControlD,
                                                 float fACMix, float fBDMix);

    // 7. process buffers instead of Frames:
    // NOTE: set m_bWantBuffers = true to use this function
    virtual bool __stdcall processSocketAudioBuffer(float *pInputBuffer, float *pOutputBuffer, UINT uNumInputChannels,
                                                    UINT uNumOutputChannels, UINT uBufferSize);

    // 8. rocess buffers instead of Frames:
    // NOTE: set m_bWantVSTBuffers = true to use this function
    virtual bool __stdcall processVSTAudioBuffer(float **ppInputs, float **ppOutputs, UINT uNumChannels,
                                                 int uNumFrames);

    // 9. MIDI Note On Event
    virtual bool __stdcall midiNoteOn(UINT uChannel, UINT uMIDINote, UINT uVelocity);

    // 10. MIDI Note Off Event
    virtual bool __stdcall midiNoteOff(UINT uChannel, UINT uMIDINote, UINT uVelocity, bool bAllNotesOff);

    // 11. all MIDI messages -
    // NOTE: set m_bWantAllMIDIMessages true to get everything else (other than note on/off)
    virtual bool __stdcall midiMessage(unsigned char cChannel, unsigned char cStatus, unsigned char cData1,
                                       unsigned char cData2);

    // 12. initUI() is called only once from the constructor; you do not need to write or call it
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

    float m_fDDLOutput; // added for dimension chorus
    float m_fWetLevel;



    // END OF USER CODE -------------------------------------------------------------- //


    // ADDED BY SOCKET -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
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
    UINT m_uTZF;
    enum {
        OFF, ON
    };

    // **--0x1A7F--**
    // ------------------------------------------------------------------------------- //

};















