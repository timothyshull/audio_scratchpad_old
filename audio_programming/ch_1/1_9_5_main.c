#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI (3.141592654)
#endif

enum {
    ARG_NAME, ARG_OUTFILE, ARG_DUR, ARG_HZ, ARG_SR, ARG_AMP, ARG_TYPE, ARG_NARGS
};

enum samptype {
    RAWSAMP_SHORT, RAWSAMP_FLOAT
};

int byte_order();

const char* endianness[2] = {"big_endian", "little_endian"};

int main(int argc, char* argv[]) {
    unsigned int i, nsamps;
    unsigned int maxframe = 0;
    unsigned int samptype, endian, bitreverse;
    double samp, dur, freq, srate, amp, step;
    double start, end, fac, maxsamp;
    double twopi = 2.0 * M_PI;
    double angleincr;
    FILE* fp = NULL;
    float fsamp;
    short ssamp;

    if (argc != ARG_NARGS) {
        printf("Usage: tforkraw outsfile.raw dur freq srate amp isfloat\n");
        return 1;
    }

    dur = atof(argv[ARG_DUR]);
    freq = atof(argv[ARG_HZ]);
    srate = atof(argv[ARG_SR]);
    amp = atof(argv[ARG_AMP]);
    samptype = (unsigned int) atoi(argv[ARG_TYPE]);

    if (samptype > 1) {
        printf("error: sampletype can be only 0 or 1\n");
        return 1;
    }

    fp = fopen(argv[ARG_OUTFILE], "wb");
    if (fp == NULL) {
        fprintf(stderr, "Error creating output file %s\n", argv[ARG_OUTFILE]);
        return 1;
    }

    nsamps = (unsigned int) (dur * srate);
    angleincr = twopi * freq / srate;
    step = dur / nsamps;
    start = 1.0;
    end = 1.0e-4;
    maxsamp = 0.0;
    fac = pow(end / start, 1.0 / nsamps);
    endian = (unsigned int) byte_order();

    printf("Writing %d %s samples\n", nsamps, endianness[endian]);

    if (samptype == RAWSAMP_SHORT) {
        for (i = 0; i < nsamps; i++) {
            samp = amp * sin(angleincr * i);
            samp *= start;
            start *= fac;

            ssamp = (short) (samp * 32767.0);
            if (fwrite(&ssamp, sizeof(short), 1, fp) != 1) {
                printf("Error writing data to file\n");
                return 1;
            }
            if (fabs(samp) > maxsamp) {
                maxsamp = fabs(samp);
                maxframe = i;
            }
        }
    } else {
        for (i = 0; i < nsamps; i++) {
            samp = amp * sin(angleincr * i);
            samp *= start;
            start *= fac;
            fsamp = (float) samp;

            if (fwrite(&fsamp, sizeof(float), 1, fp) != 1) {
                printf("Error writing data to file\n");
                return 1;
            }
            if (fabs(samp) > maxsamp) {
                maxsamp = fabs(samp);
                maxframe = i;
            }
        }
    }

    fclose(fp);
    printf("Done. Maximum sample value = %.8lf at frame %d\n", maxsamp, maxframe);
    return 0;
}

int byte_order() {
    int one = 1;
    char* endptr = (char*) &one;
    return *endptr;
}