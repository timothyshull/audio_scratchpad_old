/*
	Socket(R) Rapid Plug-In Development (RaPID) Client
	Applications Programming Interface
	Derived Class Object Implementation
*/


#include "ModDelayModule.h"


/* constructor()
	You can initialize variables here.
	You can also allocate memory here as long is it does not
	require the plugin to be fully instantiated. If so, allocate in init()

*/
CModDelayModule::CModDelayModule() {
    // Added by Socket - DO NOT REMOVE
    //
    // Setup the Socket UI Control List and Initialize Variables
    initUI();
    // END InitUI

    // built in initialization
    m_PlugInName = "ModDelayModule";

    // Default to Stereo Operation:
    // Change this if you want to support more/less channels
    m_uMaxInputChannels = 2;
    m_uMaxOutputChannels = 2;

    // use of MIDI controllers to adjust sliders/knobs
    m_bEnableMIDIControl = true;        // by default this is enabled

    // custom GUI stuff
    m_bLinkGUIRowsAndButtons = false;    // change this if you want to force-link

    // DO NOT CHANGE let Socket change it for you
    m_bUseCustomVSTGUI = false;

    // output only - SYNTH - plugin DO NOT CHANGE let Socket change it for you
    m_bOutputOnlyPlugIn = false;

    // Finish initializations here
    m_fMinDelay_mSec = 0.0;
    m_fMaxDelay_mSec = 0.0;
    m_fChorusOffset = 0.0;

    m_LFO.m_fFrequency_Hz = 0;
    m_LFO.m_uOscType = m_uLFOType; // triangle	enum{sine,saw,tri,square};

    m_DDL.m_bUseExternalFeedback = false;
    m_DDL.m_fDelay_ms = 0;

    m_fDDLOutput = 0;

}


/* destructor()
	Destroy variables allocated in the contructor()

*/
CModDelayModule::~CModDelayModule(void) {


}

/* prepareForPlay()
	Called by the client after Play() is initiated but before audio streams

	You can perform buffer flushes and per-run intializations.
	You can check the following variables and use them if needed:

	m_nNumWAVEChannels;
	m_nSampleRate;
	m_nBitDepth;

	NOTE: the above values are only valid during prepareForPlay() and
		  processAudioFrame() because the user might change to another wave file,
		  or use the sound card, oscillators, or impulse response mechanisms

    NOTE: if you allocte memory in this function, destroy it in ::destroy() above
*/
bool __stdcall CModDelayModule::prepareForPlay() {
    // Add your code here:
    m_LFO.prepareForPlay();

    m_DDL.m_nSampleRate = m_nSampleRate;

    m_DDL.prepareForPlay();

    m_LFO.m_uPolarity = 1;   // 0 = bipolar, 1 = unipolar
    m_LFO.m_uTableMode = 0;  // normal, no band limiting
    m_LFO.reset();          // reset it

    // initialize
    cookModType();
    updateLFO();
    updateDDL();

    // start the LFO!
    m_LFO.m_bNoteOn = true;

    m_fDDLOutput = 0;


    return true;
}

// LFO function to set:
//			- the LFO Frequency
//			- the oscillator type
void CModDelayModule::updateLFO() {
    // set raw data
    m_LFO.m_fFrequency_Hz = m_fModFrequency_Hz;
    m_LFO.m_uOscType = m_uLFOType;

    // cook it
    m_LFO.cookFrequency();
}

// DDL function to set:
//			- the DDL Feedback amount (disabled for Vibrato)
void CModDelayModule::updateDDL() {
    // test and set if needed
    if (m_uModType != Vibrato)
        m_DDL.m_fFeedback_pct = m_fFeedback_pct;

    // m_DDL.m_fWetLevel will be set in cookVaribles() below
    m_DDL.m_fWetLevel_pct = m_fWetLevel;

    // cook it
    m_DDL.cookVariables();
}
// cookMod() function:
/*
		Min Delay (mSec)	Max delay (mSec)	Wet/Dry(%)	Feedback(%)
Flanger		0			7-10			50/50		-100 to +100
Vibrato		0			7-10			100/0		0
Chorus		5			20-40			50/50		-100 to +100

*/
void CModDelayModule::cookModType() {
    switch (m_uModType) {
        case Flanger: {
            if (m_uTZF == ON)
                m_fMinDelay_mSec = 0.0;
            else
                m_fMinDelay_mSec = 0.1;

            m_fMaxDelay_mSec = 7;
            m_DDL.m_fWetLevel_pct = 50.0;
            m_DDL.m_fFeedback_pct = m_fFeedback_pct;

            break;
        }

        case Vibrato: {
            m_fMinDelay_mSec = 0.0;
            m_fMaxDelay_mSec = 7;
            m_DDL.m_fWetLevel_pct = 100.0;
            m_DDL.m_fFeedback_pct = 0.0; // NOTE! no FB for vibrato
            break;
        }

        case Chorus: {
            m_fMinDelay_mSec = 5;
            m_fMaxDelay_mSec = 30;
            m_DDL.m_fWetLevel_pct = 50.0;
            m_DDL.m_fFeedback_pct = m_fFeedback_pct;
            break;
        }

        default: // is Flanger
        {
            if (m_uTZF == ON)
                m_fMinDelay_mSec = 0.0;
            else
                m_fMinDelay_mSec = 0.1;

            m_fMaxDelay_mSec = 7;
            m_DDL.m_fWetLevel_pct = 50.0;
            m_DDL.m_fFeedback_pct = m_fFeedback_pct;
            break;
        }
    }
}
// calculateDelayOffset():
/*
	fLFOSample: a value from 0.0 to 1.0 from the LFO object

	returns: the calculated delay time in mSec for each effect

	NOTES: - the range for the flanger/vibrato is simply mapped from min to max
		  starting at min: fLFOSample*(m_fMaxDelay_mSec - m_fMinDelay_mSec)) +
  m_fMinDelay_mSec

	       - the range for the Chorus includes the starting offset
		  fStart = m_fMinDelay_mSec + m_fChorusOffset;
*/
float CModDelayModule::calculateDelayOffset(float fLFOSample) {
    if (m_uModType == Flanger || m_uModType == Vibrato) {
        // flanger 0->1 gets mapped to 0->maxdelay
        //float f = (m_fModDepth_pct/100.0)*(fLFOSample*(m_fMaxDelay_mSec - m_fMinDelay_mSec)) + m_fMinDelay_mSec;
        //if(f < 0)
        //	this->sendStatusWndText("delay less than 0!!!");

        return (m_fModDepth_pct / 100.0) * (fLFOSample * (m_fMaxDelay_mSec - m_fMinDelay_mSec)) + m_fMinDelay_mSec;
    }
    else if (m_uModType == Chorus) {
        // chorus adds starting offset to move delay range
        float fStart = m_fMinDelay_mSec + m_fChorusOffset;

        return (m_fModDepth_pct / 100.0) * (fLFOSample * (m_fMaxDelay_mSec - m_fMinDelay_mSec)) + fStart;
    }

    return 0;
}

