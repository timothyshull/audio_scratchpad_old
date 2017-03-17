//
// Created by Timothy Shull on 1/23/16.
//

#include <stdio.h>
#include <math.h>

int main(void) {
    double semitone_ratio = pow(2, 1 / 12.0);
    double c5 = 220.0 * pow(semitone_ratio, 3);
    double c0 = c5 * pow(0.5, 5);
    int midinote = 73;
    double frequency = c0 * pow(semitone_ratio, midinote);
    printf("MIDI note %d has frequency %f\n", midinote, frequency);
    return 0;
}