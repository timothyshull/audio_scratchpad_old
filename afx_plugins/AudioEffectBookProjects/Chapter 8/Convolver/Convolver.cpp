/*
	RackAFX(TM)
	Applications Programming Interface
	Derived Class Object Implementation
*/


#include "Convolver.h"


/* constructor()
	You can initialize variables here.
	You can also allocate memory here as long is it does not
	require the plugin to be fully instantiated. If so, allocate in init()

*/
CConvolver::CConvolver() {
    // Added by RackAFX - DO NOT REMOVE
    //
    // initUI() for GUI controls: this must be called before initializing/using any GUI variables
    initUI();
    // END initUI()

    // built in initialization
    m_PlugInName = "Convolver";

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

    // Finish initializations here
    // set our max buffer length for init
    m_nIRLength = 1024;    // 1024max

    // dynamically allocate the input x buffers and save the pointers
    m_pBufferLeft = new float[m_nIRLength];
    m_pBufferRight = new float[m_nIRLength];

    // flush x buffers
    memset(m_pBufferLeft, 0, m_nIRLength * sizeof(float));
    memset(m_pBufferRight, 0, m_nIRLength * sizeof(float));

    // flush IR buffers
    memset(&m_h_Left, 0, m_nIRLength * sizeof(float));
    memset(&m_h_Right, 0, m_nIRLength * sizeof(float));

    // reset all indices
    m_nReadIndexDL = 0;
    m_nReadIndexH = 0;
    m_nWriteIndex = 0;

    // set the flag for RackAFX to load IRs into our convolver
    m_bWantIRs = true;
}


/* destructor()
	Destroy variables allocated in the contructor()

*/
CConvolver::~CConvolver(void) {
    // free up our input buffers
    delete[] m_pBufferLeft;
    delete[] m_pBufferRight;
}

/*
initialize()
	Called by the client after creation; the parent window handle is now valid
	so you can use the Plug-In -> Host functions here (eg sendUpdateUI())

	NOTE: This function is called whenever the plug-in gets a new Client UI
	      e.g. loading and unloading User Plug-Ins.

	      Therefore, do not place One Time Initialization code here, place it in
	      the END of the constructor.

	      This function is really designed only for letting you communicate back
	      with the Host GUI via sendUpdateUI()
	See the website www.willpirkle.com for more details
*/
bool __stdcall CConvolver::initialize() {
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
bool __stdcall CConvolver::prepareForPlay() {
    // Add your code here:
    // flush buffers
    memset(m_pBufferLeft, 0, m_nIRLength * sizeof(float));
    memset(m_pBufferRight, 0, m_nIRLength * sizeof(float));

    // reset indices
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
LEFT OUTPUT = pOutputBuffer[1]

*/
bool __stdcall CConvolver::processAudioFrame(float *pInputBuffer, float *pOutputBuffer, UINT uNumInputChannels,
                                             UINT uNumOutputChannels) {
    // Do LEFT (MONO) Channel; there is always at least one input/one output
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
    for (int i = 0; i < m_nIRLength; i++) {
        // do the sum of products
        yn_accum += m_pBufferLeft[m_nReadIndexDL] * m_h_Left[m_nReadIndexH];

        // advance the IR index
        m_nReadIndexH++;

        // decrement the Delay Line index
        m_nReadIndexDL--;

        // check for wrap of delay line (no need to check IR buffer)
        if (m_nReadIndexDL < 0)
            m_nReadIndexDL = m_nIRLength - 1;
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
        for (int i = 0; i < m_nIRLength; i++) {
            // do the sum of products
            yn_accum += m_pBufferRight[m_nReadIndexDL] * m_h_Right[m_nReadIndexH];

            // advance the IR index
            m_nReadIndexH++;

            // decrement the Delay Line index
            m_nReadIndexDL--;

            // check for wrap of delay line (no need to check IR buffer)
            if (m_nReadIndexDL < 0)
                m_nReadIndexDL = m_nIRLength - 1;
        }
        // write out
        pOutputBuffer[1] = yn_accum;
    }

    // incremnent the pointers and wrap if necessary
    m_nWriteIndex++;
    if (m_nWriteIndex >= m_nIRLength)
        m_nWriteIndex = 0;

    return true;
}


/* ADDED BY RACKAFX -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
   	**--0x2983--**

		Assignable Buttons               Index
	-----------------------------------------------
		B1		                          50
		B2		                          51
		B3		                          52

	-----------------------------------------------


	**--0xFFDD--**
// ------------------------------------------------------------------------------- */
// Add your UI Handler code here ------------------------------------------------- //
//
bool __stdcall CConvolver::userInterfaceChange(int nControlIndex) {
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
bool __stdcall CConvolver::joystickControlChange(float fControlA, float fControlB, float fControlC, float fControlD,
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
bool __stdcall CConvolver::processRackAFXAudioBuffer(float *pInputBuffer, float *pOutputBuffer,
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
bool __stdcall CConvolver::processVSTAudioBuffer(float **ppInputs, float **ppOutputs,
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

bool __stdcall CConvolver::midiNoteOn(UINT uChannel, UINT uMIDINote, UINT uVelocity) {
    return true;
}

bool __stdcall CConvolver::midiNoteOff(UINT uChannel, UINT uMIDINote, UINT uVelocity, bool bAllNotesOff) {
    return true;
}

// uModValue = 0->127
bool __stdcall CConvolver::midiModWheel(UINT uChannel, UINT uModValue) {
    return true;
}

// nActualPitchBendValue 		= -8192 -> +8191, 0 at center
// fNormalizedPitchBendValue 	= -1.0  -> +1.0,  0 at center
bool __stdcall CConvolver::midiPitchBend(UINT uChannel, int nActualPitchBendValue, float fNormalizedPitchBendValue) {
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
bool __stdcall CConvolver::midiClock() {

    return true;
}

// any midi message other than note on, note off, pitchbend, mod wheel or clock
bool __stdcall CConvolver::midiMessage(unsigned char cChannel, unsigned char cStatus, unsigned char
cData1, unsigned char cData2) {
    return true;
}


// DO NOT DELETE THIS FUNCTION --------------------------------------------------- //
bool __stdcall CConvolver::initUI() {
    // ADDED BY RACKAFX -- DO NOT EDIT THIS CODE!!! ------------------------------ //
    if (m_UIControlList.count() > 0)
        return true;

// **--0xDEA7--**


// **--0xEDA5--**
// ------------------------------------------------------------------------------- //

    return true;

}

