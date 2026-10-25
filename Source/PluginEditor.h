#pragma once
#include "PluginProcessor.h"

class PotatoChordsEditor : public juce::AudioProcessorEditor
{
public:
    explicit PotatoChordsEditor (PotatoChordsProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::ComboBox key, scale;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment keyAttachment, scaleAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PotatoChordsEditor)
};
