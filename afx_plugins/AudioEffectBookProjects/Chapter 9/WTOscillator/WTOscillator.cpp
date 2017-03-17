/*
	RackAFX(TM)
	Applications Programming Interface
	Derived Class Object Implementation
*/


#include "WTOscillator.h"


/* constructor()
	You can initialize variables here.
	You can also allocate memory here as long is it does not
	require the plugin to be fully instantiated. If so, allocate in init()

*/
CWTOscillator::CWTOscillator() {
    // Added by RackAFX - DO NOT REMOVE
    //
    // initUI() for GUI controls: this must be called before initializing/using any GUI variables
    initUI();
    // END initUI()

    // built in initialization
    m_PlugInName = "WTOscillator";

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
    m_bOutputOnlyPlugIn = true;

    // slope and y-intercept values for the
    // Triangle Wave
    // rising edge1:
    float mt1 = 1.0 / 256.0;
    float bt1 = 0.0;

    // rising edge2:
    float mt2 = 1.0 / 256.0;
    float bt2 = -1.0;

    // falling edge:
    float mtf2 = -2.0 / 512.0;
    float btf2 = +1.0;

    // Sawtooth
    // rising edge1:
    float ms1 = 1.0 / 512.0;
    float bs1 = 0.0;

    // rising edge2:
    float ms2 = 1.0 / 512.0;
    float bs2 = -1.0;

    // to keep track of max-es for normalization later
    float fMaxTri = 0;
    float fMaxSaw = 0;
    float fMaxSqr = 0;

    // setup arrays
    for (int i = 0; i < 1024; i++) {
        // sample the sinusoid, 1024 points
        // sin(wnT) = sin(2pi*i/1024)
        m_SinArray[i] = sin(((float) i / 1024.0) * (2 * pi));

        // Sawtooth
        m_SawtoothArray[i] = i < 512 ? ms1 * i + bs1 : ms2 * (i - 511) + bs2;

        // Triangle
        if (i < 256)
            m_TriangleArray[i] = mt1 * i + bt1; // mx + b; rising edge 1
        else if (i >= 256 && i < 768)
            m_TriangleArray[i] = mtf2 * (i - 256) + btf2; // mx + b; falling edge
        else
            m_TriangleArray[i] = mt2 * (i - 768) + bt2; // mx + b; rising edge 2

        // square:
        m_SquareArray[i] = i < 512 ? +1.0 : -1.0;

        // zero to start, then loops build the rest
        m_SawtoothArray_BL5[i] = 0.0;
        m_SquareArray_BL5[i] = 0.0;
        m_TriangleArray_BL5[i] = 0.0;

        // sawtooth: += (-1)^g+1(1/g)sin(wnT)
        for (int g = 1; g <= 6; g++) {
            double n = double(g);
            m_SawtoothArray_BL5[i] += pow((float) -1.0, (float) (g + 1)) * (1.0 / n) * sin(2.0 * pi * i * n / 1024.0);
        }

        // triangle: += (-1)^g(1/(2g+1+^2)sin(w(2n+1)T)
        // NOTE: the limit is 3 here because of the way the sum is constructed
        // (look at the (2n+1) components
        for (int g = 0; g <= 3; g++) {
            double n = double(g);
            m_TriangleArray_BL5[i] += pow((float) -1.0, (float) n) * (1.0 / pow((float) (2 * n + 1), (float) 2.0)) *
                                      sin(2.0 * pi * (2.0 * n + 1) * i / 1024.0);
        }

        // square: += (1/g)sin(wnT)
        for (int g = 1; g <= 5; g += 2) {
            double n = double(g);
            m_SquareArray_BL5[i] += (1.0 / n) * sin(2.0 * pi * i * n / 1024.0);
        }

        // store the max values
        if (i == 0) {
            fMaxSaw = m_SawtoothArray_BL5[i];
            fMaxTri = m_TriangleArray_BL5[i];
            fMaxSqr = m_SquareArray_BL5[i];
        }
        else {
            // test and store
            if (m_SawtoothArray_BL5[i] > fMaxSaw)
                fMaxSaw = m_SawtoothArray_BL5[i];

            if (m_TriangleArray_BL5[i] > fMaxTri)
                fMaxTri = m_TriangleArray_BL5[i];

            if (m_SquareArray_BL5[i] > fMaxSqr)
                fMaxSqr = m_SquareArray_BL5[i];
        }
    }

    // normalize the bandlimited tables
    for (int i = 0; i < 1024; i++) {
        // normalize it
        m_SawtoothArray_BL5[i] /= fMaxSaw;
        m_TriangleArray_BL5[i] /= fMaxTri;
        m_SquareArray_BL5[i] /= fMaxSqr;
    }

    // clear variables
    m_fReadIndex = 0.0;
    m_fQuadPhaseReadIndex = 0.0;
    m_f_inc = 0.0;

    // silent
    m_bNoteOn = false;

    // norm phase
    m_bInvert = false;

    // initialize inc
    cookFrequency();

}


