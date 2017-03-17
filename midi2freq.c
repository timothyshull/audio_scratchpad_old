#include<stdio.h>
#include <math.h>

int main()
{
    double semitone_ratio;
    double c0;
    double c5;
    double frequency;
    int midinote;

    semitone_ratio = pow(2, 1/12.0);
    c5 = 220.0 * pow(semitone_ratio, 3);
    c0 = c5 * pow(0.5, 5);

    midinote = 73;
    frequency = c0 * pow(semitone_ratio, midinote);

    printf("MIDI Note %d has frequency %f\n", midinote, frequency);

    return 0;
}
