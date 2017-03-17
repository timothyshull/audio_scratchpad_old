#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"


class MainContentComponent   : public AudioAppComponent,
                               public Slider::Listener
{
public:
    MainContentComponent()
    {
        targetLevel = 0.125;
        
        levelSlider.setRange (0.0, 0.25);
        levelSlider.setValue (targetLevel, dontSendNotification);
        levelSlider.setTextBoxStyle (Slider::TextBoxRight, false, 100, 20);
        levelSlider.addListener (this);
        
        levelLabel.setText ("Noise Level", dontSendNotification);
        
        addAndMakeVisible (&levelSlider);
        addAndMakeVisible (&levelLabel);
        
        setSize (800, 100);
        
        setAudioChannels (0, 2);
    }

    ~MainContentComponent()
    {
        shutdownAudio();
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        resetParameters();
    }

    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override
    {
        int numSamplesRemaining = bufferToFill.numSamples;
        int offset = 0;
        
        if (samplesToTarget > 0)
        {
            const float levelIncrement = (targetLevel - currentLevel) / samplesToTarget;
            const int numSamplesThisTime = jmin (numSamplesRemaining, samplesToTarget);
            
            for (int sample = 0; sample < numSamplesThisTime; ++sample)
            {
                for (int channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
                    bufferToFill.buffer->setSample (channel, sample, random.nextFloat() * currentLevel);
                
                currentLevel += levelIncrement;
                --samplesToTarget;
            }
            
            offset = numSamplesThisTime;
            numSamplesRemaining -= numSamplesThisTime;
            
            if (samplesToTarget == 0)
                currentLevel = targetLevel;
        }
        
        if (numSamplesRemaining > 0)
        {
            for (int channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
            {
                float* buffer = bufferToFill.buffer->getWritePointer (channel, bufferToFill.startSample + offset);
                
                for (int sample = 0; sample < numSamplesRemaining; ++sample)
                    *buffer++ = random.nextFloat() * currentLevel;
            }
        }
    }

    void releaseResources() override
    {
    }

    void resized() override
    {
        levelLabel.setBounds (10, 10, 90, 20);
        levelSlider.setBounds (100, 10, getWidth() - 110, 20);
    }
    
    void sliderValueChanged (Slider* slider) override
    {
        if (&levelSlider == slider)
        {
            targetLevel = levelSlider.getValue();
            samplesToTarget = rampLengthSamples;
        }
    }
    
    void resetParameters()
    {
        currentLevel = targetLevel;
        samplesToTarget = 0;
    }
    

private:
    Random random;
    Slider levelSlider;
    Label levelLabel;
    float currentLevel;
    float targetLevel;
    int samplesToTarget;
    static const int rampLengthSamples;
    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};

const int MainContentComponent::rampLengthSamples = 128;

Component* createMainContentComponent()     { return new MainContentComponent(); }

#endif  // MAINCOMPONENT_H_INCLUDED
