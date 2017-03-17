/*
	Socket(R) Rapid Plug-In Development (RaPID) Client
	Applications Programming Interface
	Derived Class Object Implementation
*/


#include "DDLModule.h"


/* constructor()
	You can initialize variables here.
	You can also allocate memory here as long is it does not
	require the plugin to be fully instantiated. If so, allocate in init()

*/
CDDLModule::CDDLModule() {
    // Added by Socket - DO NOT REMOVE
    //
    // Setup the Socket UI Control List and Initialize Variables
    initUI();
    // END InitUI

    // built in initialization
    m_PlugInName = "DDLModule";
    m_fDelayInSamples = 0;
    m_fFeedback = 0;
    m_fDelay_ms = 0;
    m_fFeedback_pct = 0;
    m_fWetLevel = 0;

    m_pBuffer = NULL;
    m_nBufferSize = 0;

    //m_pBuffer = new float[m_nBufferSize];
    //
    //// flush buffer
    //memset(m_pBuffer, 0, m_nBufferSize*sizeof(float));

    // reset
    m_nReadIndex = 0;
    m_nWriteIndex = 0;
    m_fDDLOutput = 0;



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
    //// reset first
    //resetDelay();

    // cook
    cookVariables();

}

void CDDLModule::resetDelay() {
    // flush buffer
    if (m_pBuffer)
        memset(m_pBuffer, 0, m_nBufferSize * sizeof(float));

    // init read/write indices
    m_nWriteIndex = 0; // reset the Write index to top
    m_nReadIndex = 0; // reset the Read index to top

    m_fDDLOutput = 0;
}

/* destructor()
	Destroy variables allocated in the contructor()

*/
CDDLModule::~CDDLModule(void) {
    // delete buffer if it exists
    if (m_pBuffer)
        delete[] m_pBuffer;


}

