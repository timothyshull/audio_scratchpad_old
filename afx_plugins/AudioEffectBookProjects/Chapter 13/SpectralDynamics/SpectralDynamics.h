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

// abstract base class for RackAFX filters
class CSpectralDynamics : public CPlugIn {
public:
    // RackAFX Plug-In API Member Methods:
    // The followung 5 methods must be impelemented for a meaningful Plug-In
    //
    // 1. One Time Initialization
    CSpectralDynamics();

    // 2. One Time Destruction
    virtual ~CSpectralDynamics(void);

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
    // envelope detectors X2
    CEnvelopeDetector m_Left_LF_Detector;
    CEnvelopeDetector m_Right_LF_Detector;
    CEnvelopeDetector m_Left_HF_Detector;
    CEnvelopeDetector m_Right_HF_Detector;

    // calculate the compressor G(n) value from the Detector output
    float calcCompressorGain(float fDetectorValue, float fThreshold, float fRatio, float fKneeWidth, bool bLimit);

    // calculate the downward expander G(n) value from the Detector output
    float calcDownwardExpanderGain(float fDetectorValue, float fThreshold, float fRatio, float fKneeWidth, bool bGate);

    // our input filter banks X2
    CBiQuad m_LeftLPF;
    CBiQuad m_LeftHPF;
    CBiQuad m_RightLPF;
    CBiQuad m_RightHPF;

    // function to set all the filter cutoffs
    void calculateFilterBankCoeffs(float fCutoffFreq);

    // mono version
    float calculateMonoOutput(float fMonoInput);

    // stereo linked version
    void calculateStereoOutput(float fLeftInput, float fRightInput, float &fLeftOutput, float &fRightOutput);


    // END OF USER CODE -------------------------------------------------------------- //


    // ADDED BY RACKAFX -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
    //  **--0x07FD--**

    float m_fFilterBankCutoff;
    float m_HF_DetectorGain_dB;
    float m_HF_Threshold;
    float m_HF_Attack_mSec;
    float m_HF_Release_mSec;
    float m_HF_Ratio;
    float m_HF_MakeUpGain_dB;
    float m_fKneeWidth;
    float m_LF_DetectorGain_dB;
    float m_LF_Threshold;
    float m_LF_Attack_mSec;
    float m_LF_Release_mSec;
    float m_LF_Ratio;
    float m_LF_MakeUpGain_dB;
    UINT m_uProcessorType;
    enum {
        COMP, LIMIT, EXPAND, GATE
    };
    UINT m_uTimeConstant;
    enum {
        Digital, Analog
    };
    float m_fMeterLFIn;
    float m_fMeterHFIn;
    float m_fMeterLFGr;
    float m_fMeterHFGr;
    float m_fMeterLFOut;
    float m_fMeterHFOut;

    // **--0x1A7F--**
    // ------------------------------------------------------------------------------- //

};



