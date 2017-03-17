/*
	RackAFX(TM)
	Applications Programming Interface
	Derived Class Object Implementation
*/


#include "ConvolveIt.h"


/* constructor()
	You can initialize variables here.
	You can also allocate memory here as long is it does not
	require the plugin to be fully instantiated. If so, allocate in init()

*/
CConvolveIt::CConvolveIt() {
    // Added by RackAFX - DO NOT REMOVE
    //
    // initUI() for GUI controls: this must be called before initializing/using any GUI variables
    initUI();
    // END initUI()

    // built in initialization
    m_PlugInName = "ConvolveIt";

    // Default to Stereo Operation:
    // Change this if you want to support more/less channels
    m_uMaxInputChannels = 2;
    m_uMaxOutputChannels = 2;

    // use of MIDI controllers to adjust sliders/knobs
    m_bEnableMIDIControl = true;        // by default this is enabled

    // custom GUI stuff
    m_bLinkGUIRowsAndButtons = false;    // change this if you want to force-link

    // DO NOT CHANGE let RackAFX change it for you; use Edit Project to alter
    m_bUseCustomVSTGUI = false;

    // for a user (not RackAFX) generated GUI - advanced you must compile your own resources
    // DO NOT CHANGE let RackAFX change it for you; use Edit Project to alter
    m_bUserCustomGUI = false;

    // output only - SYNTH - plugin DO NOT CHANGE let RackAFX change it for you; use Edit Project to alter
    m_bOutputOnlyPlugIn = false;

    // un-comment this for VST/AU Buffer-style processing
    // m_bWantVSTBuffers = true;

    // Finish initializations here
    // reset all indices
    m_nReadIndexDL = 0;
    m_nReadIndexH = 0;
    m_nWriteIndex = 0;
    m_nConvolutionLength = 0;

    // --- clear out
    m_pLeftIR = NULL;
    m_pRightIR = NULL;

    // --- input buffers
    m_pBufferLeft = NULL;
    m_pBufferRight = NULL;

    // --- one way to use the CWaveData object is to initialize it at construction
    //     the file LPF2sir.wav was placed in my C:\ root directory
    //     this is a highly resonant LPF IR; easy to verify working
    //     SEE userInterfaceChange() for the second way to use this object
    CWaveData *pWaveData = new CWaveData("C:\\LPF2sirQ.wav");
    if (pWaveData) {
        // --- use helper (optionally, show message box)
        if (openIRFile(pWaveData)); // MessageBox ( NULL , "Opened IR file!" , "Message" , MB_OK);
        //else
        //MessageBox ( NULL , "Failed to open IR file!" , "Message" , MB_OK);

        // --- dont need it amymore
        delete pWaveData;
    }
}

bool CConvolveIt::openIRFile(CWaveData *pWaveData) {
    if (!pWaveData->m_bWaveLoaded) {
        m_nConvolutionLength = 0;
        return false;
    }

    m_nConvolutionLength = (int) ((float) pWaveData->m_uSampleCount / (float) pWaveData->m_uNumChannels);

    if (m_pLeftIR) delete[] m_pLeftIR;
    if (m_pRightIR) delete[] m_pRightIR;
    if (m_pBufferLeft) delete[] m_pBufferLeft;
    if (m_pBufferRight) delete[] m_pBufferRight;

    m_pLeftIR = new float[m_nConvolutionLength];
    m_pRightIR = new float[m_nConvolutionLength];
    m_pBufferLeft = new float[m_nConvolutionLength];
    m_pBufferRight = new float[m_nConvolutionLength];

    memset(m_pBufferLeft, 0, m_nConvolutionLength * sizeof(float));
    memset(m_pBufferRight, 0, m_nConvolutionLength * sizeof(float));
    memset(m_pLeftIR, 0, m_nConvolutionLength * sizeof(float));
    memset(m_pRightIR, 0, m_nConvolutionLength * sizeof(float));

    // --- for mono, just copy CWaveData buffer into our IR buffers
    if (pWaveData->m_uNumChannels == 1) {
        for (int i = 0; i < pWaveData->m_uSampleCount; i++) {
            m_pLeftIR[i] = pWaveData->m_pWaveBuffer[i];
            m_pRightIR[i] = m_pLeftIR[i];
        }
    }
    else // stereo
    {
        int m = 0;
        for (int i = 0; i < pWaveData->m_uSampleCount; i++) {
            // left channel are even index, right are odd
            if (i % 2 == 0)
                m_pLeftIR[m] = pWaveData->m_pWaveBuffer[i];
            else
                m_pRightIR[m++] = pWaveData->m_pWaveBuffer[i];
        }
    }

    return true;
}

