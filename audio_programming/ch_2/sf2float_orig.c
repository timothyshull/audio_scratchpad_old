#include <stdio.h>
#include <stdlib.h>
#include <portsf.h>

enum {ARG_PROGNAME, ARG_INFILE, ARG_OUTFILE, ARG_NARGS};

const int FRAME_BUF_SZ = 64;

int handle_exit(int, int, float*, PSF_CHPEAK*, int);

int main(int argc, char* argv[]) {
    PSF_PROPS props;
    long framesread, totalread;

    int ifd = -1, ofd = -1;
    int error = 0;
    psf_format outformat = PSF_FMT_UNKNOWN;
    PSF_CHPEAK* peaks = NULL;
    float* frame = NULL;

    printf("sf2float: convert soundfile to floats format\n");

    if (argc < ARG_NARGS) {
        printf("Insufficient arguments.\nUsage:\n\tsf2float infile outfile\n");
        return 1;
    }

    if (psf_init()) {
        printf("Unable to start portsf\n");
        return 1;
    }

    ifd = psf_sndOpen(argv[ARG_INFILE], &props, 0);
    if (ifd < 0) {
        printf("Error: unable to open infile %s\n", argv[ARG_INFILE]);
        return 1;
    }

    if (props.samptype == PSF_SAMP_IEEE_FLOAT) {
        printf("Info: input file is already in floats format.\n");
    }

    props.samptype = PSF_SAMP_IEEE_FLOAT;
    outformat = psf_getFormatExt(argv[ARG_OUTFILE]);
    if (outformat == PSF_FMT_UNKNOWN) {
        printf("outfile name %s has unknown format.\nAcceptable file types include: .wav, .aiff, .aif, .afc, .aifc\n", argv[ARG_OUTFILE]);
        error++;
        return handle_exit(ifd, ofd, frame, peaks, error);
    }

    props.format = outformat;

    ofd = psf_sndCreate(argv[2], &props, 0, 0, PSF_CREATE_RDWR);
    if (ofd < 0) {
        printf("Error: unable to create outfile %s\n", argv[ARG_OUTFILE]);
        error++;
        return handle_exit(ifd, ofd, frame, peaks, error);
    }

    frame = (float *) malloc(props.chans * sizeof(float) * FRAME_BUF_SZ);

    if (frame == NULL) {
        puts("No memory!\n");
        error++;
        return handle_exit(ifd, ofd, frame, peaks, error);
    }

    peaks = (PSF_CHPEAK*) malloc(props.chans * sizeof(PSF_CHPEAK));
    if (peaks == NULL) {
        puts("No memory!\n");
        error++;
        return handle_exit(ifd, ofd, frame, peaks, error);
    }

    printf("copying...\n");

    framesread = psf_sndReadFloatFrames(ifd, frame, FRAME_BUF_SZ);
    totalread = 0;

    while (framesread == FRAME_BUF_SZ) {
        totalread += FRAME_BUF_SZ;

        if (psf_sndWriteFloatFrames(ofd, frame, 1) != 1) {
            printf("Error writing to outfile\n");
            error++;
            break;
        }
        /* do any processing here */
        framesread = psf_sndReadFloatFrames(ifd, frame, FRAME_BUF_SZ);
    }

    if (framesread < 0) {
        printf("Error reading infile. Outfile is incomplete.\n");
        error++;
    } else {
        printf("Done. %d sample frames copied to %s\n", (int)totalread, argv[ARG_OUTFILE]);
    }

    if (psf_sndReadPeaks(ofd, peaks, NULL)) {
        long i;
        double peaktime;
        printf("PEAK information:\n");

        for (i = 0; i < props.chans; i++) {
            peaktime = (double) peaks[i].pos / props.srate;
            printf("CH %d\t%.4f at %.4f secs\n", (int)(i + 1), peaks[i].val, peaktime);
        }
    }

    return handle_exit(ifd, ofd, frame, peaks, error);
}

int handle_exit(int in_ifd, int in_ofd, float* in_frame, PSF_CHPEAK* in_peak, int in_error) {
    if (in_ifd >= 0)
        psf_sndClose(in_ifd);
    if (in_ofd >= 0)
        psf_sndClose(in_ofd);
    if (in_frame)
        free(in_frame);
    if (in_peak)
        free(in_peak);
    psf_finish();
    return in_error;
}