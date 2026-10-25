#pragma once
#include "PluginProcessor.h"
#include "gui/ChordPad.h"
#include "gui/Controls.h"
#include "gui/Display.h"
#include "gui/Keyboard.h"
#include "gui/PotatoLogo.h"
#include "gui/ProgressionLane.h"
#include <optional>

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
    void padPressed (int degree);
    void loadPreset (int index);
    int hintSource() const;
    juce::String statusText() const;

    PotatoChordsProcessor& proc;
    pc::gui::look::LookAndFeel look;
    juce::Image background;

    juce::ComboBox key, sound, presets;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment keyAttachment, soundAttachment;
    pc::gui::ChoiceStrip scale, flavour, spread;
    pc::gui::Switch chordKeys, preview, majorFive, playLane;
    pc::gui::ActionButton clearLane { "Clear" };
    pc::gui::DragChip dragLane { "Drag lane" }, dragTake { "Drag take" };
    pc::gui::LedToggle capture { "Capture", pc::gui::colours::record };
    std::array<std::unique_ptr<pc::gui::ChordPad>, pc::theory::numDegrees> pads;
    pc::gui::PotatoLogo logo;
    pc::gui::Display display;
    pc::gui::ProgressionLane lane;
    pc::gui::Keyboard keyboard;

    pc::midiexport::Recorder take;
    int selectedSlot = -1, lastDegree = -1;
    float logoGlow = 0.0f;
    bool minorKey = false;
    juce::String status;
    juce::Rectangle<int> statusArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PotatoChordsEditor)
};
