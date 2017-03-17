#include <stdio.h>
#include <stdlib.h>
#include <portsf.h>
#include <unistd.h>

enum {
    ARG_INFILE = 1,
    ARG_OUTFILE = 2,
    ARG_NARGS = 3
};


void clear_scr() {
//    if (!cur_term) {
//        int result = 0;
//        setupterm(NULL, STDOUT_FILENO, &result);
//        if (result <= 0) return;
//    }
//
//    putp(tigetstr(clear_screen));
    printf("%c[2J", 27);
    printf("%c[1;1H", 27);
}

//printw

//32 64 128 256 512 1024 2048 4096

int exit_sf2float(int ifd, int ofd, float *frame, PSF_CHPEAK *peaks, int error);

int main(int argc, char *argv[]) {
    PSF_PROPS props;
    long framesread;
    long totalread;

    int ofd = 1;
    int error = 0;
    PSF_CHPEAK *peaks = NULL;
    float *frame = NULL;
    int buffer_size = 32;

    clear_scr();

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
        printf("Total frames read: %lu\r", totalread);
        fflush(stdout);
        usleep(100);
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