/* destructor()
	Destroy variables allocated in the contructor()

*/
CConvolveIt::~CConvolveIt(void) {
    if (m_pLeftIR) delete[] m_pLeftIR;
    if (m_pRightIR) delete[] m_pRightIR;
    if (m_pBufferLeft) delete[] m_pBufferLeft;
    if (m_pBufferRight) delete[] m_pBufferRight;
}

/*
initialize()
	Called by the client after creation; the parent window handle is now valid
	so you can use the Plug-In -> Host functions here (eg sendUpdateUI())
	See the website www.willpirkle.com for more details
*/
bool __stdcall CConvolveIt::initialize() {
    // Add your code here

    return true;
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
bool __stdcall CConvolveIt::prepareForPlay() {
    // Add your code here:
    memset(m_pBufferLeft, 0, m_nConvolutionLength * sizeof(float));
    memset(m_pBufferRight, 0, m_nConvolutionLength * sizeof(float));

    // reset all indices
    m_nReadIndexDL = 0;
    m_nReadIndexH = 0;
    m_nWriteIndex = 0;

    return true;
}


/* processAudioFrame

// ALL VALUES IN AND OUT ON THE RANGE OF -1.0 TO + 1.0

LEFT INPUT = pInputBuffer[0];
RIGHT INPUT = pInputBuffer[1]

LEFT INPUT = pInputBuffer[0]
RIGHT OUTPUT = pOutputBuffer[1]

*/
bool __stdcall CConvolveIt::processAudioFrame(float *pInputBuffer, float *pOutputBuffer, UINT uNumInputChannels,
                                              UINT uNumOutputChannels) {
    if (m_nConvolutionLength <= 0) {
        // pass thru
        pOutputBuffer[0] = pInputBuffer[0];
        if (uNumInputChannels == 1 && uNumOutputChannels == 2)
            pOutputBuffer[1] = pInputBuffer[0];
        if (uNumInputChannels == 2 && uNumOutputChannels == 2)
            pOutputBuffer[1] = pInputBuffer[1];
        return true;
    }

    // Read the Input
    float xn = pInputBuffer[0];

    // write x(n) -- now have x(n) -> x(n-1023)
    m_pBufferLeft[m_nWriteIndex] = xn;

    // reset: read index for Delay Line -> write index
    m_nReadIndexDL = m_nWriteIndex;

    // reset: read index for IR - > top (0)
    m_nReadIndexH = 0;

    // accumulator
    float yn_accum = 0;

    // convolve:
    for (int i = 0; i < m_nConvolutionLength; i++) {
        // do the sum of products
        yn_accum += m_pBufferLeft[m_nReadIndexDL] * m_pLeftIR[m_nReadIndexH];

        // advance the IR index
        m_nReadIndexH++;

        // decrement the Delay Line index
        m_nReadIndexDL--;

        // check for wrap of delay line (no need to check IR buffer)
        if (m_nReadIndexDL < 0)
            m_nReadIndexDL = m_nConvolutionLength - 1;
    }

    // write out
    pOutputBuffer[0] = yn_accum;

    // Mono-In, Stereo-Out (AUX Effect)
    if (uNumInputChannels == 1 && uNumOutputChannels == 2)
        pOutputBuffer[1] = pOutputBuffer[0]; // just copy

    // Stereo-In, Stereo-Out (INSERT Effect)
    if (uNumInputChannels == 2 && uNumOutputChannels == 2) {
        // Read the Input
        xn = pInputBuffer[1];

        // write x(n) -- now have x(n) -> x(n-1023)
        m_pBufferRight[m_nWriteIndex] = xn;

        // reset: read index for Delay Line -> write index
        m_nReadIndexDL = m_nWriteIndex;

        // reset: read index for IR - > top (0)
        m_nReadIndexH = 0;

        // accumulator
        yn_accum = 0;

        // convolve:
        for (int i = 0; i < m_nConvolutionLength; i++) {
            // do the sum of products
            yn_accum += m_pBufferRight[m_nReadIndexDL] * m_pRightIR[m_nReadIndexH];

            // advance the IR index
            m_nReadIndexH++;

            // decrement the Delay Line index
            m_nReadIndexDL--;

            // check for wrap of delay line (no need to check IR buffer)
            if (m_nReadIndexDL < 0)
                m_nReadIndexDL = m_nConvolutionLength - 1;
        }
        // write out
        pOutputBuffer[1] = yn_accum;
    }

    // incremnent the pointers and wrap if necessary
    m_nWriteIndex++;
    if (m_nWriteIndex >= m_nConvolutionLength)
        m_nWriteIndex = 0;

    return true;
}


/* ADDED BY RACKAFX -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
   	**--0x2983--**

	Variable Name                    Index
-----------------------------------------------

	Assignable Buttons               Index
-----------------------------------------------
	Open IR                           50
	                                  51
	                                  52

-----------------------------------------------
Joystick Drop List Boxes          Index
-----------------------------------------------
	 Drop List A                     60
	 Drop List B                     61
	 Drop List C                     62
	 Drop List D                     63

-----------------------------------------------

	**--0xFFDD--**
// ------------------------------------------------------------------------------- */
// Add your UI Handler code here ------------------------------------------------- //
//
bool __stdcall CConvolveIt::userInterfaceChange(int nControlIndex) {
    // decode the control index, or delete the switch and use brute force calls
    switch (nControlIndex) {
        case 50: {
            // --- the other way to initialize is to let the user select an IR
            CWaveData *pWaveData = new CWaveData(NULL);

            if (pWaveData) {
                if (pWaveData->initWithUserWAVFile()) {
                    // --- use helper
                    if (openIRFile(pWaveData))
                        sendStatusWndText("Opened IR File!");
                    else
                        sendStatusWndText("Failed to open IR File!");

                    // --- don't need anymore
                    delete pWaveData;
                }
            }

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
bool __stdcall CConvolveIt::joystickControlChange(float fControlA, float fControlB, float fControlC, float fControlD,
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

	To use this function set m_bWantBuffers = true in your constructor.

	******************************
	********* IMPORTANT! *********
	******************************
	If you are going to ultimately make this a VST Compatible Plug-In and you want to process
	buffers, you need to override the NEXT function below:

	processVSTAudioBuffer()


	This function (processRackAFXAudioBuffer) is not supported in the VST wrapper because
	the VST buffer sizes no maximum value. This would require the use of dynamic buffering
	in the callback which is not acceptable for performance!
*/
bool __stdcall CConvolveIt::processRackAFXAudioBuffer(float *pInputBuffer, float *pOutputBuffer,
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

	NOTE: You do not have to implement this function if you don't want to; the processAudioFrame()
	will still work; however this using function will be more CPU efficient for your plug-in, and will
	override processAudioFrame().

	To use this function set m_bWantVSTBuffers = true in your constructor.

	The VST input and output buffers are pointers-to-pointers. The pp buffers are the same depth as uNumChannels, so
	if uNumChannels = 2, then ppInputs would contain two pointers,

		inBuffer[0] = a pointer to the LEFT buffer of data
		inBuffer[1] = a pointer to the RIGHT buffer of data

	Similarly, outBuffer would have 2 pointers, one for left and one for right.

	For 5.1 audio you would get 6 pointers in each buffer.

*/
bool __stdcall CConvolveIt::processVSTAudioBuffer(float **inBuffer, float **outBuffer, UINT uNumChannels,
                                                  int inFramesToProcess) {
    // PASS Through example
    // MONO First
    float *pInputL = inBuffer[0];
    float *pOutputL = outBuffer[0];
    float *pInputR = NULL;
    float *pOutputR = NULL;

    // if STEREO,
    if (inBuffer[1])
        pInputR = inBuffer[1];

    if (outBuffer[1])
        pOutputR = outBuffer[1];

    // Process audio by de-referencing ptrs
    // this is siple pass through code
    while (--inFramesToProcess >= 0) {
        // Left channel processing
        *pOutputL = *pInputL;

        // If there is a right channel
        if (pInputR)
            *pOutputR = *pInputR;

        // advance pointers
        pInputL++;
        pOutputL++;
        if (pInputR) pInputR++;
        if (pOutputR) pOutputR++;
    }
    // all OK
    return true;
}

bool __stdcall CConvolveIt::midiNoteOn(UINT uChannel, UINT uMIDINote, UINT uVelocity) {
    return true;
}

bool __stdcall CConvolveIt::midiNoteOff(UINT uChannel, UINT uMIDINote, UINT uVelocity, bool bAllNotesOff) {
    return true;
}

// uModValue = 0->127
bool __stdcall CConvolveIt::midiModWheel(UINT uChannel, UINT uModValue) {
    return true;
}

// nActualPitchBendValue 		= -8192 -> +8191, 0 at center
// fNormalizedPitchBendValue 	= -1.0  -> +1.0,  0 at center
bool __stdcall CConvolveIt::midiPitchBend(UINT uChannel, int nActualPitchBendValue, float fNormalizedPitchBendValue) {
    return true;
}

// MIDI Clock
// http://home.roadrunner.com/~jgglatt/tech/midispec/clock.htm
/* There are 24 MIDI Clocks in every quarter note. (12 MIDI Clocks in an eighth note, 6 MIDI Clocks in a 16th, etc).
   Therefore, when a slave device counts down the receipt of 24 MIDI Clock messages, it knows that one quarter note
   has passed. When the slave counts off another 24 MIDI Clock messages, it knows that another quarter note has passed.
   Etc. Of course, the rate that the master sends these messages is based upon the master's tempo.

   For example, for a tempo of 120 BPM (ie, there are 120 quarter notes in every minute), the master sends a MIDI clock
   every 20833 microseconds. (ie, There are 1,000,000 microseconds in a second. Therefore, there are 60,000,000
   microseconds in a minute. At a tempo of 120 BPM, there are 120 quarter notes per minute. There are 24 MIDI clocks
   in each quarter note. Therefore, there should be 24 * 120 MIDI Clocks per minute.
   So, each MIDI Clock is sent at a rate of 60,000,000/(24 * 120) microseconds).
*/
bool __stdcall CConvolveIt::midiClock() {

    return true;
}

// any midi message other than note on, note off, pitchbend, mod wheel or clock
bool __stdcall CConvolveIt::midiMessage(unsigned char cChannel, unsigned char cStatus, unsigned char
cData1, unsigned char cData2) {
    return true;
}


// DO NOT DELETE THIS FUNCTION --------------------------------------------------- //
bool __stdcall CConvolveIt::initUI() {
    // ADDED BY RACKAFX -- DO NOT EDIT THIS CODE!!! ------------------------------ //
    if (m_UIControlList.count() > 0)
        return true;

// **--0xDEA7--**

    m_uX_TrackPadIndex = -1;
    m_uY_TrackPadIndex = -1;

    m_AssignButton1Name = "Open IR";
    m_AssignButton2Name = "";
    m_AssignButton3Name = "";

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

    m_uPlugInEx[0] = 0;
    m_uPlugInEx[1] = 0;
    m_uPlugInEx[2] = 0;
    m_uPlugInEx[3] = 0;
    m_uPlugInEx[4] = 0;
    m_uPlugInEx[5] = 0;
    m_uPlugInEx[6] = 0;
    m_uPlugInEx[7] = 0;
    m_uPlugInEx[8] = 0;
    m_uPlugInEx[9] = 0;
    m_uPlugInEx[10] = 0;
    m_uPlugInEx[11] = 0;
    m_uPlugInEx[12] = 0;
    m_uPlugInEx[13] = 0;
    m_uPlugInEx[14] = 0;
    m_uPlugInEx[15] = 0;
    m_uPlugInEx[16] = 0;
    m_uPlugInEx[17] = 0;
    m_uPlugInEx[18] = 0;
    m_uPlugInEx[19] = 0;
    m_uPlugInEx[20] = 0;
    m_uPlugInEx[21] = 0;
    m_uPlugInEx[22] = 0;
    m_uPlugInEx[23] = 0;
    m_uPlugInEx[24] = 0;
    m_uPlugInEx[25] = 0;
    m_uPlugInEx[26] = 0;
    m_uPlugInEx[27] = 0;
    m_uPlugInEx[28] = 0;
    m_uPlugInEx[29] = 0;
    m_uPlugInEx[30] = 0;
    m_uPlugInEx[31] = 0;
    m_uPlugInEx[32] = 0;
    m_uPlugInEx[33] = 0;
    m_uPlugInEx[34] = 0;
    m_uPlugInEx[35] = 0;
    m_uPlugInEx[36] = 0;
    m_uPlugInEx[37] = 0;
    m_uPlugInEx[38] = 0;
    m_uPlugInEx[39] = 0;
    m_uPlugInEx[40] = 0;
    m_uPlugInEx[41] = 0;
    m_uPlugInEx[42] = 0;
    m_uPlugInEx[43] = 0;
    m_uPlugInEx[44] = 0;
    m_uPlugInEx[45] = 0;
    m_uPlugInEx[46] = 0;
    m_uPlugInEx[47] = 0;
    m_uPlugInEx[48] = 0;
    m_uPlugInEx[49] = 0;
    m_uPlugInEx[50] = 0;
    m_uPlugInEx[51] = 0;
    m_uPlugInEx[52] = 0;
    m_uPlugInEx[53] = 0;
    m_uPlugInEx[54] = 0;
    m_uPlugInEx[55] = 0;
    m_uPlugInEx[56] = 0;
    m_uPlugInEx[57] = 0;
    m_uPlugInEx[58] = 0;
    m_uPlugInEx[59] = 0;
    m_uPlugInEx[60] = 0;
    m_uPlugInEx[61] = 0;
    m_uPlugInEx[62] = 0;
    m_uPlugInEx[63] = 0;
    m_fPlugInEx[0] = 0.000000;
    m_fPlugInEx[1] = 0.000000;
    m_fPlugInEx[2] = 0.000000;
    m_fPlugInEx[3] = 0.000000;
    m_fPlugInEx[4] = 0.000000;
    m_fPlugInEx[5] = 0.000000;
    m_fPlugInEx[6] = 0.000000;
    m_fPlugInEx[7] = 0.000000;
    m_fPlugInEx[8] = 0.000000;
    m_fPlugInEx[9] = 0.000000;
    m_fPlugInEx[10] = 0.000000;
    m_fPlugInEx[11] = 0.000000;
    m_fPlugInEx[12] = 0.000000;
    m_fPlugInEx[13] = 0.000000;
    m_fPlugInEx[14] = 0.000000;
    m_fPlugInEx[15] = 0.000000;
    m_fPlugInEx[16] = 0.000000;
    m_fPlugInEx[17] = 0.000000;
    m_fPlugInEx[18] = 0.000000;
    m_fPlugInEx[19] = 0.000000;
    m_fPlugInEx[20] = 0.000000;
    m_fPlugInEx[21] = 0.000000;
    m_fPlugInEx[22] = 0.000000;
    m_fPlugInEx[23] = 0.000000;
    m_fPlugInEx[24] = 0.000000;
    m_fPlugInEx[25] = 0.000000;
    m_fPlugInEx[26] = 0.000000;
    m_fPlugInEx[27] = 0.000000;
    m_fPlugInEx[28] = 0.000000;
    m_fPlugInEx[29] = 0.000000;
    m_fPlugInEx[30] = 0.000000;
    m_fPlugInEx[31] = 0.000000;
    m_fPlugInEx[32] = 0.000000;
    m_fPlugInEx[33] = 0.000000;
    m_fPlugInEx[34] = 0.000000;
    m_fPlugInEx[35] = 0.000000;
    m_fPlugInEx[36] = 0.000000;
    m_fPlugInEx[37] = 0.000000;
    m_fPlugInEx[38] = 0.000000;
    m_fPlugInEx[39] = 0.000000;
    m_fPlugInEx[40] = 0.000000;
    m_fPlugInEx[41] = 0.000000;
    m_fPlugInEx[42] = 0.000000;
    m_fPlugInEx[43] = 0.000000;
    m_fPlugInEx[44] = 0.000000;
    m_fPlugInEx[45] = 0.000000;
    m_fPlugInEx[46] = 0.000000;
    m_fPlugInEx[47] = 0.000000;
    m_fPlugInEx[48] = 0.000000;
    m_fPlugInEx[49] = 0.000000;
    m_fPlugInEx[50] = 0.000000;
    m_fPlugInEx[51] = 0.000000;
    m_fPlugInEx[52] = 0.000000;
    m_fPlugInEx[53] = 0.000000;
    m_fPlugInEx[54] = 0.000000;
    m_fPlugInEx[55] = 0.000000;
    m_fPlugInEx[56] = 0.000000;
    m_fPlugInEx[57] = 0.000000;
    m_fPlugInEx[58] = 0.000000;
    m_fPlugInEx[59] = 0.000000;
    m_fPlugInEx[60] = 0.000000;
    m_fPlugInEx[61] = 0.000000;
    m_fPlugInEx[62] = 0.000000;
    m_fPlugInEx[63] = 0.000000;

    m_TextLabels[0] = "";
    m_TextLabels[1] = "";
    m_TextLabels[2] = "";
    m_TextLabels[3] = "";
    m_TextLabels[4] = "";
    m_TextLabels[5] = "";
    m_TextLabels[6] = "";
    m_TextLabels[7] = "";
    m_TextLabels[8] = "";
    m_TextLabels[9] = "";
    m_TextLabels[10] = "";
    m_TextLabels[11] = "";
    m_TextLabels[12] = "";
    m_TextLabels[13] = "";
    m_TextLabels[14] = "";
    m_TextLabels[15] = "";
    m_TextLabels[16] = "";
    m_TextLabels[17] = "";
    m_TextLabels[18] = "";
    m_TextLabels[19] = "";
    m_TextLabels[20] = "";
    m_TextLabels[21] = "";
    m_TextLabels[22] = "";
    m_TextLabels[23] = "";
    m_TextLabels[24] = "";
    m_TextLabels[25] = "";
    m_TextLabels[26] = "";
    m_TextLabels[27] = "";
    m_TextLabels[28] = "";
    m_TextLabels[29] = "";
    m_TextLabels[30] = "";
    m_TextLabels[31] = "";
    m_TextLabels[32] = "";
    m_TextLabels[33] = "";
    m_TextLabels[34] = "";
    m_TextLabels[35] = "";
    m_TextLabels[36] = "";
    m_TextLabels[37] = "";
    m_TextLabels[38] = "";
    m_TextLabels[39] = "";
    m_TextLabels[40] = "";
    m_TextLabels[41] = "";
    m_TextLabels[42] = "";
    m_TextLabels[43] = "";
    m_TextLabels[44] = "";
    m_TextLabels[45] = "";
    m_TextLabels[46] = "";
    m_TextLabels[47] = "";
    m_TextLabels[48] = "";
    m_TextLabels[49] = "";
    m_TextLabels[50] = "";
    m_TextLabels[51] = "";
    m_TextLabels[52] = "";
    m_TextLabels[53] = "";
    m_TextLabels[54] = "";
    m_TextLabels[55] = "";
    m_TextLabels[56] = "";
    m_TextLabels[57] = "";
    m_TextLabels[58] = "";
    m_TextLabels[59] = "";
    m_TextLabels[60] = "";
    m_TextLabels[61] = "";
    m_TextLabels[62] = "";
    m_TextLabels[63] = "";

    m_uLabelCX[0] = 0;
    m_uLabelCX[1] = 0;
    m_uLabelCX[2] = 0;
    m_uLabelCX[3] = 0;
    m_uLabelCX[4] = 0;
    m_uLabelCX[5] = 0;
    m_uLabelCX[6] = 0;
    m_uLabelCX[7] = 0;
    m_uLabelCX[8] = 0;
    m_uLabelCX[9] = 0;
    m_uLabelCX[10] = 0;
    m_uLabelCX[11] = 0;
    m_uLabelCX[12] = 0;
    m_uLabelCX[13] = 0;
    m_uLabelCX[14] = 0;
    m_uLabelCX[15] = 0;
    m_uLabelCX[16] = 0;
    m_uLabelCX[17] = 0;
    m_uLabelCX[18] = 0;
    m_uLabelCX[19] = 0;
    m_uLabelCX[20] = 0;
    m_uLabelCX[21] = 0;
    m_uLabelCX[22] = 0;
    m_uLabelCX[23] = 0;
    m_uLabelCX[24] = 0;
    m_uLabelCX[25] = 0;
    m_uLabelCX[26] = 0;
    m_uLabelCX[27] = 0;
    m_uLabelCX[28] = 0;
    m_uLabelCX[29] = 0;
    m_uLabelCX[30] = 0;
    m_uLabelCX[31] = 0;
    m_uLabelCX[32] = 0;
    m_uLabelCX[33] = 0;
    m_uLabelCX[34] = 0;
    m_uLabelCX[35] = 0;
    m_uLabelCX[36] = 0;
    m_uLabelCX[37] = 0;
    m_uLabelCX[38] = 0;
    m_uLabelCX[39] = 0;
    m_uLabelCX[40] = 0;
    m_uLabelCX[41] = 0;
    m_uLabelCX[42] = 0;
    m_uLabelCX[43] = 0;
    m_uLabelCX[44] = 0;
    m_uLabelCX[45] = 0;
    m_uLabelCX[46] = 0;
    m_uLabelCX[47] = 0;
    m_uLabelCX[48] = 0;
    m_uLabelCX[49] = 0;
    m_uLabelCX[50] = 0;
    m_uLabelCX[51] = 0;
    m_uLabelCX[52] = 0;
    m_uLabelCX[53] = 0;
    m_uLabelCX[54] = 0;
    m_uLabelCX[55] = 0;
    m_uLabelCX[56] = 0;
    m_uLabelCX[57] = 0;
    m_uLabelCX[58] = 0;
    m_uLabelCX[59] = 0;
    m_uLabelCX[60] = 0;
    m_uLabelCX[61] = 0;
    m_uLabelCX[62] = 0;
    m_uLabelCX[63] = 0;
    m_uLabelCY[0] = 0;
    m_uLabelCY[1] = 0;
    m_uLabelCY[2] = 0;
    m_uLabelCY[3] = 0;
    m_uLabelCY[4] = 0;
    m_uLabelCY[5] = 0;
    m_uLabelCY[6] = 0;
    m_uLabelCY[7] = 0;
    m_uLabelCY[8] = 0;
    m_uLabelCY[9] = 0;
    m_uLabelCY[10] = 0;
    m_uLabelCY[11] = 0;
    m_uLabelCY[12] = 0;
    m_uLabelCY[13] = 0;
    m_uLabelCY[14] = 0;
    m_uLabelCY[15] = 0;
    m_uLabelCY[16] = 0;
    m_uLabelCY[17] = 0;
    m_uLabelCY[18] = 0;
    m_uLabelCY[19] = 0;
    m_uLabelCY[20] = 0;
    m_uLabelCY[21] = 0;
    m_uLabelCY[22] = 0;
    m_uLabelCY[23] = 0;
    m_uLabelCY[24] = 0;
    m_uLabelCY[25] = 0;
    m_uLabelCY[26] = 0;
    m_uLabelCY[27] = 0;
    m_uLabelCY[28] = 0;
    m_uLabelCY[29] = 0;
    m_uLabelCY[30] = 0;
    m_uLabelCY[31] = 0;
    m_uLabelCY[32] = 0;
    m_uLabelCY[33] = 0;
    m_uLabelCY[34] = 0;
    m_uLabelCY[35] = 0;
    m_uLabelCY[36] = 0;
    m_uLabelCY[37] = 0;
    m_uLabelCY[38] = 0;
    m_uLabelCY[39] = 0;
    m_uLabelCY[40] = 0;
    m_uLabelCY[41] = 0;
    m_uLabelCY[42] = 0;
    m_uLabelCY[43] = 0;
    m_uLabelCY[44] = 0;
    m_uLabelCY[45] = 0;
    m_uLabelCY[46] = 0;
    m_uLabelCY[47] = 0;
    m_uLabelCY[48] = 0;
    m_uLabelCY[49] = 0;
    m_uLabelCY[50] = 0;
    m_uLabelCY[51] = 0;
    m_uLabelCY[52] = 0;
    m_uLabelCY[53] = 0;
    m_uLabelCY[54] = 0;
    m_uLabelCY[55] = 0;
    m_uLabelCY[56] = 0;
    m_uLabelCY[57] = 0;
    m_uLabelCY[58] = 0;
    m_uLabelCY[59] = 0;
    m_uLabelCY[60] = 0;
    m_uLabelCY[61] = 0;
    m_uLabelCY[62] = 0;
    m_uLabelCY[63] = 0;

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

    // Additional Preset Support (avanced)


    // **--0xEDA5--**
// ------------------------------------------------------------------------------- //

    return true;

}



