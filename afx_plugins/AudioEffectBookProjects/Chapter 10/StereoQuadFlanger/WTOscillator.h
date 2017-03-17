/*
	RackAFX(TM)
	Applicatios Programming Interface
	Derived Class Object Definition
	Copyright(c) Tritone Systems Inc. 2006-2012

	Your plug-in must implement the constructor,
	destructor and virtual Plug-In API Functions below.
*/

#pragma once

// base class
#include "plugin.h"

// abstract base class for RackAFX filters
class CWTOscillator : public CPlugIn {
public:
    // RackAFX Plug-In API Member Methods:
    // The followung 5 methods must be impelemented for a meaningful Plug-In
    //
    // 1. One Time Initialization
    CWTOscillator();

    // 2. One Time Destruction
    virtual ~CWTOscillator(void);

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
    //
    // Array for the Table
    float m_SinArray[1024];            // 1024 Point Sinusoid
    float m_SawtoothArray[1024];    // saw
    float m_TriangleArray[1024];    // tri
    float m_SquareArray[1024];        // sqr

    // band limited to 5 partials
    float m_SawtoothArray_BL5[1024];        // saw, BL = 5
    float m_TriangleArray_BL5[1024];        // tri, BL = 5
    float m_SquareArray_BL5[1024];            // sqr, BL = 5

    // current read location
    float m_fReadIndex;
    float m_fQuadPhaseReadIndex;        // NOTE its a FLOAT!

    // invert output
    bool m_bInvert;

    // reset the read index
    void reset() {
        m_fReadIndex = 0.0;            // top of buffer
        m_fQuadPhaseReadIndex = 256.0;    // 1/4 of our 1024 point buffer
    }

    // our inc value
    float m_f_inc;

    // our cooking function
    void cookFrequency();

    // our note on/off message
    bool m_bNoteOn;

    // funciton to do the Oscillator; may be called by an external parent Plug-In too
    //	pYn is the normal output
    //	pYqn is the quad phase output
    void doOscillate(float *pYn, float *pYqn);

    // END OF USER CODE -------------------------------------------------------------- //


    // ADDED BY RACKAFX -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
    //  **--0x07FD--**

    float m_fFrequency_Hz;
    UINT m_uOscType;
    enum {
        sine, saw, tri, square
    };
    UINT m_uTableMode;
    enum {
        normal, bandlimit
    };
    UINT m_uPolarity;
    enum {
        bipolar, unipolar
    };

    // **--0x1A7F--**
    // ------------------------------------------------------------------------------- //

};



