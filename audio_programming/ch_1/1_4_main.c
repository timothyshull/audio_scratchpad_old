#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char* argv[]) {
    double c5, c0, semitone_ratio;
    double frequency;
    int midi_note;

    semitone_ratio = pow(2.0, 1.0 / 12.0);
    c5 = 220.0 * pow(semitone_ratio, 3.0);

    if (argc != 2) {
        printf("cpsmidi: converts MIDI note to frequency.\n");
        printf("usage: cpsmidi MIDI note\n");
        printf("range: 0 <= MIDI note <= 127\n");
        return 1;
    }

    midi_note = atoi(argv[1]);

    if (midi_note < 0) {
        printf("Bad MIDI note value: %s\n", argv[1]);
        return 1;
    }

    if (midi_note > 127) {
        printf("%s is beyond the MIDI range!\n", argv[1]);
        return 1;
    }

    frequency = c0 * pow(semitone_ratio, midi_note);
    printf("Frequency of MIDI note %d = %f\n", midi_note, frequency);
    return 0;
}
