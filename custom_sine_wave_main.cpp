//
//  main.cpp
//  AudioExperiments
//
//  Created by Tim Shull on 9/26/16.
//  Copyright © 2016 Tim Shull. All rights reserved.
//

#include <iostream>
#include <AudioToolbox/AudioToolbox.h>
#include "CustomSineWavePlayer.h"

int main(int argc, const char *argv[]) {
    CustomSineWavePlayer player{880.0};
    player.init();

    OSStatus err;

    // start playing
    err = AudioOutputUnitStart(player.outputUnit);
    player.check_error(err, "Couldn't start output unit");
    std::cout << "playing" << std::endl;
    sleep(5);

    AudioOutputUnitStop(player.outputUnit);
    AudioUnitUninitialize(player.outputUnit);
    AudioComponentInstanceDispose(player.outputUnit);
    return 0;
}