void CDDLModule::cookVariables() {
    m_fFeedback = m_fFeedback_pct / 100.0;
    m_fWetLevel = m_fWetLevel_pct / 100.0;
    m_fDelayInSamples = m_fDelay_ms * (44100.0 / 1000.0);

    // subtract to make read index
    m_nReadIndex = m_nWriteIndex - (int) m_fDelayInSamples;

    //  the check and wrap BACKWARDS if the index is negative
    if (m_nReadIndex < 0)
        m_nReadIndex += m_nBufferSize;    // amount of wrap is Read + Length

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
bool __stdcall CDDLModule::prepareForPlay() {
    // setup our delay line
    m_nBufferSize = 2 * m_nSampleRate;    // 2 seconds delay @ fs

    // delete it if it exists
    if (m_pBuffer)
        delete[] m_pBuffer;

    // create the new buffer
    m_pBuffer = new float[m_nBufferSize];

    // Add your code here:
    // reset first
    resetDelay();

    // cook
    cookVariables();


    return true;
}


/* processAudioFrame

// ALL VALUES IN AND OUT ON THE RANGE OF -1.0 TO + 1.0

LEFT INPUT = pInputBuffer[0];
RIGHT INPUT = pInputBuffer[1]

LEFT INPUT = pInputBuffer[0]
LEFT OUTPUT = pOutputBuffer[1]

*/
bool __stdcall CDDLModule::processAudioFrame(float *pInputBuffer, float *pOutputBuffer, UINT uNumInputChannels,
                                             UINT uNumOutputChannels) {
    // output = input -- change this for meaningful processing
    //
    // Do LEFT (MONO) Channel
    // Read the Input
    float xn = pInputBuffer[0];

    // Read the output of the delay at m_nReadIndex
    float yn = m_pBuffer[m_nReadIndex];

    // delay < 1 sample
    if (m_nReadIndex == m_nWriteIndex && m_fDelayInSamples < 1.00) {
        // yn = x(n), will interpolate with x(n-1) next
        yn = xn;
    }

    // Read the location ONE BEHIND yn at y(n-1)
    int nReadIndex_1 = m_nReadIndex - 1;
    if (nReadIndex_1 < 0)
        nReadIndex_1 = m_nBufferSize - 1; // m_nBufferSize-1 is last location

    // get y(n-1)
    float yn_1 = m_pBuffer[nReadIndex_1];

    // interpolate: (0, yn) and (1, yn_1) by the amount fracDelay
    float fFracDelay = m_fDelayInSamples - (int) m_fDelayInSamples;

    // linerp: x1, x2, y1, y2, x
    float fInterp = dLinTerp(0, 1, yn, yn_1, fFracDelay); // interp frac between them

    // use the interpolated value
    yn = fInterp;

    // provide an unaltered output
    m_fDDLOutput = yn;

    // write the intput to the delay
    if (!m_bUseExternalFeedback)
        m_pBuffer[m_nWriteIndex] = xn + m_fFeedback * yn; // normal
    else
        m_pBuffer[m_nWriteIndex] = xn + m_fFeedbackIn; // external feedback sample

    // write the intput to the delay
    m_pBuffer[m_nWriteIndex] = xn + m_fFeedback * yn;

    // create the wet/dry mix and write to the output buffer
    // dry = 1 - wet
    pOutputBuffer[0] = m_fWetLevel * yn + (1.0 - m_fWetLevel) * xn;

    // incremnent the pointers and wrap if necessary
    m_nWriteIndex++;
    if (m_nWriteIndex >= m_nBufferSize)
        m_nWriteIndex = 0;

    m_nReadIndex++;
    if (m_nReadIndex >= m_nBufferSize)
        m_nReadIndex = 0;

    // Mono-In, Stereo-Out (AUX Effect)
    if (uNumInputChannels == 1 && uNumOutputChannels == 2)
        pOutputBuffer[1] = pOutputBuffer[0]; // copy MONO!

    // DDL Module is MONO - just do a copy here too
    // Stereo-In, Stereo-Out (INSERT Effect)
    if (uNumInputChannels == 2 && uNumOutputChannels == 2)
        pOutputBuffer[1] = pOutputBuffer[0];    // copy MONO!

    return true;
}


/* ADDED BY SOCKET -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
   	**--0x2983--**

	Variable Name                    Index
-----------------------------------------------
	m_fDelay_ms                       0
	m_fFeedback_pct                   1
	m_fWetLevel_pct                   2

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
bool __stdcall CDDLModule::userInterfaceChange(int nControlIndex) {
    // cook
    cookVariables();

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
bool __stdcall CDDLModule::joystickControlChange(float fControlA, float fControlB, float fControlC, float fControlD,
                                                 float fACMix, float fBDMix) {
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
bool __stdcall CDDLModule::processSocketAudioBuffer(float *pInputBuffer, float *pOutputBuffer,
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
bool __stdcall CDDLModule::processVSTAudioBuffer(float **ppInputs, float **ppOutputs,
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

bool __stdcall CDDLModule::midiNoteOn(UINT uChannel, UINT uMIDINote, UINT uVelocity) {
    return true;
}

bool __stdcall CDDLModule::midiNoteOff(UINT uChannel, UINT uMIDINote, UINT uVelocity, bool bAllNotesOff) {
    return true;
}

bool __stdcall CDDLModule::midiMessage(unsigned char cChannel, unsigned char cStatus, unsigned char
cData1, unsigned char cData2) {
    return true;
}

// DO NOT DELETE THIS FUNCTION --------------------------------------------------- //
bool __stdcall CDDLModule::initUI() {
// ADDED BY SOCKET -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
    if (m_UIControlList.count() > 0)
        return true;

// **--0xDEA7--**

    m_fDelay_ms = 0.000000;
    CUICtrl ui0;
    ui0.uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
    ui0.uControlId = 0;
    ui0.fUserDisplayDataLoLimit = 0.000000;
    ui0.fUserDisplayDataHiLimit = 2000.000000;
    ui0.uUserDataType = floatData;
    ui0.fInitUserIntValue = 0;
    ui0.fInitUserFloatValue = 0.000000;
    ui0.fInitUserDoubleValue = 0;
    ui0.fInitUserUINTValue = 0;
    ui0.m_pUserCookedIntData = NULL;
    ui0.m_pUserCookedFloatData = &m_fDelay_ms;
    ui0.m_pUserCookedDoubleData = NULL;
    ui0.m_pUserCookedUINTData = NULL;
    ui0.cControlUnits = "mSec                                                            ";
    ui0.cVariableName = "m_fDelay_ms";
    ui0.cEnumeratedList = "SEL1,SEL2,SEL3";
    ui0.dPresetData[0] = 0.000000;
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
    ui0.cControlName = "Delay";
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


    m_fFeedback_pct = 0.000000;
    CUICtrl ui1;
    ui1.uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
    ui1.uControlId = 1;
    ui1.fUserDisplayDataLoLimit = -100.000000;
    ui1.fUserDisplayDataHiLimit = 100.000000;
    ui1.uUserDataType = floatData;
    ui1.fInitUserIntValue = 0;
    ui1.fInitUserFloatValue = 0.000000;
    ui1.fInitUserDoubleValue = 0;
    ui1.fInitUserUINTValue = 0;
    ui1.m_pUserCookedIntData = NULL;
    ui1.m_pUserCookedFloatData = &m_fFeedback_pct;
    ui1.m_pUserCookedDoubleData = NULL;
    ui1.m_pUserCookedUINTData = NULL;
    ui1.cControlUnits = "%                                                               ";
    ui1.cVariableName = "m_fFeedback_pct";
    ui1.cEnumeratedList = "SEL1,SEL2,SEL3";
    ui1.dPresetData[0] = 0.000000;
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
    ui1.cControlName = "Feedback";
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


    m_fWetLevel_pct = 50.000000;
    CUICtrl ui2;
    ui2.uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
    ui2.uControlId = 2;
    ui2.fUserDisplayDataLoLimit = 0.000000;
    ui2.fUserDisplayDataHiLimit = 100.000000;
    ui2.uUserDataType = floatData;
    ui2.fInitUserIntValue = 0;
    ui2.fInitUserFloatValue = 50.000000;
    ui2.fInitUserDoubleValue = 0;
    ui2.fInitUserUINTValue = 0;
    ui2.m_pUserCookedIntData = NULL;
    ui2.m_pUserCookedFloatData = &m_fWetLevel_pct;
    ui2.m_pUserCookedDoubleData = NULL;
    ui2.m_pUserCookedUINTData = NULL;
    ui2.cControlUnits = "%                                                               ";
    ui2.cVariableName = "m_fWetLevel_pct";
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
    ui2.cControlName = "Wet/Dry";
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
    m_PresetNames[0] = "";
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











