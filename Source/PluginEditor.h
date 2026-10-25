#pragma once
#include "PluginProcessor.h"
#include "gui/ChordPad.h"
#include "gui/Controls.h"
#include "gui/Display.h"
#include "gui/Keyboard.h"
#include "gui/PotatoLogo.h"

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
    pc::gui::look::LookAndFeel look;
    juce::Image background;

    juce::ComboBox key;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment keyAttachment;
    pc::gui::ChoiceStrip scale, flavour, spread, sound;
    pc::gui::Switch chordKeys, preview, majorFive;
    std::array<std::unique_ptr<pc::gui::ChordPad>, pc::theory::numDegrees> pads;
    pc::gui::PotatoLogo logo;
    pc::gui::Display display;
    pc::gui::Keyboard keyboard;

    float logoGlow = 0.0f;
    bool minorKey = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PotatoChordsEditor)
};