/* processAudioFrame

// ALL VALUES IN AND OUT ON THE RANGE OF -1.0 TO + 1.0

LEFT INPUT = pInputBuffer[0];
RIGHT INPUT = pInputBuffer[1]

LEFT INPUT = pInputBuffer[0]
LEFT OUTPUT = pOutputBuffer[1]

*/
bool __stdcall CModDelayModule::processAudioFrame(float *pInputBuffer, float *pOutputBuffer, UINT uNumInputChannels,
                                                  UINT uNumOutputChannels) {
    // Do LEFT (MONO) Channel
    //
    // 1. Get LFO Values, normal and quad phase
    float fYn = 0;
    float fYqn = 0;
    m_LFO.doOscillate(&fYn, &fYqn);

    // 2. calculate delay offset
    float fDelay = 0.0;
    if (m_uLFOPhase == quad)
        fDelay = calculateDelayOffset(fYqn); // quadrature LFO
    else if (m_uLFOPhase == invert)
        fDelay = calculateDelayOffset(-fYn + 1.0); // inverted LFO
    else
        fDelay = calculateDelayOffset(fYn); // normal LFO

    // 3. set the delay & cook
    m_DDL.m_fDelay_ms = fDelay;
    m_DDL.cookVariables();

    // 4. get the delay output one channel in/one channel out
    //
    m_DDL.processAudioFrame(&pInputBuffer[0], &pOutputBuffer[0], 1, 1);

    // save for dimension chorus
    m_fDDLOutput = m_DDL.m_fDDLOutput;

    // Mono-In, Stereo-Out (AUX Effect)
    if (uNumInputChannels == 1 && uNumOutputChannels == 2)
        pOutputBuffer[1] = pOutputBuffer[0];

    // Stereo-In, Stereo-Out (INSERT Effect)
    if (uNumInputChannels == 2 && uNumOutputChannels == 2)
        pOutputBuffer[1] = pOutputBuffer[0];

    return true;
}


/* ADDED BY SOCKET -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
   	**--0x2983--**

	Variable Name                    Index
-----------------------------------------------
	m_fModDepth_pct                   0
	m_fModFrequency_Hz                1
	m_fFeedback_pct                   2
	m_fChorusOffset                   3
	m_uModType                        41
	m_uLFOType                        42
	m_uLFOPhase                       43
	m_uTZF                            44

	Assignable Buttons               Index
-----------------------------------------------
	B1                                50
	B2                                51
	B3                                52

-----------------------------------------------
 JS_PROGRAM_CHANGE for a JoyStickProgramSwap (advanced)

-----------------------------------------------

	**--0xFFDD--**
// ------------------------------------------------------------------------------- */
// Add your UI Handler code here ------------------------------------------------- //
//
bool __stdcall CModDelayModule::userInterfaceChange(int nControlIndex) {
    // change the min/max limits; set wet/dry and Feedback
    if (nControlIndex == 41 || nControlIndex == 44) // 41 is mod type switch 44 = TZF
        cookModType();

    // else just call the other updates which handle all the rest
    //
    // frequency and LFO type
    updateLFO();

    // Wet/Dry and Feedback
    updateDDL();

    // decode the control index, or delete the switch and use brute force calls
    switch (nControlIndex) {
        case 0: {
            break;
        }

        default:
            break;
    }

    return true;
}


/* joystickControlChange

	Indicates the user moved the joystick point; the variables are the relative mixes
	of each axis; the values will add up to 1.0

			B
			|
		A -	x -	C
			|
			D

	The point in the very center (x) would be:
	fControlA = 0.25
	fControlB = 0.25
	fControlC = 0.25
	fControlD = 0.25

	AC Mix = projection on X Axis (0 -> 1)
	BD Mix = projection on Y Axis (0 -> 1)
*/
bool __stdcall CModDelayModule::joystickControlChange(float fControlA, float fControlB, float fControlC,
                                                      float fControlD, float fACMix, float fBDMix) {
    // add your code here

    return true;
}


/* processAudioBuffer

// ALL VALUES IN AND OUT ON THE RANGE OF -1.0 TO + 1.0

	The I/O buffers are interleaved depending on the number of channels. If uNumChannels = 2, then the
	buffer is L/R/L/R/L/R etc...

	if uNumChannels = 6 then the buffer is L/R/C/Sub/BL/BR etc...

	It is up to you to decode and de-interleave the data.

	For advanced users only!!
*/
bool __stdcall CModDelayModule::processSocketAudioBuffer(float *pInputBuffer, float *pOutputBuffer,
                                                         UINT uNumInputChannels, UINT uNumOutputChannels,
                                                         UINT uBufferSize) {

    for (UINT i = 0; i < uBufferSize; i++) {
        // pass through code
        pOutputBuffer[i] = pInputBuffer[i];
    }


    return true;
}


/* processVSTAudioBuffer

// ALL VALUES IN AND OUT ON THE RANGE OF -1.0 TO + 1.0

	The I/O buffers are interleaved depending on the number of channels. If uNumChannels = 2, then the
	buffer is L/R/L/R/L/R etc...

	if uNumChannels = 6 then the buffer is L/R/C/Sub/BL/BR etc...

	It is up to you to decode and de-interleave the data.

	The VST input and output buffers are pointers-to-pointers. The pp buffers are the same depth as uNumChannels, so
	if uNumChannels = 2, then ppInputs would contain two pointers,

		ppInputs[0] = a pointer to the LEFT buffer of data
		ppInputs[1] = a pointer to the RIGHT buffer of data

	Similarly, ppOutputs would have 2 pointers, one for left and one for right.

*/
bool __stdcall CModDelayModule::processVSTAudioBuffer(float **ppInputs, float **ppOutputs,
                                                      UINT uNumChannels, int uNumFrames) {
    // PASS Through example for Stereo interleaved data

    // MONO First
    float *in1 = ppInputs[0];
    float *out1 = ppOutputs[0];
    float *in2;
    float *out2;

    // if STEREO,
    if (uNumChannels == 2) {
        in2 = ppInputs[1];
        out2 = ppOutputs[1];
    }

    // loop and pass input to output by de-referencing ptrs
    while (--uNumFrames >= 0) {
        (*out1++) = (*in1++);

        if (uNumChannels == 2)
            (*out2++) = (*in2++);
    }

    // all OK
    return true;
}

