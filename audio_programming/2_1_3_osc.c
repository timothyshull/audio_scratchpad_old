#include <stdio.h>
#include <stdlib.h>
#include <portsf.h>
#include <math.h>

enum {
    ARG_INFILE = 1,
    ARG_OUTFILE = 2,
    ARG_NARGS = 3
};

typedef uint64_t UINT;

const double pi = M_PI;

const int m_nSampleRate = 48000;

//32 64 128 256 512 1024 2048 4096

int exit_sf2float(int ifd, int ofd, float *frame, PSF_CHPEAK *peaks, int error);

float m_f_b1;
float m_f_b2;

float m_f_y_z1;
float m_f_y_z2;

float m_fFrequency_Hz;

int processAudioFrame(float *pInputBuffer, float *pOutputBuffer, UINT uNumInputChannels,
                       UINT uNumOutputChannels) {

    // otherwise, do the oscillator
    // do difference equation y(n) = -b1y(n-2) - b2y(n-2)
    pOutputBuffer[0] = -m_f_b1 * m_f_y_z1 - m_f_b2 * m_f_y_z2;

    // Mono-In, Stereo-Out (AUX Effect)
    if (uNumInputChannels == 1 && uNumOutputChannels == 2)
        pOutputBuffer[1] = pOutputBuffer[0];

    // Stereo-In, Stereo-Out (INSERT Effect)
    if (uNumInputChannels == 2 && uNumOutputChannels == 2)
        pOutputBuffer[1] = pOutputBuffer[0];

    // shuffle memory
    m_f_y_z2 = m_f_y_z1;
    m_f_y_z1 = pOutputBuffer[0];


    return 1;
}

void cookFrequency() {
    // Oscillation Rate = theta = wT = w/fs
    float f_wT = (2.0 * pi * m_fFrequency_Hz) / (float) m_nSampleRate;

    // coefficients according to design equations
    m_f_b1 = -2.0 * cos(f_wT);
    m_f_b2 = 1.0;

    // set initial conditions so that first sample out is 0.0
    // OLD WAY:
    // 	m_f_y_z1 = sin(-1.0*f_wT);	// sin(wnT) = sin(w(-1)T)
    // 	m_f_y_z2 = sin(-2.0*f_wT);	// sin(wnT) = sin(w(-2)T)

    // re calculate the new initial conditions
    // arcsine of y(n-1) gives us wnT
    double wnT1 = asin(m_f_y_z1);

    // find n by dividing wnT by wT
    float n = wnT1 / f_wT;

    // re calculate the new initial conditions
    // asin returns values from -pi/2 to +pi/2 where the sinusoid
    // moves from -1 to +1 -- the leading (rising) edge of the
    // sinewave. If we are on that leading edge (increasing)
    // then we use the value 1T behind.
    //
    // If we are on the falling edge, we use the value 1T ahead
    // because it mimics the value that would be 1T behind
    if (m_f_y_z1 > m_f_y_z2)
        n -= 1;
    else
        n += 1;

    // calculate the new (old) sample
    m_f_y_z2 = sin((n) * f_wT);
}


int main(int argc, char *argv[]) {
    PSF_PROPS props;
    long framesread;
    long totalread;

    int ofd = 1;
    int error = 0;
    PSF_CHPEAK *peaks = NULL;
    float *frame = NULL;
    int buffer_size = 32;

    printf("SF2FLOAT: convert soundfile to floats format\n");

    if (argc < ARG_NARGS) {
        printf("Insufficient arguments.\nUsage:\n\tsf2float infile outfile\n");
        exit(1);
    }

    if (psf_init()) {
        printf("Unable to start portsf\n");
        exit(1);
    }

    int ifd = psf_sndOpen(argv[ARG_INFILE], &props, 0);

    if (ifd < 0) {
        printf("Error: unable to open infile %s\n", argv[ARG_INFILE]);
        exit(1);
    }

    if (props.samptype == PSF_SAMP_IEEE_FLOAT) {
        printf("Info: input file is already in floats format.\n");
    }

    props.samptype = PSF_SAMP_IEEE_FLOAT;
    psf_format outformat = psf_getFormatExt(argv[ARG_OUTFILE]);
    if (outformat == PSF_FMT_UNKNOWN) {
        printf("outfile name %s has unknown format.\nAcceptable file types include: .wav, .aiff, .aif, .afc, .aifc\n",
               argv[ARG_OUTFILE]);
        error++;
        exit_sf2float(ifd, ofd, frame, peaks, error);
    }

    props.format = outformat;

    ofd = psf_sndCreate(argv[2], &props, 0, 0, PSF_CREATE_RDWR);
    if (ofd < 0) {
        printf("Error: unable to create outfile %s\n", argv[ARG_OUTFILE]);
        error++;
        exit_sf2float(ifd, ofd, frame, peaks, error);
    }

    frame = (float *) malloc(props.chans * sizeof(float) * buffer_size);

    if (frame == NULL) {
        puts("No memory!\n");
        error++;
        exit_sf2float(ifd, ofd, frame, peaks, error);
    }

    peaks = (PSF_CHPEAK *) malloc(props.chans * sizeof(PSF_CHPEAK));
    if (peaks == NULL) {
        puts("No memory!\n");
        error++;
        exit_sf2float(ifd, ofd, frame, peaks, error);
    }

    printf("copying...\n");

    framesread = psf_sndReadFloatFrames(ifd, frame, buffer_size);
    totalread = 0;

    while (framesread == buffer_size) {
        totalread += buffer_size;

        if (psf_sndWriteFloatFrames(ofd, frame, buffer_size) != buffer_size) {
            printf("Error writing to outfile\n");
            error++;
            break;
        }
        /* do any processing here */
        framesread = psf_sndReadFloatFrames(ifd, frame, buffer_size);
    }

    if (framesread < 0) {
        printf("Error reading infile. Outfile is incomplete.\n");
        error++;
    } else {
        printf("Done. %d sample frames copied to %s\n", (int) totalread, argv[ARG_OUTFILE]);
    }

    if (psf_sndReadPeaks(ofd, peaks, NULL)) {
        long i;
        double peaktime;
        printf("PEAK information:\n");

        for (i = 0; i < props.chans; i++) {
            peaktime = (double) peaks[i].pos / props.srate;
            printf("CH %d\t%.4f at %.4f secs\n", (int) (i + 1), peaks[i].val, peaktime);
        }
    }

    exit_sf2float(ifd, ofd, frame, peaks, error);
}

int exit_sf2float(int ifd, int ofd, float *frame, PSF_CHPEAK *peaks, int error) {
    if (ifd >= 0) {
        psf_sndClose(ifd);
    }
    if (ofd >= 0) {
        psf_sndClose(ofd);
    }
    if (frame) {
        free(frame);
    }
    if (peaks) {
        free(peaks);
    }
    psf_finish();
    exit(error);
}