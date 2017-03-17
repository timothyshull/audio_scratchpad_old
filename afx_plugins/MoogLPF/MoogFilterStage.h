#pragma once

#include "pluginconstants.h"

class CMoogFilterStage :
        public CBiQuad {
public:
    CMoogFilterStage(void);

    ~CMoogFilterStage(void);

protected:
    // controls
    float rho; // cutoff
    float scalar;   // scalar value
    float sampleRate;   // scalar value

public:
    inline void initialize(float newSampleRate) {
        // save
        sampleRate = newSampleRate;
        flushDelays();
    }

    void calculateCoeffs(float fc);

    float doFilterStage(float xn);

    float getSampleRate() { return sampleRate; }
};