bool __stdcall CModDelayModule::midiNoteOn(UINT uChannel, UINT uMIDINote, UINT uVelocity) {
    return true;
}

bool __stdcall CModDelayModule::midiNoteOff(UINT uChannel, UINT uMIDINote, UINT uVelocity, bool bAllNotesOff) {
    return true;
}

bool __stdcall CModDelayModule::midiMessage(unsigned char cChannel, unsigned char cStatus, unsigned char
cData1, unsigned char cData2) {
    return true;
}

// DO NOT DELETE THIS FUNCTION --------------------------------------------------- //
bool __stdcall CModDelayModule::initUI() {
// ADDED BY SOCKET -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
    if (m_UIControlList.count() > 0)
        return true;

// **--0xDEA7--**

    m_fModDepth_pct = 50.000000;
    CUICtrl ui0;
    ui0.uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
    ui0.uControlId = 0;
    ui0.fUserDisplayDataLoLimit = 0.000000;
    ui0.fUserDisplayDataHiLimit = 100.000000;
    ui0.uUserDataType = floatData;
    ui0.fInitUserIntValue = 0;
    ui0.fInitUserFloatValue = 50.000000;
    ui0.fInitUserDoubleValue = 0;
    ui0.fInitUserUINTValue = 0;
    ui0.m_pUserCookedIntData = NULL;
    ui0.m_pUserCookedFloatData = &m_fModDepth_pct;
    ui0.m_pUserCookedDoubleData = NULL;
    ui0.m_pUserCookedUINTData = NULL;
    ui0.cControlUnits = "%                                                               ";
    ui0.cVariableName = "m_fModDepth_pct";
    ui0.cEnumeratedList = "SEL1,SEL2,SEL3";
    ui0.dPresetData[0] = 50.000000;
    ui0.dPresetData[1] = 0.000000;
    ui0.dPresetData[2] = 0.000000;
    ui0.dPresetData[3] = 0.000000;
    ui0.dPresetData[4] = 0.000000;
    ui0.dPresetData[5] = 0.000000;
    ui0.dPresetData[6] = 0.000000;
    ui0.dPresetData[7] = 0.000000;
    ui0.dPresetData[8] = 0.000000;
    ui0.dPresetData[9] = 0.000000;
    ui0.dPresetData[10] = 0.000000;
    ui0.dPresetData[11] = 0.000000;
    ui0.dPresetData[12] = 0.000000;
    ui0.dPresetData[13] = 0.000000;
    ui0.dPresetData[14] = 0.000000;
    ui0.dPresetData[15] = 0.000000;
    ui0.cControlName = "Depth";
    ui0.bOwnerControl = false;
    ui0.bMIDIControl = false;
    ui0.uMIDIControlCommand = 176;
    ui0.uMIDIControlName = 3;
    ui0.uMIDIControlChannel = 0;
    ui0.nGUIRow = -1;
    ui0.nGUIColumn = -1;
    ui0.uControlTheme[0] = 0;
    ui0.uControlTheme[1] = 0;
    ui0.uControlTheme[2] = 0;
    ui0.uControlTheme[3] = 0;
    ui0.uControlTheme[4] = 0;
    ui0.uControlTheme[5] = 0;
    ui0.uControlTheme[6] = 0;
    ui0.uControlTheme[7] = 0;
    ui0.uControlTheme[8] = 0;
    ui0.uControlTheme[9] = 0;
    ui0.uControlTheme[10] = 0;
    ui0.uControlTheme[11] = 0;
    ui0.uControlTheme[12] = 0;
    ui0.uControlTheme[13] = 0;
    ui0.uControlTheme[14] = 0;
    ui0.uControlTheme[15] = 0;
    ui0.uControlTheme[16] = 0;
    ui0.uControlTheme[17] = 0;
    ui0.uControlTheme[18] = 0;
    ui0.uControlTheme[19] = 0;
    ui0.uControlTheme[20] = 0;
    ui0.uControlTheme[21] = 0;
    ui0.uControlTheme[22] = 0;
    ui0.uControlTheme[23] = 0;
    ui0.uControlTheme[24] = 0;
    ui0.uControlTheme[25] = 0;
    ui0.uControlTheme[26] = 0;
    ui0.uControlTheme[27] = 0;
    ui0.uControlTheme[28] = 0;
    ui0.uControlTheme[29] = 0;
    ui0.uControlTheme[30] = 0;
    ui0.uControlTheme[31] = 0;
    m_UIControlList.append(ui0);


    m_fModFrequency_Hz = 0.180000;
    CUICtrl ui1;
    ui1.uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
    ui1.uControlId = 1;
    ui1.fUserDisplayDataLoLimit = 0.020000;
    ui1.fUserDisplayDataHiLimit = 5.000000;
    ui1.uUserDataType = floatData;
    ui1.fInitUserIntValue = 0;
    ui1.fInitUserFloatValue = 0.180000;
    ui1.fInitUserDoubleValue = 0;
    ui1.fInitUserUINTValue = 0;
    ui1.m_pUserCookedIntData = NULL;
    ui1.m_pUserCookedFloatData = &m_fModFrequency_Hz;
    ui1.m_pUserCookedDoubleData = NULL;
    ui1.m_pUserCookedUINTData = NULL;
    ui1.cControlUnits = "Hz                                                              ";
    ui1.cVariableName = "m_fModFrequency_Hz";
    ui1.cEnumeratedList = "SEL1,SEL2,SEL3";
    ui1.dPresetData[0] = 0.180000;
    ui1.dPresetData[1] = 0.000000;
    ui1.dPresetData[2] = 0.000000;
    ui1.dPresetData[3] = 0.000000;
    ui1.dPresetData[4] = 0.000000;
    ui1.dPresetData[5] = 0.000000;
    ui1.dPresetData[6] = 0.000000;
    ui1.dPresetData[7] = 0.000000;
    ui1.dPresetData[8] = 0.000000;
    ui1.dPresetData[9] = 0.000000;
    ui1.dPresetData[10] = 0.000000;
    ui1.dPresetData[11] = 0.000000;
    ui1.dPresetData[12] = 0.000000;
    ui1.dPresetData[13] = 0.000000;
    ui1.dPresetData[14] = 0.000000;
    ui1.dPresetData[15] = 0.000000;
    ui1.cControlName = "Rate";
    ui1.bOwnerControl = false;
    ui1.bMIDIControl = false;
    ui1.uMIDIControlCommand = 176;
    ui1.uMIDIControlName = 3;
    ui1.uMIDIControlChannel = 0;
    ui1.nGUIRow = -1;
    ui1.nGUIColumn = -1;
    ui1.uControlTheme[0] = 0;
    ui1.uControlTheme[1] = 0;
    ui1.uControlTheme[2] = 0;
    ui1.uControlTheme[3] = 0;
    ui1.uControlTheme[4] = 0;
    ui1.uControlTheme[5] = 0;
    ui1.uControlTheme[6] = 0;
    ui1.uControlTheme[7] = 0;
    ui1.uControlTheme[8] = 0;
    ui1.uControlTheme[9] = 0;
    ui1.uControlTheme[10] = 0;
    ui1.uControlTheme[11] = 0;
    ui1.uControlTheme[12] = 0;
    ui1.uControlTheme[13] = 0;
    ui1.uControlTheme[14] = 0;
    ui1.uControlTheme[15] = 0;
    ui1.uControlTheme[16] = 0;
    ui1.uControlTheme[17] = 0;
    ui1.uControlTheme[18] = 0;
    ui1.uControlTheme[19] = 0;
    ui1.uControlTheme[20] = 0;
    ui1.uControlTheme[21] = 0;
    ui1.uControlTheme[22] = 0;
    ui1.uControlTheme[23] = 0;
    ui1.uControlTheme[24] = 0;
    ui1.uControlTheme[25] = 0;
    ui1.uControlTheme[26] = 0;
    ui1.uControlTheme[27] = 0;
    ui1.uControlTheme[28] = 0;
    ui1.uControlTheme[29] = 0;
    ui1.uControlTheme[30] = 0;
    ui1.uControlTheme[31] = 0;
    m_UIControlList.append(ui1);


    m_fFeedback_pct = 0.000000;
    CUICtrl ui2;
    ui2.uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
    ui2.uControlId = 2;
    ui2.fUserDisplayDataLoLimit = -100.000000;
    ui2.fUserDisplayDataHiLimit = 100.000000;
    ui2.uUserDataType = floatData;
    ui2.fInitUserIntValue = 0;
    ui2.fInitUserFloatValue = 0.000000;
    ui2.fInitUserDoubleValue = 0;
    ui2.fInitUserUINTValue = 0;
    ui2.m_pUserCookedIntData = NULL;
    ui2.m_pUserCookedFloatData = &m_fFeedback_pct;
    ui2.m_pUserCookedDoubleData = NULL;
    ui2.m_pUserCookedUINTData = NULL;
    ui2.cControlUnits = "%                                                               ";
    ui2.cVariableName = "m_fFeedback_pct";
    ui2.cEnumeratedList = "SEL1,SEL2,SEL3";
    ui2.dPresetData[0] = 0.000000;
    ui2.dPresetData[1] = 0.000000;
    ui2.dPresetData[2] = 0.000000;
    ui2.dPresetData[3] = 0.000000;
    ui2.dPresetData[4] = 0.000000;
    ui2.dPresetData[5] = 0.000000;
    ui2.dPresetData[6] = 0.000000;
    ui2.dPresetData[7] = 0.000000;
    ui2.dPresetData[8] = 0.000000;
    ui2.dPresetData[9] = 0.000000;
    ui2.dPresetData[10] = 0.000000;
    ui2.dPresetData[11] = 0.000000;
    ui2.dPresetData[12] = 0.000000;
    ui2.dPresetData[13] = 0.000000;
    ui2.dPresetData[14] = 0.000000;
    ui2.dPresetData[15] = 0.000000;
    ui2.cControlName = "Feedback";
    ui2.bOwnerControl = false;
    ui2.bMIDIControl = false;
    ui2.uMIDIControlCommand = 176;
    ui2.uMIDIControlName = 3;
    ui2.uMIDIControlChannel = 0;
    ui2.nGUIRow = -1;
    ui2.nGUIColumn = -1;
    ui2.uControlTheme[0] = 0;
    ui2.uControlTheme[1] = 0;
    ui2.uControlTheme[2] = 0;
    ui2.uControlTheme[3] = 0;
    ui2.uControlTheme[4] = 0;
    ui2.uControlTheme[5] = 0;
    ui2.uControlTheme[6] = 0;
    ui2.uControlTheme[7] = 0;
    ui2.uControlTheme[8] = 0;
    ui2.uControlTheme[9] = 0;
    ui2.uControlTheme[10] = 0;
    ui2.uControlTheme[11] = 0;
    ui2.uControlTheme[12] = 0;
    ui2.uControlTheme[13] = 0;
    ui2.uControlTheme[14] = 0;
    ui2.uControlTheme[15] = 0;
    ui2.uControlTheme[16] = 0;
    ui2.uControlTheme[17] = 0;
    ui2.uControlTheme[18] = 0;
    ui2.uControlTheme[19] = 0;
    ui2.uControlTheme[20] = 0;
    ui2.uControlTheme[21] = 0;
    ui2.uControlTheme[22] = 0;
    ui2.uControlTheme[23] = 0;
    ui2.uControlTheme[24] = 0;
    ui2.uControlTheme[25] = 0;
    ui2.uControlTheme[26] = 0;
    ui2.uControlTheme[27] = 0;
    ui2.uControlTheme[28] = 0;
    ui2.uControlTheme[29] = 0;
    ui2.uControlTheme[30] = 0;
    ui2.uControlTheme[31] = 0;
    m_UIControlList.append(ui2);


    m_fChorusOffset = 0.000000;
    CUICtrl ui3;
    ui3.uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
    ui3.uControlId = 3;
    ui3.fUserDisplayDataLoLimit = 0.000000;
    ui3.fUserDisplayDataHiLimit = 30.000000;
    ui3.uUserDataType = floatData;
    ui3.fInitUserIntValue = 0;
    ui3.fInitUserFloatValue = 0.000000;
    ui3.fInitUserDoubleValue = 0;
    ui3.fInitUserUINTValue = 0;
    ui3.m_pUserCookedIntData = NULL;
    ui3.m_pUserCookedFloatData = &m_fChorusOffset;
    ui3.m_pUserCookedDoubleData = NULL;
    ui3.m_pUserCookedUINTData = NULL;
    ui3.cControlUnits = "mSec                                                            ";
    ui3.cVariableName = "m_fChorusOffset";
    ui3.cEnumeratedList = "SEL1,SEL2,SEL3";
    ui3.dPresetData[0] = 0.000000;
    ui3.dPresetData[1] = 0.000000;
    ui3.dPresetData[2] = 0.000000;
    ui3.dPresetData[3] = 0.000000;
    ui3.dPresetData[4] = 0.000000;
    ui3.dPresetData[5] = 0.000000;
    ui3.dPresetData[6] = 0.000000;
    ui3.dPresetData[7] = 0.000000;
    ui3.dPresetData[8] = 0.000000;
    ui3.dPresetData[9] = 0.000000;
    ui3.dPresetData[10] = 0.000000;
    ui3.dPresetData[11] = 0.000000;
    ui3.dPresetData[12] = 0.000000;
    ui3.dPresetData[13] = 0.000000;
    ui3.dPresetData[14] = 0.000000;
    ui3.dPresetData[15] = 0.000000;
    ui3.cControlName = "Chorus Offset";
    ui3.bOwnerControl = false;
    ui3.bMIDIControl = false;
    ui3.uMIDIControlCommand = 176;
    ui3.uMIDIControlName = 3;
    ui3.uMIDIControlChannel = 0;
    ui3.nGUIRow = -1;
    ui3.nGUIColumn = -1;
    ui3.uControlTheme[0] = 0;
    ui3.uControlTheme[1] = 0;
    ui3.uControlTheme[2] = 0;
    ui3.uControlTheme[3] = 0;
    ui3.uControlTheme[4] = 0;
    ui3.uControlTheme[5] = 0;
    ui3.uControlTheme[6] = 0;
    ui3.uControlTheme[7] = 0;
    ui3.uControlTheme[8] = 0;
    ui3.uControlTheme[9] = 0;
    ui3.uControlTheme[10] = 0;
    ui3.uControlTheme[11] = 0;
    ui3.uControlTheme[12] = 0;
    ui3.uControlTheme[13] = 0;
    ui3.uControlTheme[14] = 0;
    ui3.uControlTheme[15] = 0;
    ui3.uControlTheme[16] = 0;
    ui3.uControlTheme[17] = 0;
    ui3.uControlTheme[18] = 0;
    ui3.uControlTheme[19] = 0;
    ui3.uControlTheme[20] = 0;
    ui3.uControlTheme[21] = 0;
    ui3.uControlTheme[22] = 0;
    ui3.uControlTheme[23] = 0;
    ui3.uControlTheme[24] = 0;
    ui3.uControlTheme[25] = 0;
    ui3.uControlTheme[26] = 0;
    ui3.uControlTheme[27] = 0;
    ui3.uControlTheme[28] = 0;
    ui3.uControlTheme[29] = 0;
    ui3.uControlTheme[30] = 0;
    ui3.uControlTheme[31] = 0;
    m_UIControlList.append(ui3);


    m_uModType = 0;
    CUICtrl ui4;
    ui4.uControlType = FILTER_CONTROL_RADIO_SWITCH_VARIABLE;
    ui4.uControlId = 41;
    ui4.fUserDisplayDataLoLimit = 0.000000;
    ui4.fUserDisplayDataHiLimit = 2.000000;
    ui4.uUserDataType = UINTData;
    ui4.fInitUserIntValue = 0;
    ui4.fInitUserFloatValue = 0;
    ui4.fInitUserDoubleValue = 0;
    ui4.fInitUserUINTValue = 0.000000;
    ui4.m_pUserCookedIntData = NULL;
    ui4.m_pUserCookedFloatData = NULL;
    ui4.m_pUserCookedDoubleData = NULL;
    ui4.m_pUserCookedUINTData = &m_uModType;
    ui4.cControlUnits = "";
    ui4.cVariableName = "m_uModType";
    ui4.cEnumeratedList = "Flanger,Vibrato,Chorus";
    ui4.dPresetData[0] = 0.000000;
    ui4.dPresetData[1] = 0.000000;
    ui4.dPresetData[2] = 0.000000;
    ui4.dPresetData[3] = 0.000000;
    ui4.dPresetData[4] = 0.000000;
    ui4.dPresetData[5] = 0.000000;
    ui4.dPresetData[6] = 0.000000;
    ui4.dPresetData[7] = 0.000000;
    ui4.dPresetData[8] = 0.000000;
    ui4.dPresetData[9] = 0.000000;
    ui4.dPresetData[10] = 0.000000;
    ui4.dPresetData[11] = 0.000000;
    ui4.dPresetData[12] = 0.000000;
    ui4.dPresetData[13] = 0.000000;
    ui4.dPresetData[14] = 0.000000;
    ui4.dPresetData[15] = 0.000000;
    ui4.cControlName = "Mod Type";
    ui4.bOwnerControl = false;
    ui4.bMIDIControl = false;
    ui4.uMIDIControlCommand = 176;
    ui4.uMIDIControlName = 3;
    ui4.uMIDIControlChannel = 0;
    ui4.nGUIRow = -1;
    ui4.nGUIColumn = -1;
    ui4.uControlTheme[0] = 0;
    ui4.uControlTheme[1] = 0;
    ui4.uControlTheme[2] = 0;
    ui4.uControlTheme[3] = 0;
    ui4.uControlTheme[4] = 0;
    ui4.uControlTheme[5] = 0;
    ui4.uControlTheme[6] = 0;
    ui4.uControlTheme[7] = 0;
    ui4.uControlTheme[8] = 0;
    ui4.uControlTheme[9] = 0;
    ui4.uControlTheme[10] = 0;
    ui4.uControlTheme[11] = 0;
    ui4.uControlTheme[12] = 0;
    ui4.uControlTheme[13] = 0;
    ui4.uControlTheme[14] = 0;
    ui4.uControlTheme[15] = 0;
    ui4.uControlTheme[16] = 0;
    ui4.uControlTheme[17] = 0;
    ui4.uControlTheme[18] = 0;
    ui4.uControlTheme[19] = 0;
    ui4.uControlTheme[20] = 0;
    ui4.uControlTheme[21] = 0;
    ui4.uControlTheme[22] = 0;
    ui4.uControlTheme[23] = 0;
    ui4.uControlTheme[24] = 0;
    ui4.uControlTheme[25] = 0;
    ui4.uControlTheme[26] = 0;
    ui4.uControlTheme[27] = 0;
    ui4.uControlTheme[28] = 0;
    ui4.uControlTheme[29] = 0;
    ui4.uControlTheme[30] = 0;
    ui4.uControlTheme[31] = 0;
    m_UIControlList.append(ui4);


    m_uLFOType = 0;
    CUICtrl ui5;
    ui5.uControlType = FILTER_CONTROL_RADIO_SWITCH_VARIABLE;
    ui5.uControlId = 42;
    ui5.fUserDisplayDataLoLimit = 0.000000;
    ui5.fUserDisplayDataHiLimit = 1.000000;
    ui5.uUserDataType = UINTData;
    ui5.fInitUserIntValue = 0;
    ui5.fInitUserFloatValue = 0;
    ui5.fInitUserDoubleValue = 0;
    ui5.fInitUserUINTValue = 0.000000;
    ui5.m_pUserCookedIntData = NULL;
    ui5.m_pUserCookedFloatData = NULL;
    ui5.m_pUserCookedDoubleData = NULL;
    ui5.m_pUserCookedUINTData = &m_uLFOType;
    ui5.cControlUnits = "";
    ui5.cVariableName = "m_uLFOType";
    ui5.cEnumeratedList = "tri,sin";
    ui5.dPresetData[0] = 0.000000;
    ui5.dPresetData[1] = 0.000000;
    ui5.dPresetData[2] = 0.000000;
    ui5.dPresetData[3] = 0.000000;
    ui5.dPresetData[4] = 0.000000;
    ui5.dPresetData[5] = 0.000000;
    ui5.dPresetData[6] = 0.000000;
    ui5.dPresetData[7] = 0.000000;
    ui5.dPresetData[8] = 0.000000;
    ui5.dPresetData[9] = 0.000000;
    ui5.dPresetData[10] = 0.000000;
    ui5.dPresetData[11] = 0.000000;
    ui5.dPresetData[12] = 0.000000;
    ui5.dPresetData[13] = 0.000000;
    ui5.dPresetData[14] = 0.000000;
    ui5.dPresetData[15] = 0.000000;
    ui5.cControlName = "LFO";
    ui5.bOwnerControl = false;
    ui5.bMIDIControl = false;
    ui5.uMIDIControlCommand = 176;
    ui5.uMIDIControlName = 3;
    ui5.uMIDIControlChannel = 0;
    ui5.nGUIRow = -1;
    ui5.nGUIColumn = -1;
    ui5.uControlTheme[0] = 0;
    ui5.uControlTheme[1] = 0;
    ui5.uControlTheme[2] = 0;
    ui5.uControlTheme[3] = 0;
    ui5.uControlTheme[4] = 0;
    ui5.uControlTheme[5] = 0;
    ui5.uControlTheme[6] = 0;
    ui5.uControlTheme[7] = 0;
    ui5.uControlTheme[8] = 0;
    ui5.uControlTheme[9] = 0;
    ui5.uControlTheme[10] = 0;
    ui5.uControlTheme[11] = 0;
    ui5.uControlTheme[12] = 0;
    ui5.uControlTheme[13] = 0;
    ui5.uControlTheme[14] = 0;
    ui5.uControlTheme[15] = 0;
    ui5.uControlTheme[16] = 0;
    ui5.uControlTheme[17] = 0;
    ui5.uControlTheme[18] = 0;
    ui5.uControlTheme[19] = 0;
    ui5.uControlTheme[20] = 0;
    ui5.uControlTheme[21] = 0;
    ui5.uControlTheme[22] = 0;
    ui5.uControlTheme[23] = 0;
    ui5.uControlTheme[24] = 0;
    ui5.uControlTheme[25] = 0;
    ui5.uControlTheme[26] = 0;
    ui5.uControlTheme[27] = 0;
    ui5.uControlTheme[28] = 0;
    ui5.uControlTheme[29] = 0;
    ui5.uControlTheme[30] = 0;
    ui5.uControlTheme[31] = 0;
    m_UIControlList.append(ui5);


    m_uLFOPhase = 0;
    CUICtrl ui6;
    ui6.uControlType = FILTER_CONTROL_RADIO_SWITCH_VARIABLE;
    ui6.uControlId = 43;
    ui6.fUserDisplayDataLoLimit = 0.000000;
    ui6.fUserDisplayDataHiLimit = 2.000000;
    ui6.uUserDataType = UINTData;
    ui6.fInitUserIntValue = 0;
    ui6.fInitUserFloatValue = 0;
    ui6.fInitUserDoubleValue = 0;
    ui6.fInitUserUINTValue = 0.000000;
    ui6.m_pUserCookedIntData = NULL;
    ui6.m_pUserCookedFloatData = NULL;
    ui6.m_pUserCookedDoubleData = NULL;
    ui6.m_pUserCookedUINTData = &m_uLFOPhase;
    ui6.cControlUnits = "";
    ui6.cVariableName = "m_uLFOPhase";
    ui6.cEnumeratedList = "normal,quad,invert";
    ui6.dPresetData[0] = 0.000000;
    ui6.dPresetData[1] = 0.000000;
    ui6.dPresetData[2] = 0.000000;
    ui6.dPresetData[3] = 0.000000;
    ui6.dPresetData[4] = 0.000000;
    ui6.dPresetData[5] = 0.000000;
    ui6.dPresetData[6] = 0.000000;
    ui6.dPresetData[7] = 0.000000;
    ui6.dPresetData[8] = 0.000000;
    ui6.dPresetData[9] = 0.000000;
    ui6.dPresetData[10] = 0.000000;
    ui6.dPresetData[11] = 0.000000;
    ui6.dPresetData[12] = 0.000000;
    ui6.dPresetData[13] = 0.000000;
    ui6.dPresetData[14] = 0.000000;
    ui6.dPresetData[15] = 0.000000;
    ui6.cControlName = "Phase";
    ui6.bOwnerControl = false;
    ui6.bMIDIControl = false;
    ui6.uMIDIControlCommand = 176;
    ui6.uMIDIControlName = 3;
    ui6.uMIDIControlChannel = 0;
    ui6.nGUIRow = -1;
    ui6.nGUIColumn = -1;
    ui6.uControlTheme[0] = 0;
    ui6.uControlTheme[1] = 0;
    ui6.uControlTheme[2] = 0;
    ui6.uControlTheme[3] = 0;
    ui6.uControlTheme[4] = 0;
    ui6.uControlTheme[5] = 0;
    ui6.uControlTheme[6] = 0;
    ui6.uControlTheme[7] = 0;
    ui6.uControlTheme[8] = 0;
    ui6.uControlTheme[9] = 0;
    ui6.uControlTheme[10] = 0;
    ui6.uControlTheme[11] = 0;
    ui6.uControlTheme[12] = 0;
    ui6.uControlTheme[13] = 0;
    ui6.uControlTheme[14] = 0;
    ui6.uControlTheme[15] = 0;
    ui6.uControlTheme[16] = 0;
    ui6.uControlTheme[17] = 0;
    ui6.uControlTheme[18] = 0;
    ui6.uControlTheme[19] = 0;
    ui6.uControlTheme[20] = 0;
    ui6.uControlTheme[21] = 0;
    ui6.uControlTheme[22] = 0;
    ui6.uControlTheme[23] = 0;
    ui6.uControlTheme[24] = 0;
    ui6.uControlTheme[25] = 0;
    ui6.uControlTheme[26] = 0;
    ui6.uControlTheme[27] = 0;
    ui6.uControlTheme[28] = 0;
    ui6.uControlTheme[29] = 0;
    ui6.uControlTheme[30] = 0;
    ui6.uControlTheme[31] = 0;
    m_UIControlList.append(ui6);


    m_uTZF = 0;
    CUICtrl ui7;
    ui7.uControlType = FILTER_CONTROL_RADIO_SWITCH_VARIABLE;
    ui7.uControlId = 44;
    ui7.fUserDisplayDataLoLimit = 0.000000;
    ui7.fUserDisplayDataHiLimit = 1.000000;
    ui7.uUserDataType = UINTData;
    ui7.fInitUserIntValue = 0;
    ui7.fInitUserFloatValue = 0;
    ui7.fInitUserDoubleValue = 0;
    ui7.fInitUserUINTValue = 0.000000;
    ui7.m_pUserCookedIntData = NULL;
    ui7.m_pUserCookedFloatData = NULL;
    ui7.m_pUserCookedDoubleData = NULL;
    ui7.m_pUserCookedUINTData = &m_uTZF;
    ui7.cControlUnits = "";
    ui7.cVariableName = "m_uTZF";
    ui7.cEnumeratedList = "OFF,ON";
    ui7.dPresetData[0] = 0.000000;
    ui7.dPresetData[1] = 0.000000;
    ui7.dPresetData[2] = 0.000000;
    ui7.dPresetData[3] = 0.000000;
    ui7.dPresetData[4] = 0.000000;
    ui7.dPresetData[5] = 0.000000;
    ui7.dPresetData[6] = 0.000000;
    ui7.dPresetData[7] = 0.000000;
    ui7.dPresetData[8] = 0.000000;
    ui7.dPresetData[9] = 0.000000;
    ui7.dPresetData[10] = 0.000000;
    ui7.dPresetData[11] = 0.000000;
    ui7.dPresetData[12] = 0.000000;
    ui7.dPresetData[13] = 0.000000;
    ui7.dPresetData[14] = 0.000000;
    ui7.dPresetData[15] = 0.000000;
    ui7.cControlName = "TZF";
    ui7.bOwnerControl = false;
    ui7.bMIDIControl = false;
    ui7.uMIDIControlCommand = 176;
    ui7.uMIDIControlName = 3;
    ui7.uMIDIControlChannel = 0;
    ui7.nGUIRow = -1;
    ui7.nGUIColumn = -1;
    ui7.uControlTheme[0] = 0;
    ui7.uControlTheme[1] = 0;
    ui7.uControlTheme[2] = 0;
    ui7.uControlTheme[3] = 0;
    ui7.uControlTheme[4] = 0;
    ui7.uControlTheme[5] = 0;
    ui7.uControlTheme[6] = 0;
    ui7.uControlTheme[7] = 0;
    ui7.uControlTheme[8] = 0;
    ui7.uControlTheme[9] = 0;
    ui7.uControlTheme[10] = 0;
    ui7.uControlTheme[11] = 0;
    ui7.uControlTheme[12] = 0;
    ui7.uControlTheme[13] = 0;
    ui7.uControlTheme[14] = 0;
    ui7.uControlTheme[15] = 0;
    ui7.uControlTheme[16] = 0;
    ui7.uControlTheme[17] = 0;
    ui7.uControlTheme[18] = 0;
    ui7.uControlTheme[19] = 0;
    ui7.uControlTheme[20] = 0;
    ui7.uControlTheme[21] = 0;
    ui7.uControlTheme[22] = 0;
    ui7.uControlTheme[23] = 0;
    ui7.uControlTheme[24] = 0;
    ui7.uControlTheme[25] = 0;
    ui7.uControlTheme[26] = 0;
    ui7.uControlTheme[27] = 0;
    ui7.uControlTheme[28] = 0;
    ui7.uControlTheme[29] = 0;
    ui7.uControlTheme[30] = 0;
    ui7.uControlTheme[31] = 0;
    m_UIControlList.append(ui7);


    m_uX_TrackPadIndex = -1;
    m_uY_TrackPadIndex = -1;

    m_AssignButton1Name = "B1";
    m_AssignButton2Name = "B2";
    m_AssignButton3Name = "B3";

    m_bLatchingAssignButton1 = false;
    m_bLatchingAssignButton2 = false;
    m_bLatchingAssignButton3 = false;

    m_nGUIType = -1;
    m_nGUIThemeID = -1;
    m_bUseCustomVSTGUI = false;

    m_uControlTheme[0] = 0;
    m_uControlTheme[1] = 0;
    m_uControlTheme[2] = 0;
    m_uControlTheme[3] = 0;
    m_uControlTheme[4] = 0;
    m_uControlTheme[5] = 0;
    m_uControlTheme[6] = 0;
    m_uControlTheme[7] = 0;
    m_uControlTheme[8] = 0;
    m_uControlTheme[9] = 0;
    m_uControlTheme[10] = 0;
    m_uControlTheme[11] = 0;
    m_uControlTheme[12] = 0;
    m_uControlTheme[13] = 0;
    m_uControlTheme[14] = 0;
    m_uControlTheme[15] = 0;
    m_uControlTheme[16] = 0;
    m_uControlTheme[17] = 0;
    m_uControlTheme[18] = 0;
    m_uControlTheme[19] = 0;
    m_uControlTheme[20] = 0;
    m_uControlTheme[21] = 0;
    m_uControlTheme[22] = 0;
    m_uControlTheme[23] = 0;
    m_uControlTheme[24] = 0;
    m_uControlTheme[25] = 0;
    m_uControlTheme[26] = 0;
    m_uControlTheme[27] = 0;
    m_uControlTheme[28] = 0;
    m_uControlTheme[29] = 0;
    m_uControlTheme[30] = 0;
    m_uControlTheme[31] = 0;
    m_uControlTheme[32] = 0;
    m_uControlTheme[33] = 0;
    m_uControlTheme[34] = 0;
    m_uControlTheme[35] = 0;
    m_uControlTheme[36] = 0;
    m_uControlTheme[37] = 0;
    m_uControlTheme[38] = 0;
    m_uControlTheme[39] = 0;
    m_uControlTheme[40] = 0;
    m_uControlTheme[41] = 0;
    m_uControlTheme[42] = 0;
    m_uControlTheme[43] = 0;
    m_uControlTheme[44] = 0;
    m_uControlTheme[45] = 0;
    m_uControlTheme[46] = 0;
    m_uControlTheme[47] = 0;
    m_uControlTheme[48] = 0;
    m_uControlTheme[49] = 0;
    m_uControlTheme[50] = 0;
    m_uControlTheme[51] = 0;
    m_uControlTheme[52] = 0;
    m_uControlTheme[53] = 0;
    m_uControlTheme[54] = 0;
    m_uControlTheme[55] = 0;
    m_uControlTheme[56] = 0;
    m_uControlTheme[57] = 0;
    m_uControlTheme[58] = 0;
    m_uControlTheme[59] = 0;
    m_uControlTheme[60] = 0;
    m_uControlTheme[61] = 0;
    m_uControlTheme[62] = 0;
    m_uControlTheme[63] = 0;


    m_pVectorJSProgram[JS_PROG_INDEX(0, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(0, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(0, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(0, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(0, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(0, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(0, 6)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(1, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(1, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(1, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(1, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(1, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(1, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(1, 6)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(2, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(2, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(2, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(2, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(2, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(2, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(2, 6)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(3, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(3, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(3, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(3, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(3, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(3, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(3, 6)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(4, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(4, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(4, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(4, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(4, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(4, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(4, 6)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(5, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(5, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(5, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(5, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(5, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(5, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(5, 6)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(6, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(6, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(6, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(6, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(6, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(6, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(6, 6)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(7, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(7, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(7, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(7, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(7, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(7, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(7, 6)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(8, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(8, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(8, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(8, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(8, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(8, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(8, 6)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(9, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(9, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(9, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(9, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(9, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(9, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(9, 6)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(10, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(10, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(10, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(10, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(10, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(10, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(10, 6)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(11, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(11, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(11, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(11, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(11, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(11, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(11, 6)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(12, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(12, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(12, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(12, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(12, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(12, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(12, 6)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(13, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(13, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(13, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(13, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(13, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(13, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(13, 6)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(14, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(14, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(14, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(14, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(14, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(14, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(14, 6)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(15, 0)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(15, 1)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(15, 2)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(15, 3)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(15, 4)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(15, 5)] = 0.0000;
    m_pVectorJSProgram[JS_PROG_INDEX(15, 6)] = 0.0000;


    m_JS_XCtrl.cControlName = "MIDI JS X";
    m_JS_XCtrl.uControlId = 0;
    m_JS_XCtrl.bMIDIControl = false;
    m_JS_XCtrl.uMIDIControlCommand = 176;
    m_JS_XCtrl.uMIDIControlName = 16;
    m_JS_XCtrl.uMIDIControlChannel = 0;


    m_JS_YCtrl.cControlName = "MIDI JS Y";
    m_JS_YCtrl.uControlId = 0;
    m_JS_YCtrl.bMIDIControl = false;
    m_JS_YCtrl.uMIDIControlCommand = 176;
    m_JS_YCtrl.uMIDIControlName = 17;
    m_JS_YCtrl.uMIDIControlChannel = 0;


    float *pJSProg = NULL;
    m_PresetNames[0] = "Factory Preset";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[0] = pJSProg;

    m_PresetNames[1] = "";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[1] = pJSProg;

    m_PresetNames[2] = "";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[2] = pJSProg;

    m_PresetNames[3] = "";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[3] = pJSProg;

    m_PresetNames[4] = "";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[4] = pJSProg;

    m_PresetNames[5] = "";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[5] = pJSProg;

    m_PresetNames[6] = "";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[6] = pJSProg;

    m_PresetNames[7] = "";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[7] = pJSProg;

    m_PresetNames[8] = "";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[8] = pJSProg;

    m_PresetNames[9] = "";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[9] = pJSProg;

    m_PresetNames[10] = "";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[10] = pJSProg;

    m_PresetNames[11] = "";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[11] = pJSProg;

    m_PresetNames[12] = "";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[12] = pJSProg;

    m_PresetNames[13] = "";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[13] = pJSProg;

    m_PresetNames[14] = "";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[14] = pJSProg;

    m_PresetNames[15] = "";
    pJSProg = new float[MAX_JS_PROGRAM_STEPS * MAX_JS_PROGRAM_STEP_VARS];
    pJSProg[JS_PROG_INDEX(0, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(0, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(1, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(2, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(3, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(4, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(5, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(6, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(7, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(8, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(9, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(10, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(11, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(12, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(13, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(14, 6)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 0)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 1)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 2)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 3)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 4)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 5)] = 0.000000;
    pJSProg[JS_PROG_INDEX(15, 6)] = 0.000000;
    m_PresetJSPrograms[15] = pJSProg;


    // **--0xEDA5--**
// ------------------------------------------------------------------------------- //

    return true;

}

























