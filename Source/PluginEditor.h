#pragma once
#include "PluginProcessor.h"
#include "gui/ChordPad.h"
#include "gui/Controls.h"
#include "gui/Display.h"
#include "gui/Keyboard.h"
#include "gui/PotatoLogo.h"
#include "gui/ProgressionLane.h"
#include "gui/MelodyLane.h"
#include "gui/SongStrip.h"
#include "gui/WriterPanel.h"
#include "Writer.h"
#include "MelodyWriter.h"
#include "Ideas.h"
#include <optional>

class PotatoChordsEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PotatoChordsEditor (PotatoChordsProcessor&);
    ~PotatoChordsEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    float zoom() const;  // window width / layout width
    void setSongView (bool song) { viewTabs.setSelected (song ? 1 : 0); showView (song); }  // for the snapshot tool
    void showWriter (bool melody) { auto& w = melody ? melodyWriter : writer; w.setVisible (true); w.toFront (false); }                                   // for the snapshot tool

private:
    void timerCallback() override;
    void refresh();
    void padPressed (int degree);
    void loadPreset (int index);
    void loadIdea (int index);
    static constexpr int ideaIdBase = 101;  // Starters menu ids: 1-8 progressions, 101+ song ideas
    void exportLane();
    juce::File laneFile();
    int hintSource() const;
    juce::String statusText() const;
    pc::midiexport::Feel feel (bool forMelody) const;
    juce::File songFile (int first, int last, const juce::String& name);
    void applySongChoice();
    void setSongBpm (int bpm);  // 0 = sync to the DAW's tempo
    void showView (bool song);

    PotatoChordsProcessor& proc;
    pc::gui::look::LookAndFeel look;
    juce::Image background;

    juce::ComboBox key, sound, presets, rhythm, melodySound;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment keyAttachment, soundAttachment, rhythmAttachment, melodySoundAttachment;
    pc::gui::ChoiceStrip scale, flavour, spread, octave, melodyOctave;
    pc::gui::Switch chordKeys, preview, majorFive, playLane, learn, playSong;
    pc::gui::Fader chorusFader, reverbFader, strumFader, velocityFader, humaniseFader, swingFader;
    pc::gui::ActionButton clearLane { "Clear" }, clearMelody { "Clear" }, repeatMelody { "Repeat 1-4" }, exportMidi { "Export .mid" }, panic { "All notes off" },
                          undo { "", -1 }, redo { "", 1 }, writeLane { "Write" }, writeMelody { "Write" };
    std::unique_ptr<juce::FileChooser> chooser;
    pc::gui::DragChip dragLane { "Drag lane" }, dragTake { "Drag take" }, dragMelody { "Drag melody" };
    pc::gui::LedToggle capture { "Capture", pc::gui::colours::record };
    std::array<std::unique_ptr<pc::gui::ChordPad>, pc::theory::numDegrees> pads;
    pc::gui::PotatoLogo logo;
    pc::gui::Display display;
    pc::gui::ProgressionLane lane;
    pc::gui::MelodyLane melodyLane;
    pc::gui::Keyboard keyboard;
    pc::gui::TabStrip viewTabs { { "Melody", "Song" } };
    juce::ComboBox songTemplate, songLength, songBpm;
    pc::gui::DragChip dragSong { "Drag song" };
    pc::gui::SongStrip songStrip;
    pc::gui::LedToggle songSync { "Sync", pc::gui::colours::accent };
    pc::gui::WriterPanel writer { "WRITE A PROGRESSION", "Style", pc::gui::namesOf (pc::writer::styleNames),
                                  "Mood", pc::gui::namesOf (pc::writer::moodNames), "Complete empty bars" },
                         melodyWriter { "WRITE A MELODY", "Shape", pc::gui::namesOf (pc::melodywriter::shapeNames),
                                        "Busy", pc::gui::namesOf (pc::melodywriter::busyNames) };
    bool songView = false;

    juce::TooltipWindow tooltips { this, 600 };
    pc::midiexport::Recorder take;
    int selectedSlot = -1, lastDegree = -1;
    float logoGlow = 0.0f;
    bool minorKey = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PotatoChordsEditor)
};
