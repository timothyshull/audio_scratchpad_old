/*
	Socket(R) Rapid Plug-In Development (RaPID) Client
	Applications Programming Interface
	Derived Class Object Definition

	Your plug-in must implement the constructor,
	destructor and virtual Plug-In API Functions below.
*/

#pragma once

#include "pluginconstants.h"
#include "DDLModule.h"
#include "WTOscillator.h"
#include "ModDelayModule.h"
#include "plugin.h"


// abstract base class for DSP filters
class CDimensionChorus : public CPlugIn {
public:    // Plug-In API Functions
    // Plug-In API Member Methods:
    // The followung 5 methods must be impelemented for a meaningful Plug-In
    //
    // 1. One Time Initialization
    CDimensionChorus();

    // 2. One Time Destruction
    virtual ~CDimensionChorus(void);

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
    CModDelayModule m_ModDelayLeft;
    CModDelayModule m_ModDelayRight;

    CBiQuad m_HPFLeft;
    CBiQuad m_HPFRight;

    void cookBiquads();

    // function to transfer out variables to it and cook
    void updateModules(); // you could split this out into smaller functions


    // END OF USER CODE -------------------------------------------------------------- //


    // ADDED BY SOCKET -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
    //  **--0x07FD--**

    float m_fRate_Hz;
    float m_fDepth;
    float m_fWetMix;
    float m_fCrossMix;
    float m_fHPF_Fc;

    // **--0x1A7F--**
    // ------------------------------------------------------------------------------- //

};











