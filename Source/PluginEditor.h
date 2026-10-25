#pragma once
#include "PluginProcessor.h"
#include "gui/ChordPad.h"
#include "gui/Controls.h"

class PotatoChordsEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PotatoChordsEditor (PotatoChordsProcessor&);
    ~PotatoChordsEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refresh();

    PotatoChordsProcessor& proc;
    juce::LookAndFeel_V4 look;

    juce::ComboBox key;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment keyAttachment;
    pc::gui::ChoiceStrip scale, flavour, spread, sound;
    pc::gui::Switch chordKeys, preview, majorFive;
    std::array<std::unique_ptr<pc::gui::ChordPad>, pc::theory::numDegrees> pads;

    juce::String nowPlaying;
    pc::voicing::Voiced shownNotes;
    bool minorKey = false, chordKeysOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PotatoChordsEditor)
};