/* destructor()
	Destroy variables allocated in the contructor()

*/
CWTOscillator::~CWTOscillator(void) {


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
bool __stdcall CWTOscillator::initialize() {
    // Add your code here

    return true;
}

// cooking function
void CWTOscillator::cookFrequency() {
    // inc = L*fd/fs
    m_f_inc = 1024.0 * m_fFrequency_Hz / (float) m_nSampleRate;
}

// funciton to do the Oscillator; may be called by an external parent Plug-In too
//	pYn is the normal output
//	pYqn is the quad phase output
void CWTOscillator::doOscillate(float *pYn, float *pYqn) {
    // IMPORTANT: for future modules, bail out if no note-on event!
    if (!m_bNoteOn) {
        *pYn = 0.0;
        *pYqn = 0.0;
        return;
    }

    // our output value for this cycle
    float fOutSample = 0;
    float fQuadPhaseOutSample = 0;

    // get INT part
    int nReadIndex = (int) m_fReadIndex;
    int nQuadPhaseReadIndex = (int) m_fQuadPhaseReadIndex;

    // get FRAC part � NOTE no Quad Phase frac is needed because it will be the same!
    float fFrac = m_fReadIndex - nReadIndex;

    // setup second index for interpolation; wrap the buffer if needed
    int nReadIndexNext = nReadIndex + 1 > 1023 ? 0 : nReadIndex + 1;
    int nQuadPhaseReadIndexNext = nQuadPhaseReadIndex + 1 > 1023 ? 0 : nQuadPhaseReadIndex + 1;

    // interpolate the output
    switch (m_uOscType) {
        case sine:
            fOutSample = dLinTerp(0, 1, m_SinArray[nReadIndex], m_SinArray[nReadIndexNext], fFrac);
            fQuadPhaseOutSample = dLinTerp(0, 1, m_SinArray[nQuadPhaseReadIndex], m_SinArray[nQuadPhaseReadIndexNext],
                                           fFrac);
            break;

        case saw:
            if (m_uTableMode == normal)    // normal
            {
                fOutSample = dLinTerp(0, 1, m_SawtoothArray[nReadIndex], m_SawtoothArray[nReadIndexNext], fFrac);
                fQuadPhaseOutSample = dLinTerp(0, 1, m_SawtoothArray[nQuadPhaseReadIndex],
                                               m_SawtoothArray[nQuadPhaseReadIndexNext], fFrac);
            }
            else                        // bandlimited
            {
                fOutSample = dLinTerp(0, 1, m_SawtoothArray_BL5[nReadIndex], m_SawtoothArray_BL5[nReadIndexNext],
                                      fFrac);
                fQuadPhaseOutSample = dLinTerp(0, 1, m_SawtoothArray_BL5[nQuadPhaseReadIndex],
                                               m_SawtoothArray_BL5[nQuadPhaseReadIndexNext], fFrac);
            }
            break;

        case tri:
            if (m_uTableMode == normal)    // normal
            {
                fOutSample = dLinTerp(0, 1, m_TriangleArray[nReadIndex], m_TriangleArray[nReadIndexNext], fFrac);
                fQuadPhaseOutSample = dLinTerp(0, 1, m_TriangleArray[nQuadPhaseReadIndex],
                                               m_TriangleArray[nQuadPhaseReadIndexNext], fFrac);
            }
            else                        // bandlimited
            {
                fOutSample = dLinTerp(0, 1, m_TriangleArray_BL5[nReadIndex], m_TriangleArray_BL5[nReadIndexNext],
                                      fFrac);
                fQuadPhaseOutSample = dLinTerp(0, 1, m_TriangleArray_BL5[nQuadPhaseReadIndex],
                                               m_TriangleArray_BL5[nQuadPhaseReadIndexNext], fFrac);
            }
            break;

        case square:
            if (m_uTableMode == normal)    // normal
            {
                fOutSample = dLinTerp(0, 1, m_SquareArray[nReadIndex], m_SquareArray[nReadIndexNext], fFrac);
                fQuadPhaseOutSample = dLinTerp(0, 1, m_SquareArray[nQuadPhaseReadIndex],
                                               m_SquareArray[nQuadPhaseReadIndexNext], fFrac);
            }
            else                        // bandlimited
            {
                fOutSample = dLinTerp(0, 1, m_SquareArray_BL5[nReadIndex], m_SquareArray_BL5[nReadIndexNext], fFrac);
                fQuadPhaseOutSample = dLinTerp(0, 1, m_SquareArray_BL5[nQuadPhaseReadIndex],
                                               m_SquareArray_BL5[nQuadPhaseReadIndexNext], fFrac);
            }
            break;

            // always need a default
        default:
            fOutSample = dLinTerp(0, 1, m_SinArray[nReadIndex], m_SinArray[nReadIndexNext], fFrac);
            fQuadPhaseOutSample = dLinTerp(0, 1, m_SinArray[nQuadPhaseReadIndex], m_SinArray[nQuadPhaseReadIndexNext],
                                           fFrac);
            break;
    }

    // add the increment for next time
    m_fReadIndex += m_f_inc;
    m_fQuadPhaseReadIndex += m_f_inc;

    // check for wrap
    if (m_fReadIndex >= 1024)
        m_fReadIndex = m_fReadIndex - 1024;

    if (m_fQuadPhaseReadIndex >= 1024)
        m_fQuadPhaseReadIndex = m_fQuadPhaseReadIndex - 1024;

    // invert?
    if (m_bInvert) {
        fOutSample *= -1.0;
        fQuadPhaseOutSample *= -1.0;
    }

    // write out
    *pYn = fOutSample;
    *pYqn = fQuadPhaseOutSample;

    // create unipolar; div 2 then shift up 0.5
    if (m_uPolarity == unipolar) {
        *pYn /= 2.0;
        *pYn += 0.5;

        *pYqn /= 2.0;
        *pYqn += 0.5;
    }

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
bool __stdcall CWTOscillator::prepareForPlay() {
    // Add your code here:
    // reset the index
    reset();

    // cook curent frequency
    cookFrequency();

    return true;
}


/* processAudioFrame

// ALL VALUES IN AND OUT ON THE RANGE OF -1.0 TO + 1.0

LEFT INPUT = pInputBuffer[0];
RIGHT INPUT = pInputBuffer[1]

LEFT INPUT = pInputBuffer[0]
LEFT OUTPUT = pOutputBuffer[1]

*/
bool __stdcall CWTOscillator::processAudioFrame(float *pInputBuffer, float *pOutputBuffer, UINT uNumInputChannels,
                                                UINT uNumOutputChannels) {
    // Do LEFT (MONO) Channel
    // if not running, write 0s and bail
    if (!m_bNoteOn) {
        pOutputBuffer[0] = 0.0;
        if (uNumOutputChannels == 2)
            pOutputBuffer[1] = 0.0;

        return true;
    }

    // some intermediate variables for return
    float fY = 0;
    float fYq = 0;

    // call the oscilator function, return values into fY and fYq
    doOscillate(&fY, &fYq);

    // write fY to the Left
    pOutputBuffer[0] = fY;

    // write fYq to the Right
    if (uNumOutputChannels == 2) {
        pOutputBuffer[1] = fYq;
    }

    return true;
}


/* ADDED BY RACKAFX -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
   	**--0x2983--**

	Variable Name                    Index
-----------------------------------------------
	m_fFrequency_Hz                   0
	m_uOscType                        41
	m_uTableMode                      42
	m_uPolarity                       43

	Assignable Buttons               Index
-----------------------------------------------
	Note On                           50
	Note Off                          51
	B3                                52

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
bool __stdcall CWTOscillator::userInterfaceChange(int nControlIndex) {
    // decode the control index, or delete the switch and use brute force calls
    // add your code here
    switch (nControlIndex) {
        case 0:
            cookFrequency();
            break;

            // note on
        case 50:
            reset();
            cookFrequency();
            m_bNoteOn = true;
            break;

            // note off
        case 51:
            m_bNoteOn = false;
            break;

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
bool __stdcall CWTOscillator::joystickControlChange(float fControlA, float fControlB, float fControlC, float fControlD,
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
bool __stdcall CWTOscillator::processRackAFXAudioBuffer(float *pInputBuffer, float *pOutputBuffer,
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
bool __stdcall CWTOscillator::processVSTAudioBuffer(float **ppInputs, float **ppOutputs,
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

bool __stdcall CWTOscillator::midiNoteOn(UINT uChannel, UINT uMIDINote, UINT uVelocity) {
    return true;
}

bool __stdcall CWTOscillator::midiNoteOff(UINT uChannel, UINT uMIDINote, UINT uVelocity, bool bAllNotesOff) {
    return true;
}

// uModValue = 0->127
bool __stdcall CWTOscillator::midiModWheel(UINT uChannel, UINT uModValue) {
    return true;
}

// nActualPitchBendValue 		= -8192 -> +8191, 0 at center
// fNormalizedPitchBendValue 	= -1.0  -> +1.0,  0 at center
bool __stdcall CWTOscillator::midiPitchBend(UINT uChannel, int nActualPitchBendValue, float fNormalizedPitchBendValue) {
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
bool __stdcall CWTOscillator::midiClock() {

    return true;
}

// any midi message other than note on, note off, pitchbend, mod wheel or clock
bool __stdcall CWTOscillator::midiMessage(unsigned char cChannel, unsigned char cStatus, unsigned char
cData1, unsigned char cData2) {
    return true;
}


// DO NOT DELETE THIS FUNCTION --------------------------------------------------- //
bool __stdcall CWTOscillator::initUI() {
    // ADDED BY RACKAFX -- DO NOT EDIT THIS CODE!!! ------------------------------ //
    if (m_UIControlList.count() > 0)
        return true;

// **--0xDEA7--**

    m_fFrequency_Hz = 440.000000;
    CUICtrl ui0;
    ui0.uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
    ui0.uControlId = 0;
    ui0.bLogSlider = false;
    ui0.bExpSlider = false;
    ui0.fUserDisplayDataLoLimit = 25.000000;
    ui0.fUserDisplayDataHiLimit = 4200.000000;
    ui0.uUserDataType = floatData;
    ui0.fInitUserIntValue = 0;
    ui0.fInitUserFloatValue = 440.000000;
    ui0.fInitUserDoubleValue = 0;
    ui0.fInitUserUINTValue = 0;
    ui0.m_pUserCookedIntData = NULL;
    ui0.m_pUserCookedFloatData = &m_fFrequency_Hz;
    ui0.m_pUserCookedDoubleData = NULL;
    ui0.m_pUserCookedUINTData = NULL;
    ui0.cControlUnits = "Hz                                                              ";
    ui0.cVariableName = "m_fFrequency_Hz";
    ui0.cEnumeratedList = "SEL1,SEL2,SEL3";
    ui0.dPresetData[0] = 440.000000;
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
    ui0.cControlName = "Frequency";
    ui0.bOwnerControl = false;
    ui0.bMIDIControl = false;
    ui0.uMIDIControlCommand = 176;
    ui0.uMIDIControlName = 3;
    ui0.uMIDIControlChannel = 0;
    ui0.nGUIRow = 1;
    ui0.nGUIColumn = 1;
    ui0.uControlTheme[0] = 1;
    ui0.uControlTheme[1] = 9;
    ui0.uControlTheme[2] = 0;
    ui0.uControlTheme[3] = 0;
    ui0.uControlTheme[4] = 0;
    ui0.uControlTheme[5] = 1;
    ui0.uControlTheme[6] = 0;
    ui0.uControlTheme[7] = 0;
    ui0.uControlTheme[8] = 16777215;
    ui0.uControlTheme[9] = 11119017;
    ui0.uControlTheme[10] = 1;
    ui0.uControlTheme[11] = 12632256;
    ui0.uControlTheme[12] = 1;
    ui0.uControlTheme[13] = 6316128;
    ui0.uControlTheme[14] = 3;
    ui0.uControlTheme[15] = 8421504;
    ui0.uControlTheme[16] = 14772545;
    ui0.uControlTheme[17] = 1;
    ui0.uControlTheme[18] = 0;
    ui0.uControlTheme[19] = 0;
    ui0.uControlTheme[20] = 0;
    ui0.uControlTheme[21] = 0;
    ui0.uControlTheme[22] = 0;
    ui0.uControlTheme[23] = 19;
    ui0.uControlTheme[24] = 17;
    ui0.uControlTheme[25] = 0;
    ui0.uControlTheme[26] = 0;
    ui0.uControlTheme[27] = 0;
    ui0.uControlTheme[28] = 0;
    ui0.uControlTheme[29] = 0;
    ui0.uControlTheme[30] = 0;
    ui0.uControlTheme[31] = 0;
    ui0.uFluxCapControl[0] = 0;
    ui0.uFluxCapControl[1] = 0;
    ui0.uFluxCapControl[2] = 0;
    ui0.uFluxCapControl[3] = 0;
    ui0.uFluxCapControl[4] = 0;
    ui0.uFluxCapControl[5] = 0;
    ui0.uFluxCapControl[6] = 0;
    ui0.uFluxCapControl[7] = 0;
    ui0.uFluxCapControl[8] = 0;
    ui0.uFluxCapControl[9] = 0;
    ui0.uFluxCapControl[10] = 0;
    ui0.uFluxCapControl[11] = 0;
    ui0.uFluxCapControl[12] = 0;
    ui0.uFluxCapControl[13] = 0;
    ui0.uFluxCapControl[14] = 0;
    ui0.uFluxCapControl[15] = 0;
    ui0.uFluxCapControl[16] = 0;
    ui0.uFluxCapControl[17] = 0;
    ui0.uFluxCapControl[18] = 0;
    ui0.uFluxCapControl[19] = 0;
    ui0.uFluxCapControl[20] = 0;
    ui0.uFluxCapControl[21] = 0;
    ui0.uFluxCapControl[22] = 0;
    ui0.uFluxCapControl[23] = 0;
    ui0.uFluxCapControl[24] = 0;
    ui0.uFluxCapControl[25] = 0;
    ui0.uFluxCapControl[26] = 0;
    ui0.uFluxCapControl[27] = 0;
    ui0.uFluxCapControl[28] = 0;
    ui0.uFluxCapControl[29] = 0;
    ui0.uFluxCapControl[30] = 0;
    ui0.uFluxCapControl[31] = 0;
    ui0.uFluxCapControl[32] = 0;
    ui0.uFluxCapControl[33] = 0;
    ui0.uFluxCapControl[34] = 0;
    ui0.uFluxCapControl[35] = 0;
    ui0.uFluxCapControl[36] = 0;
    ui0.uFluxCapControl[37] = 0;
    ui0.uFluxCapControl[38] = 0;
    ui0.uFluxCapControl[39] = 0;
    ui0.uFluxCapControl[40] = 0;
    ui0.uFluxCapControl[41] = 0;
    ui0.uFluxCapControl[42] = 0;
    ui0.uFluxCapControl[43] = 0;
    ui0.uFluxCapControl[44] = 0;
    ui0.uFluxCapControl[45] = 0;
    ui0.uFluxCapControl[46] = 0;
    ui0.uFluxCapControl[47] = 0;
    ui0.uFluxCapControl[48] = 0;
    ui0.uFluxCapControl[49] = 0;
    ui0.uFluxCapControl[50] = 0;
    ui0.uFluxCapControl[51] = 0;
    ui0.uFluxCapControl[52] = 0;
    ui0.uFluxCapControl[53] = 0;
    ui0.uFluxCapControl[54] = 0;
    ui0.uFluxCapControl[55] = 0;
    ui0.uFluxCapControl[56] = 0;
    ui0.uFluxCapControl[57] = 0;
    ui0.uFluxCapControl[58] = 0;
    ui0.uFluxCapControl[59] = 0;
    ui0.uFluxCapControl[60] = 0;
    ui0.uFluxCapControl[61] = 0;
    ui0.uFluxCapControl[62] = 0;
    ui0.uFluxCapControl[63] = 0;
    ui0.fFluxCapData[0] = 0.000000;
    ui0.fFluxCapData[1] = 0.000000;
    ui0.fFluxCapData[2] = 0.000000;
    ui0.fFluxCapData[3] = 0.000000;
    ui0.fFluxCapData[4] = 0.000000;
    ui0.fFluxCapData[5] = 0.000000;
    ui0.fFluxCapData[6] = 0.000000;
    ui0.fFluxCapData[7] = 0.000000;
    ui0.fFluxCapData[8] = 0.000000;
    ui0.fFluxCapData[9] = 0.000000;
    ui0.fFluxCapData[10] = 0.000000;
    ui0.fFluxCapData[11] = 0.000000;
    ui0.fFluxCapData[12] = 0.000000;
    ui0.fFluxCapData[13] = 0.000000;
    ui0.fFluxCapData[14] = 0.000000;
    ui0.fFluxCapData[15] = 0.000000;
    ui0.fFluxCapData[16] = 0.000000;
    ui0.fFluxCapData[17] = 0.000000;
    ui0.fFluxCapData[18] = 0.000000;
    ui0.fFluxCapData[19] = 0.000000;
    ui0.fFluxCapData[20] = 0.000000;
    ui0.fFluxCapData[21] = 0.000000;
    ui0.fFluxCapData[22] = 0.000000;
    ui0.fFluxCapData[23] = 0.000000;
    ui0.fFluxCapData[24] = 0.000000;
    ui0.fFluxCapData[25] = 0.000000;
    ui0.fFluxCapData[26] = 0.000000;
    ui0.fFluxCapData[27] = 0.000000;
    ui0.fFluxCapData[28] = 0.000000;
    ui0.fFluxCapData[29] = 0.000000;
    ui0.fFluxCapData[30] = 0.000000;
    ui0.fFluxCapData[31] = 0.000000;
    ui0.fFluxCapData[32] = 0.000000;
    ui0.fFluxCapData[33] = 0.000000;
    ui0.fFluxCapData[34] = 0.000000;
    ui0.fFluxCapData[35] = 0.000000;
    ui0.fFluxCapData[36] = 0.000000;
    ui0.fFluxCapData[37] = 0.000000;
    ui0.fFluxCapData[38] = 0.000000;
    ui0.fFluxCapData[39] = 0.000000;
    ui0.fFluxCapData[40] = 0.000000;
    ui0.fFluxCapData[41] = 0.000000;
    ui0.fFluxCapData[42] = 0.000000;
    ui0.fFluxCapData[43] = 0.000000;
    ui0.fFluxCapData[44] = 0.000000;
    ui0.fFluxCapData[45] = 0.000000;
    ui0.fFluxCapData[46] = 0.000000;
    ui0.fFluxCapData[47] = 0.000000;
    ui0.fFluxCapData[48] = 0.000000;
    ui0.fFluxCapData[49] = 0.000000;
    ui0.fFluxCapData[50] = 0.000000;
    ui0.fFluxCapData[51] = 0.000000;
    ui0.fFluxCapData[52] = 0.000000;
    ui0.fFluxCapData[53] = 0.000000;
    ui0.fFluxCapData[54] = 0.000000;
    ui0.fFluxCapData[55] = 0.000000;
    ui0.fFluxCapData[56] = 0.000000;
    ui0.fFluxCapData[57] = 0.000000;
    ui0.fFluxCapData[58] = 0.000000;
    ui0.fFluxCapData[59] = 0.000000;
    ui0.fFluxCapData[60] = 0.000000;
    ui0.fFluxCapData[61] = 0.000000;
    ui0.fFluxCapData[62] = 0.000000;
    ui0.fFluxCapData[63] = 0.000000;
    m_UIControlList.append(ui0);


    m_uOscType = 0;
    CUICtrl ui1;
    ui1.uControlType = FILTER_CONTROL_RADIO_SWITCH_VARIABLE;
    ui1.uControlId = 41;
    ui1.bLogSlider = false;
    ui1.bExpSlider = false;
    ui1.fUserDisplayDataLoLimit = 0.000000;
    ui1.fUserDisplayDataHiLimit = 3.000000;
    ui1.uUserDataType = UINTData;
    ui1.fInitUserIntValue = 0;
    ui1.fInitUserFloatValue = 0;
    ui1.fInitUserDoubleValue = 0;
    ui1.fInitUserUINTValue = 0.000000;
    ui1.m_pUserCookedIntData = NULL;
    ui1.m_pUserCookedFloatData = NULL;
    ui1.m_pUserCookedDoubleData = NULL;
    ui1.m_pUserCookedUINTData = &m_uOscType;
    ui1.cControlUnits = "";
    ui1.cVariableName = "m_uOscType";
    ui1.cEnumeratedList = "sine,saw,tri,square";
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
    ui1.cControlName = "Waveform";
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
    ui1.uControlTheme[4] = 16777215;
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
    ui1.uControlTheme[22] = 1;
    ui1.uControlTheme[23] = 109;
    ui1.uControlTheme[24] = 12;
    ui1.uControlTheme[25] = 0;
    ui1.uControlTheme[26] = 0;
    ui1.uControlTheme[27] = 0;
    ui1.uControlTheme[28] = 0;
    ui1.uControlTheme[29] = 0;
    ui1.uControlTheme[30] = 0;
    ui1.uControlTheme[31] = 0;
    ui1.uFluxCapControl[0] = 0;
    ui1.uFluxCapControl[1] = 0;
    ui1.uFluxCapControl[2] = 0;
    ui1.uFluxCapControl[3] = 0;
    ui1.uFluxCapControl[4] = 0;
    ui1.uFluxCapControl[5] = 0;
    ui1.uFluxCapControl[6] = 0;
    ui1.uFluxCapControl[7] = 0;
    ui1.uFluxCapControl[8] = 0;
    ui1.uFluxCapControl[9] = 0;
    ui1.uFluxCapControl[10] = 0;
    ui1.uFluxCapControl[11] = 0;
    ui1.uFluxCapControl[12] = 0;
    ui1.uFluxCapControl[13] = 0;
    ui1.uFluxCapControl[14] = 0;
    ui1.uFluxCapControl[15] = 0;
    ui1.uFluxCapControl[16] = 0;
    ui1.uFluxCapControl[17] = 0;
    ui1.uFluxCapControl[18] = 0;
    ui1.uFluxCapControl[19] = 0;
    ui1.uFluxCapControl[20] = 0;
    ui1.uFluxCapControl[21] = 0;
    ui1.uFluxCapControl[22] = 0;
    ui1.uFluxCapControl[23] = 0;
    ui1.uFluxCapControl[24] = 0;
    ui1.uFluxCapControl[25] = 0;
    ui1.uFluxCapControl[26] = 0;
    ui1.uFluxCapControl[27] = 0;
    ui1.uFluxCapControl[28] = 0;
    ui1.uFluxCapControl[29] = 0;
    ui1.uFluxCapControl[30] = 0;
    ui1.uFluxCapControl[31] = 0;
    ui1.uFluxCapControl[32] = 0;
    ui1.uFluxCapControl[33] = 0;
    ui1.uFluxCapControl[34] = 0;
    ui1.uFluxCapControl[35] = 0;
    ui1.uFluxCapControl[36] = 0;
    ui1.uFluxCapControl[37] = 0;
    ui1.uFluxCapControl[38] = 0;
    ui1.uFluxCapControl[39] = 0;
    ui1.uFluxCapControl[40] = 0;
    ui1.uFluxCapControl[41] = 0;
    ui1.uFluxCapControl[42] = 0;
    ui1.uFluxCapControl[43] = 0;
    ui1.uFluxCapControl[44] = 0;
    ui1.uFluxCapControl[45] = 0;
    ui1.uFluxCapControl[46] = 0;
    ui1.uFluxCapControl[47] = 0;
    ui1.uFluxCapControl[48] = 0;
    ui1.uFluxCapControl[49] = 0;
    ui1.uFluxCapControl[50] = 0;
    ui1.uFluxCapControl[51] = 0;
    ui1.uFluxCapControl[52] = 0;
    ui1.uFluxCapControl[53] = 0;
    ui1.uFluxCapControl[54] = 0;
    ui1.uFluxCapControl[55] = 0;
    ui1.uFluxCapControl[56] = 0;
    ui1.uFluxCapControl[57] = 0;
    ui1.uFluxCapControl[58] = 0;
    ui1.uFluxCapControl[59] = 0;
    ui1.uFluxCapControl[60] = 0;
    ui1.uFluxCapControl[61] = 0;
    ui1.uFluxCapControl[62] = 0;
    ui1.uFluxCapControl[63] = 0;
    ui1.fFluxCapData[0] = 0.000000;
    ui1.fFluxCapData[1] = 0.000000;
    ui1.fFluxCapData[2] = 0.000000;
    ui1.fFluxCapData[3] = 0.000000;
    ui1.fFluxCapData[4] = 0.000000;
    ui1.fFluxCapData[5] = 0.000000;
    ui1.fFluxCapData[6] = 0.000000;
    ui1.fFluxCapData[7] = 0.000000;
    ui1.fFluxCapData[8] = 0.000000;
    ui1.fFluxCapData[9] = 0.000000;
    ui1.fFluxCapData[10] = 0.000000;
    ui1.fFluxCapData[11] = 0.000000;
    ui1.fFluxCapData[12] = 0.000000;
    ui1.fFluxCapData[13] = 0.000000;
    ui1.fFluxCapData[14] = 0.000000;
    ui1.fFluxCapData[15] = 0.000000;
    ui1.fFluxCapData[16] = 0.000000;
    ui1.fFluxCapData[17] = 0.000000;
    ui1.fFluxCapData[18] = 0.000000;
    ui1.fFluxCapData[19] = 0.000000;
    ui1.fFluxCapData[20] = 0.000000;
    ui1.fFluxCapData[21] = 0.000000;
    ui1.fFluxCapData[22] = 0.000000;
    ui1.fFluxCapData[23] = 0.000000;
    ui1.fFluxCapData[24] = 0.000000;
    ui1.fFluxCapData[25] = 0.000000;
    ui1.fFluxCapData[26] = 0.000000;
    ui1.fFluxCapData[27] = 0.000000;
    ui1.fFluxCapData[28] = 0.000000;
    ui1.fFluxCapData[29] = 0.000000;
    ui1.fFluxCapData[30] = 0.000000;
    ui1.fFluxCapData[31] = 0.000000;
    ui1.fFluxCapData[32] = 0.000000;
    ui1.fFluxCapData[33] = 0.000000;
    ui1.fFluxCapData[34] = 0.000000;
    ui1.fFluxCapData[35] = 0.000000;
    ui1.fFluxCapData[36] = 0.000000;
    ui1.fFluxCapData[37] = 0.000000;
    ui1.fFluxCapData[38] = 0.000000;
    ui1.fFluxCapData[39] = 0.000000;
    ui1.fFluxCapData[40] = 0.000000;
    ui1.fFluxCapData[41] = 0.000000;
    ui1.fFluxCapData[42] = 0.000000;
    ui1.fFluxCapData[43] = 0.000000;
    ui1.fFluxCapData[44] = 0.000000;
    ui1.fFluxCapData[45] = 0.000000;
    ui1.fFluxCapData[46] = 0.000000;
    ui1.fFluxCapData[47] = 0.000000;
    ui1.fFluxCapData[48] = 0.000000;
    ui1.fFluxCapData[49] = 0.000000;
    ui1.fFluxCapData[50] = 0.000000;
    ui1.fFluxCapData[51] = 0.000000;
    ui1.fFluxCapData[52] = 0.000000;
    ui1.fFluxCapData[53] = 0.000000;
    ui1.fFluxCapData[54] = 0.000000;
    ui1.fFluxCapData[55] = 0.000000;
    ui1.fFluxCapData[56] = 0.000000;
    ui1.fFluxCapData[57] = 0.000000;
    ui1.fFluxCapData[58] = 0.000000;
    ui1.fFluxCapData[59] = 0.000000;
    ui1.fFluxCapData[60] = 0.000000;
    ui1.fFluxCapData[61] = 0.000000;
    ui1.fFluxCapData[62] = 0.000000;
    ui1.fFluxCapData[63] = 0.000000;
    m_UIControlList.append(ui1);


    m_uTableMode = 0;
    CUICtrl ui2;
    ui2.uControlType = FILTER_CONTROL_RADIO_SWITCH_VARIABLE;
    ui2.uControlId = 42;
    ui2.bLogSlider = false;
    ui2.bExpSlider = false;
    ui2.fUserDisplayDataLoLimit = 0.000000;
    ui2.fUserDisplayDataHiLimit = 1.000000;
    ui2.uUserDataType = UINTData;
    ui2.fInitUserIntValue = 0;
    ui2.fInitUserFloatValue = 0;
    ui2.fInitUserDoubleValue = 0;
    ui2.fInitUserUINTValue = 0.000000;
    ui2.m_pUserCookedIntData = NULL;
    ui2.m_pUserCookedFloatData = NULL;
    ui2.m_pUserCookedDoubleData = NULL;
    ui2.m_pUserCookedUINTData = &m_uTableMode;
    ui2.cControlUnits = "";
    ui2.cVariableName = "m_uTableMode";
    ui2.cEnumeratedList = "normal,bandlimit";
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
    ui2.cControlName = "Mode";
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
    ui2.uControlTheme[4] = 16777215;
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
    ui2.uControlTheme[22] = 1;
    ui2.uControlTheme[23] = 190;
    ui2.uControlTheme[24] = 12;
    ui2.uControlTheme[25] = 0;
    ui2.uControlTheme[26] = 0;
    ui2.uControlTheme[27] = 0;
    ui2.uControlTheme[28] = 0;
    ui2.uControlTheme[29] = 0;
    ui2.uControlTheme[30] = 0;
    ui2.uControlTheme[31] = 0;
    ui2.uFluxCapControl[0] = 0;
    ui2.uFluxCapControl[1] = 0;
    ui2.uFluxCapControl[2] = 0;
    ui2.uFluxCapControl[3] = 0;
    ui2.uFluxCapControl[4] = 0;
    ui2.uFluxCapControl[5] = 0;
    ui2.uFluxCapControl[6] = 0;
    ui2.uFluxCapControl[7] = 0;
    ui2.uFluxCapControl[8] = 0;
    ui2.uFluxCapControl[9] = 0;
    ui2.uFluxCapControl[10] = 0;
    ui2.uFluxCapControl[11] = 0;
    ui2.uFluxCapControl[12] = 0;
    ui2.uFluxCapControl[13] = 0;
    ui2.uFluxCapControl[14] = 0;
    ui2.uFluxCapControl[15] = 0;
    ui2.uFluxCapControl[16] = 0;
    ui2.uFluxCapControl[17] = 0;
    ui2.uFluxCapControl[18] = 0;
    ui2.uFluxCapControl[19] = 0;
    ui2.uFluxCapControl[20] = 0;
    ui2.uFluxCapControl[21] = 0;
    ui2.uFluxCapControl[22] = 0;
    ui2.uFluxCapControl[23] = 0;
    ui2.uFluxCapControl[24] = 0;
    ui2.uFluxCapControl[25] = 0;
    ui2.uFluxCapControl[26] = 0;
    ui2.uFluxCapControl[27] = 0;
    ui2.uFluxCapControl[28] = 0;
    ui2.uFluxCapControl[29] = 0;
    ui2.uFluxCapControl[30] = 0;
    ui2.uFluxCapControl[31] = 0;
    ui2.uFluxCapControl[32] = 0;
    ui2.uFluxCapControl[33] = 0;
    ui2.uFluxCapControl[34] = 0;
    ui2.uFluxCapControl[35] = 0;
    ui2.uFluxCapControl[36] = 0;
    ui2.uFluxCapControl[37] = 0;
    ui2.uFluxCapControl[38] = 0;
    ui2.uFluxCapControl[39] = 0;
    ui2.uFluxCapControl[40] = 0;
    ui2.uFluxCapControl[41] = 0;
    ui2.uFluxCapControl[42] = 0;
    ui2.uFluxCapControl[43] = 0;
    ui2.uFluxCapControl[44] = 0;
    ui2.uFluxCapControl[45] = 0;
    ui2.uFluxCapControl[46] = 0;
    ui2.uFluxCapControl[47] = 0;
    ui2.uFluxCapControl[48] = 0;
    ui2.uFluxCapControl[49] = 0;
    ui2.uFluxCapControl[50] = 0;
    ui2.uFluxCapControl[51] = 0;
    ui2.uFluxCapControl[52] = 0;
    ui2.uFluxCapControl[53] = 0;
    ui2.uFluxCapControl[54] = 0;
    ui2.uFluxCapControl[55] = 0;
    ui2.uFluxCapControl[56] = 0;
    ui2.uFluxCapControl[57] = 0;
    ui2.uFluxCapControl[58] = 0;
    ui2.uFluxCapControl[59] = 0;
    ui2.uFluxCapControl[60] = 0;
    ui2.uFluxCapControl[61] = 0;
    ui2.uFluxCapControl[62] = 0;
    ui2.uFluxCapControl[63] = 0;
    ui2.fFluxCapData[0] = 0.000000;
    ui2.fFluxCapData[1] = 0.000000;
    ui2.fFluxCapData[2] = 0.000000;
    ui2.fFluxCapData[3] = 0.000000;
    ui2.fFluxCapData[4] = 0.000000;
    ui2.fFluxCapData[5] = 0.000000;
    ui2.fFluxCapData[6] = 0.000000;
    ui2.fFluxCapData[7] = 0.000000;
    ui2.fFluxCapData[8] = 0.000000;
    ui2.fFluxCapData[9] = 0.000000;
    ui2.fFluxCapData[10] = 0.000000;
    ui2.fFluxCapData[11] = 0.000000;
    ui2.fFluxCapData[12] = 0.000000;
    ui2.fFluxCapData[13] = 0.000000;
    ui2.fFluxCapData[14] = 0.000000;
    ui2.fFluxCapData[15] = 0.000000;
    ui2.fFluxCapData[16] = 0.000000;
    ui2.fFluxCapData[17] = 0.000000;
    ui2.fFluxCapData[18] = 0.000000;
    ui2.fFluxCapData[19] = 0.000000;
    ui2.fFluxCapData[20] = 0.000000;
    ui2.fFluxCapData[21] = 0.000000;
    ui2.fFluxCapData[22] = 0.000000;
    ui2.fFluxCapData[23] = 0.000000;
    ui2.fFluxCapData[24] = 0.000000;
    ui2.fFluxCapData[25] = 0.000000;
    ui2.fFluxCapData[26] = 0.000000;
    ui2.fFluxCapData[27] = 0.000000;
    ui2.fFluxCapData[28] = 0.000000;
    ui2.fFluxCapData[29] = 0.000000;
    ui2.fFluxCapData[30] = 0.000000;
    ui2.fFluxCapData[31] = 0.000000;
    ui2.fFluxCapData[32] = 0.000000;
    ui2.fFluxCapData[33] = 0.000000;
    ui2.fFluxCapData[34] = 0.000000;
    ui2.fFluxCapData[35] = 0.000000;
    ui2.fFluxCapData[36] = 0.000000;
    ui2.fFluxCapData[37] = 0.000000;
    ui2.fFluxCapData[38] = 0.000000;
    ui2.fFluxCapData[39] = 0.000000;
    ui2.fFluxCapData[40] = 0.000000;
    ui2.fFluxCapData[41] = 0.000000;
    ui2.fFluxCapData[42] = 0.000000;
    ui2.fFluxCapData[43] = 0.000000;
    ui2.fFluxCapData[44] = 0.000000;
    ui2.fFluxCapData[45] = 0.000000;
    ui2.fFluxCapData[46] = 0.000000;
    ui2.fFluxCapData[47] = 0.000000;
    ui2.fFluxCapData[48] = 0.000000;
    ui2.fFluxCapData[49] = 0.000000;
    ui2.fFluxCapData[50] = 0.000000;
    ui2.fFluxCapData[51] = 0.000000;
    ui2.fFluxCapData[52] = 0.000000;
    ui2.fFluxCapData[53] = 0.000000;
    ui2.fFluxCapData[54] = 0.000000;
    ui2.fFluxCapData[55] = 0.000000;
    ui2.fFluxCapData[56] = 0.000000;
    ui2.fFluxCapData[57] = 0.000000;
    ui2.fFluxCapData[58] = 0.000000;
    ui2.fFluxCapData[59] = 0.000000;
    ui2.fFluxCapData[60] = 0.000000;
    ui2.fFluxCapData[61] = 0.000000;
    ui2.fFluxCapData[62] = 0.000000;
    ui2.fFluxCapData[63] = 0.000000;
    m_UIControlList.append(ui2);


    m_uPolarity = 0;
    CUICtrl ui3;
    ui3.uControlType = FILTER_CONTROL_RADIO_SWITCH_VARIABLE;
    ui3.uControlId = 43;
    ui3.bLogSlider = false;
    ui3.bExpSlider = false;
    ui3.fUserDisplayDataLoLimit = 0.000000;
    ui3.fUserDisplayDataHiLimit = 1.000000;
    ui3.uUserDataType = UINTData;
    ui3.fInitUserIntValue = 0;
    ui3.fInitUserFloatValue = 0;
    ui3.fInitUserDoubleValue = 0;
    ui3.fInitUserUINTValue = 0.000000;
    ui3.m_pUserCookedIntData = NULL;
    ui3.m_pUserCookedFloatData = NULL;
    ui3.m_pUserCookedDoubleData = NULL;
    ui3.m_pUserCookedUINTData = &m_uPolarity;
    ui3.cControlUnits = "";
    ui3.cVariableName = "m_uPolarity";
    ui3.cEnumeratedList = "bipolar,unipolar";
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
    ui3.cControlName = "Polarity";
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
    ui3.uControlTheme[4] = 16777215;
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
    ui3.uControlTheme[22] = 1;
    ui3.uControlTheme[23] = 274;
    ui3.uControlTheme[24] = 12;
    ui3.uControlTheme[25] = 0;
    ui3.uControlTheme[26] = 0;
    ui3.uControlTheme[27] = 0;
    ui3.uControlTheme[28] = 0;
    ui3.uControlTheme[29] = 0;
    ui3.uControlTheme[30] = 0;
    ui3.uControlTheme[31] = 0;
    ui3.uFluxCapControl[0] = 0;
    ui3.uFluxCapControl[1] = 0;
    ui3.uFluxCapControl[2] = 0;
    ui3.uFluxCapControl[3] = 0;
    ui3.uFluxCapControl[4] = 0;
    ui3.uFluxCapControl[5] = 0;
    ui3.uFluxCapControl[6] = 0;
    ui3.uFluxCapControl[7] = 0;
    ui3.uFluxCapControl[8] = 0;
    ui3.uFluxCapControl[9] = 0;
    ui3.uFluxCapControl[10] = 0;
    ui3.uFluxCapControl[11] = 0;
    ui3.uFluxCapControl[12] = 0;
    ui3.uFluxCapControl[13] = 0;
    ui3.uFluxCapControl[14] = 0;
    ui3.uFluxCapControl[15] = 0;
    ui3.uFluxCapControl[16] = 0;
    ui3.uFluxCapControl[17] = 0;
    ui3.uFluxCapControl[18] = 0;
    ui3.uFluxCapControl[19] = 0;
    ui3.uFluxCapControl[20] = 0;
    ui3.uFluxCapControl[21] = 0;
    ui3.uFluxCapControl[22] = 0;
    ui3.uFluxCapControl[23] = 0;
    ui3.uFluxCapControl[24] = 0;
    ui3.uFluxCapControl[25] = 0;
    ui3.uFluxCapControl[26] = 0;
    ui3.uFluxCapControl[27] = 0;
    ui3.uFluxCapControl[28] = 0;
    ui3.uFluxCapControl[29] = 0;
    ui3.uFluxCapControl[30] = 0;
    ui3.uFluxCapControl[31] = 0;
    ui3.uFluxCapControl[32] = 0;
    ui3.uFluxCapControl[33] = 0;
    ui3.uFluxCapControl[34] = 0;
    ui3.uFluxCapControl[35] = 0;
    ui3.uFluxCapControl[36] = 0;
    ui3.uFluxCapControl[37] = 0;
    ui3.uFluxCapControl[38] = 0;
    ui3.uFluxCapControl[39] = 0;
    ui3.uFluxCapControl[40] = 0;
    ui3.uFluxCapControl[41] = 0;
    ui3.uFluxCapControl[42] = 0;
    ui3.uFluxCapControl[43] = 0;
    ui3.uFluxCapControl[44] = 0;
    ui3.uFluxCapControl[45] = 0;
    ui3.uFluxCapControl[46] = 0;
    ui3.uFluxCapControl[47] = 0;
    ui3.uFluxCapControl[48] = 0;
    ui3.uFluxCapControl[49] = 0;
    ui3.uFluxCapControl[50] = 0;
    ui3.uFluxCapControl[51] = 0;
    ui3.uFluxCapControl[52] = 0;
    ui3.uFluxCapControl[53] = 0;
    ui3.uFluxCapControl[54] = 0;
    ui3.uFluxCapControl[55] = 0;
    ui3.uFluxCapControl[56] = 0;
    ui3.uFluxCapControl[57] = 0;
    ui3.uFluxCapControl[58] = 0;
    ui3.uFluxCapControl[59] = 0;
    ui3.uFluxCapControl[60] = 0;
    ui3.uFluxCapControl[61] = 0;
    ui3.uFluxCapControl[62] = 0;
    ui3.uFluxCapControl[63] = 0;
    ui3.fFluxCapData[0] = 0.000000;
    ui3.fFluxCapData[1] = 0.000000;
    ui3.fFluxCapData[2] = 0.000000;
    ui3.fFluxCapData[3] = 0.000000;
    ui3.fFluxCapData[4] = 0.000000;
    ui3.fFluxCapData[5] = 0.000000;
    ui3.fFluxCapData[6] = 0.000000;
    ui3.fFluxCapData[7] = 0.000000;
    ui3.fFluxCapData[8] = 0.000000;
    ui3.fFluxCapData[9] = 0.000000;
    ui3.fFluxCapData[10] = 0.000000;
    ui3.fFluxCapData[11] = 0.000000;
    ui3.fFluxCapData[12] = 0.000000;
    ui3.fFluxCapData[13] = 0.000000;
    ui3.fFluxCapData[14] = 0.000000;
    ui3.fFluxCapData[15] = 0.000000;
    ui3.fFluxCapData[16] = 0.000000;
    ui3.fFluxCapData[17] = 0.000000;
    ui3.fFluxCapData[18] = 0.000000;
    ui3.fFluxCapData[19] = 0.000000;
    ui3.fFluxCapData[20] = 0.000000;
    ui3.fFluxCapData[21] = 0.000000;
    ui3.fFluxCapData[22] = 0.000000;
    ui3.fFluxCapData[23] = 0.000000;
    ui3.fFluxCapData[24] = 0.000000;
    ui3.fFluxCapData[25] = 0.000000;
    ui3.fFluxCapData[26] = 0.000000;
    ui3.fFluxCapData[27] = 0.000000;
    ui3.fFluxCapData[28] = 0.000000;
    ui3.fFluxCapData[29] = 0.000000;
    ui3.fFluxCapData[30] = 0.000000;
    ui3.fFluxCapData[31] = 0.000000;
    ui3.fFluxCapData[32] = 0.000000;
    ui3.fFluxCapData[33] = 0.000000;
    ui3.fFluxCapData[34] = 0.000000;
    ui3.fFluxCapData[35] = 0.000000;
    ui3.fFluxCapData[36] = 0.000000;
    ui3.fFluxCapData[37] = 0.000000;
    ui3.fFluxCapData[38] = 0.000000;
    ui3.fFluxCapData[39] = 0.000000;
    ui3.fFluxCapData[40] = 0.000000;
    ui3.fFluxCapData[41] = 0.000000;
    ui3.fFluxCapData[42] = 0.000000;
    ui3.fFluxCapData[43] = 0.000000;
    ui3.fFluxCapData[44] = 0.000000;
    ui3.fFluxCapData[45] = 0.000000;
    ui3.fFluxCapData[46] = 0.000000;
    ui3.fFluxCapData[47] = 0.000000;
    ui3.fFluxCapData[48] = 0.000000;
    ui3.fFluxCapData[49] = 0.000000;
    ui3.fFluxCapData[50] = 0.000000;
    ui3.fFluxCapData[51] = 0.000000;
    ui3.fFluxCapData[52] = 0.000000;
    ui3.fFluxCapData[53] = 0.000000;
    ui3.fFluxCapData[54] = 0.000000;
    ui3.fFluxCapData[55] = 0.000000;
    ui3.fFluxCapData[56] = 0.000000;
    ui3.fFluxCapData[57] = 0.000000;
    ui3.fFluxCapData[58] = 0.000000;
    ui3.fFluxCapData[59] = 0.000000;
    ui3.fFluxCapData[60] = 0.000000;
    ui3.fFluxCapData[61] = 0.000000;
    ui3.fFluxCapData[62] = 0.000000;
    ui3.fFluxCapData[63] = 0.000000;
    m_UIControlList.append(ui3);


    m_uX_TrackPadIndex = -1;
    m_uY_TrackPadIndex = -1;

    m_AssignButton1Name = "Note On";
    m_AssignButton2Name = "Note Off";
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


    // **--0xEDA5--**
// ------------------------------------------------------------------------------- //

    return true;

}





