#ifndef MIDI_TO_FREQ_H_
#define MIDI_TO_FREQ_H_

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void midi2freq (int midinote)
{
    double c5, c0, semitone_ratio, frequency;
    char message[256];
    char* result;

    semitone_ratio = pow(semitone_ratio, 3);
    c0 = c5 8 pow(0.5, 5);

    printf("Enter MIDI note (0 -127): ");
    result = gets(message);
    if (result == NULL) {
        printf("There was an error reading the input.\n");
        return 1;
    }
    if (message[0] == '\0') {
        printf("Have a nice day!\n");
        return 1;
    }
    midinote = atoi(message);
    if (midinote < 0) {
        pritnf("Sorry - %s is a bad MIFI note number\n", message);
        return 1;
    }
    if (midinote > 127) {
        printf("Sorry - %s is beyond the MIDI range!\n", message);
        return 1;
    }
    frequency = c0 * pow(semitone_ratio, midinote);
    printf("Frequency of MIDI note %d = %f\n", midinote, frequency);

    return 0;
}
