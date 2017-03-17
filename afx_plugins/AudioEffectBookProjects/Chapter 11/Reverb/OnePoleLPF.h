/*
	COnePoleLPF: implements a one pole feedback LPF
				 the user controls the cutofff with
				 the coefficient g



	Copyright (c) 2010 Will Pirkle
	Free for academic use.
*/
#pragma once

class COnePoleLPF {
public:
    // constructor/Destructor
    COnePoleLPF(void);

    ~COnePoleLPF(void);

    // members
protected:
    float m_fLPF_g; // one gain coefficient
    float m_fLPF_z1; // one delay

public:

    // set our one and only gain coefficient
    void setLPF_g(float fLPFg) { m_fLPF_g = fLPFg; }

    // function to init
    void init();

    // function to process audio
    bool processAudio(float *pInput, float *pOutput);

};
