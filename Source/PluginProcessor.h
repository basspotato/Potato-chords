#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Theory.h"
#include "Voicing.h"
#include "MidiOut.h"
#include "PreviewSynth.h"

class PotatoChordsProcessor : public juce::AudioProcessor
{
public:
    PotatoChordsProcessor();
    ~PotatoChordsProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Potato-Chords"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Presets are deliberately not exposed as host programs: Cubase re-sends the track's program when a
    // track is selected, which would replace the user's progression.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Potato-Chords"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Chord pads (message thread). Events reach the audio thread through a lock-free FIFO.
    void padDown (int degree) { pushPadEvent (degree + 1); }
    void padUp (int degree)   { pushPadEvent (-(degree + 1)); }

    // What is sounding now, for the GUI: the held degree (-1 = none) and the chord's MIDI notes
    int heldDegree() const { return activeDegree.load (std::memory_order_relaxed); }
    pc::voicing::Voiced soundingNotes() const;
    // Every note going out right now (chord and pass-through), for the keyboard
    bool isNoteSounding (int note) const
    {
        return note >= 0 && note < 128 && ((outputNotes[(size_t) note / 64].load (std::memory_order_relaxed) >> (note % 64)) & 1) != 0;
    }

    pc::theory::Chord chordFor (int degree) const;
    bool keyUsesFlats() const;

    // White keys in this MIDI range (60-71, shown as C3-B3 in Cubase) play chords when Chord keys is on
    static constexpr int chordKeysLow = 60;
    static constexpr int chordChannel = 1;
    static constexpr int stateVersion = 1;

    juce::AudioProcessorValueTreeState apvts;

private:
    enum class Source { none, pad, key };

    struct Settings
    {
        int key = 0, scale = 0, flavour = 0, spread = 0;
        bool majorFive = false;
        bool operator!= (const Settings& o) const
        {
            return key != o.key || scale != o.scale || flavour != o.flavour || spread != o.spread || majorFive != o.majorFive;
        }
    };

    Settings readSettings() const;
    void pushPadEvent (int event);
    void handleMidi (const juce::MidiBuffer& in, bool chordKeysOn, bool padsOn);
    void playChord (int degree, juce::uint8 velocity, int samplePos);
    void stopChord (int samplePos);
    void flushAll (int samplePos);
    void renderPreview (juce::AudioBuffer<float>&);
    void publishState();

    std::atomic<float>* keyParam = nullptr;
    std::atomic<float>* scaleParam = nullptr;
    std::atomic<float>* flavourParam = nullptr;
    std::atomic<float>* spreadParam = nullptr;
    std::atomic<float>* majorFiveParam = nullptr;
    std::atomic<float>* chordKeysParam = nullptr;
    std::atomic<float>* soundParam = nullptr;
    std::atomic<float>* previewParam = nullptr;

    juce::AbstractFifo padFifo { 64 };
    std::array<int, 64> padEvents {};

    pc::midiout::NoteTracker tracker;
    pc::synth::PreviewSynth synth;
    juce::MidiBuffer out;

    Source source = Source::none;
    int currentDegree = -1, triggerChannel = 0, triggerNote = -1;
    juce::uint8 chordVelocity = 100;
    Settings chordSettings;
    pc::voicing::Voiced previousClose, playing;
    std::array<std::array<bool, 128>, 16> passHeld {};
    bool previewWasOn = true;

    std::atomic<bool> flushRequested { false };
    std::atomic<int> activeDegree { -1 };
    std::atomic<uint64_t> activeNotes { ~uint64_t (0) };
    std::array<std::atomic<uint64_t>, 2> outputNotes {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PotatoChordsProcessor)
};
