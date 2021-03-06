#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main()
{
    double c5, c0, semitone_ratio, frequency;
    int midinote;
    char message[256];
    char* result;
    int len;

    semitone_ratio = pow(2, 1.0/12);
    c5 = 220.0 * pow(semitone_ratio, 3);
    c0 = c5 * pow(0.5, 5);

    printf("Enter MIDI note (0 - 127): ");

    fgets(message, sizeof(message), stdin);

    if (message[0] != NULL && message[len - 1] == '\n') {
        message[len - 1] = '\0';
    } else {
        printf("Error reading input.\n");
        return 1;
    }

    if (result == NULL) {
        printf("Error reading the input.\n");
        return 1;
    }

    if (message[0] == '0') {
        printf("Have a nice day!\n");
        return 1;
    }

    midinote = atoi(message);
    if (midinote < 0) {
        printf("Sorry = %s is a bad MIDI note number\n", message);
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
