#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char* argv[]) {
    int notes, i;
    int ismidi = 0;
    int write_interval = 0;
    int err = 0;
    double start_val, base_freq, ratio;
    FILE* fp;
    double intervals[25];

    while (argc > 1) {
        if (argv[1][0] == '-') {
            if (argv[1][1] == 'm') {
                ismidi = 1;
            } else if (argv[1][1] == 'i') {
                write_interval = 1;
            } else {
                printf("error: unrecognized option %s\n", argv[1]);
                return 1;
            }
            argc--;
            argv++;
        } else {
            break;
        }
    }

    if (argc < 3) {
        printf("insufficient arguments\n");
        printf("Usage: iscale [-m][-i] N start_val [outfile.txt]\n");
        return 1;
    }

    notes = atoi(argv[1]);

    if (notes < 1 || notes > 24) {
        printf("error: N out of range. Must be between 1 and 24.\n");
        return 1;
    }

    start_val = atof(argv[2]);

    if (ismidi) {
        if (start_val > 127.0) {
            printf("error: MIDI start_val must be <= 127.\n");
            return 1;
        }
        if (start_val < 0.0) {
            printf("error: MIDI start_val must be >= 0.\n");
            return 1;
        }
    } else {
        if (start_val < 0.0) {
            printf("error: MIDI start_val must be >= 0.\n");
            return 1;
        }
    }

    fp = NULL;
    if (argc == 4) {
        fp = fopen(argv[3], "w");
        if (fp == NULL) {
            printf("WARNING: unable to create file %s\n", argv[3]);
            perror("");
        }
    }

    if (ismidi) {
        double c0, c5;
        ratio = pow(2.0, 1.0 / 12.0);
        c5 = 220.0 * pow(ratio, 3.0);
        c0 = c5 * pow(0.5, 5.0);
        base_freq = c0 * pow(ratio, start_val);
    } else {
        base_freq = start_val;
    }

    ratio = pow(2.0, 1.0 / notes);

    for (i = 0; i <= notes; i++) {
        intervals[i] = base_freq;
        base_freq *= ratio;
    }

    for (i = 0; i <= notes; i++) {
        if (write_interval) {
            printf("%d:\t%f\t%f\n", i, pow(ratio, i), intervals[i]);
        } else {
            printf("%d:\t%f\n", i, intervals[i]);
        }
        if (fp) {
            if (write_interval)
                err = fprintf(fp, "%d:\t%f\t%f\n", i, pow(ratio, i), intervals[i]);
            else
                err = fprintf(fp, "%d:\t%f\n", i, intervals[i]);
            if (err < 0)
                break;
        }
    }

    if (err < 0)
        perror("An error occurred while writing to the file.\n");
    if (fp)
        fclose(fp);
    return 0;
}
