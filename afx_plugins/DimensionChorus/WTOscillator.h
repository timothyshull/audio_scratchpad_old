/*
	Socket(R) Rapid Plug-In Development (RaPID) Client
	Applications Programming Interface
	Derived Class Object Definition

	Your plug-in must implement the constructor,
	destructor and virtual Plug-In API Functions below.
*/

#pragma once

#include "pluginconstants.h"
#include "plugin.h"


// abstract base class for DSP filters
class CWTOscillator : public CPlugIn {
public:    // Plug-In API Functions
    // Plug-In API Member Methods:
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
    // Array for the Table
    float m_SinArray[1024];    // 1024 Point Sinusoid
    float m_SawtoothArray[1024];  // saw
    float m_TriangleArray[1024];    // tri
    float m_SquareArray[1024];    // sqr

    // band limited to 5 partials
    float m_SawtoothArray_BL5[1024];        // saw, BL = 5
    float m_TriangleArray_BL5[1024];    // tri, BL = 5
    float m_SquareArray_BL5[1024];    // sqr, BL = 5

    void doOscillate(float *pYn, float *pYqn);

    // current read location
    float m_fReadIndex;        // NOTE its a FLOAT!

    float m_fQuadPhaseReadIndex;        // NOTE its a FLOAT!

// invert output
    bool m_bInvert;

    // reset the read index
    void reset() {
        m_fReadIndex = 0.0;
        m_fQuadPhaseReadIndex = 256.0;    // 1/4 of our 1024 point buffer
    }

    // our inc value
    float m_f_inc;

    // our cooking function
    void cookFrequency();

    // our note on/off message
    bool m_bNoteOn;



    // END OF USER CODE -------------------------------------------------------------- //


    // ADDED BY SOCKET -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
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







