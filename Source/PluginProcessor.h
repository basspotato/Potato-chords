#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Theory.h"
#include "Voicing.h"
#include "MidiOut.h"
#include "PreviewSynth.h"
#include "PreviewFx.h"
#include "Progression.h"
#include "Sequencer.h"
#include "Melody.h"
#include "MidiExport.h"

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

    // All notes off at the start of the next block (any thread)
    void panic() { flushRequested = true; }

    // What is sounding now, for the GUI: the held degree (-1 = none) and the chord's MIDI notes
    int heldDegree() const { return activeDegree.load (std::memory_order_relaxed); }
    pc::voicing::Voiced soundingNotes() const;
    // Every note going out right now (chord and pass-through), for the keyboard
    bool isNoteSounding (int note) const
    {
        return note >= 0 && note < 128 && ((outputNotes[(size_t) note / 64].load (std::memory_order_relaxed) >> (note % 64)) & 1) != 0;
    }

    // The progression lane's slot sounding now (-1 = none) and whether the host transport is running
    int playingSlot() const { return laneSlot.load (std::memory_order_relaxed); }
    bool hostIsPlaying() const { return hostPlaying.load (std::memory_order_relaxed); }

    pc::theory::Chord chordFor (int degree) const;
    pc::theory::Chord chordFor (const pc::progression::Slot&) const;

    // Capture: while on, every note going out is handed to the message thread, timed in quarter notes
    void setCapturing (bool on) { capturing.store (on); }
    bool isCapturing() const { return capturing.load(); }
    template <typename Fn> void drainCapture (Fn&& fn)  // message thread
    {
        const auto scope = captureFifo.read (captureFifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i) fn (captureEvents[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i) fn (captureEvents[(size_t) (scope.startIndex2 + i)]);
    }
    double hostBpm() const { return lastBpm.load (std::memory_order_relaxed); }
    double barQuarters() const { return lastBarQuarters.load (std::memory_order_relaxed); }
    bool keyUsesFlats() const;

    // White keys in this MIDI range (60-71, shown as C3-B3 in Cubase) play chords when Chord keys is on
    static constexpr int chordKeysLow = 60;
    static constexpr int chordChannel = 1;
    static constexpr int stateVersion = 1;

    juce::AudioProcessorValueTreeState apvts;
    pc::progression::Lane lane;  // edited by the GUI, played in time with the host
    pc::melody::Lane melody;     // the same, for the melody (channel 2)
    int playingMelodyStep() const { return melodyStepShown.load (std::memory_order_relaxed); }
    static constexpr int melodyChannel = 2;
    int editorWidth = 0;  // last window width, so the editor reopens at the same size (message thread only)

    // Undo for lane and melody edits (message thread only). Call rememberEdit() just before changing either lane.
    void rememberEdit()
    {
        push (undoStack, currentLanes());
        redoStack.clear();
    }
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    void undo() { step (undoStack, redoStack); }
    void redo() { step (redoStack, undoStack); }  // last window width, so the editor reopens at the same size (message thread only)

private:
    struct Lanes
    {
        pc::progression::Snapshot lane;
        pc::melody::Snapshot tune;
    };
    static constexpr size_t maxUndo = 100;
    Lanes currentLanes() const { return { lane.snapshot(), melody.snapshot() }; }
    static void push (std::vector<Lanes>& stack, Lanes l)
    {
        if (stack.size() >= maxUndo) stack.erase (stack.begin());
        stack.push_back (std::move (l));
    }
    void step (std::vector<Lanes>& from, std::vector<Lanes>& to)
    {
        if (from.empty()) return;
        push (to, currentLanes());
        lane.set (from.back().lane);
        melody.set (from.back().tune);
        from.pop_back();
    }
    std::vector<Lanes> undoStack, redoStack;

    enum class Source { none, pad, key };

    struct Settings
    {
        int key = 0, scale = 0, flavour = 0, spread = 0, octave = 0;
        bool majorFive = false;
        bool operator!= (const Settings& o) const
        {
            return key != o.key || scale != o.scale || flavour != o.flavour || spread != o.spread || octave != o.octave
                || majorFive != o.majorFive;
        }
    };

    Settings readSettings() const;
    void pushPadEvent (int event);
    void handleMidi (const juce::MidiBuffer& in, bool chordKeysOn, bool padsOn);
    void playChord (int degree, juce::uint8 velocity, int samplePos);
    void stopChord (int samplePos);
    void flushAll (int samplePos);
    void runLane (int numSamples);
    void enterLaneSlot (int slotIndex, const pc::progression::Snapshot&);
    void startLaneHit (const pc::sequencer::Hit&, int samplePos);
    void stopLaneChord (int samplePos);
    void startNotes (int owner, const pc::voicing::Voiced&, juce::uint8 velocity, int samplePos, int channel = chordChannel);
    void stopNotes (int owner, const pc::voicing::Voiced&, int samplePos, int channel = chordChannel);
    juce::uint8 noteVelocity() const { return (juce::uint8) juce::jlimit (1, 127, (int) velocityParam->load()); }
    void melodyStep (const pc::melody::Snapshot&, int step, bool restart, int samplePos);
    void stopMelodyNote (int samplePos);
    void renderPreview (juce::AudioBuffer<float>&);
    void captureOutput (int numSamples);
    void publishState();

    std::atomic<float>* keyParam = nullptr;
    std::atomic<float>* scaleParam = nullptr;
    std::atomic<float>* flavourParam = nullptr;
    std::atomic<float>* spreadParam = nullptr;
    std::atomic<float>* majorFiveParam = nullptr;
    std::atomic<float>* chordKeysParam = nullptr;
    std::atomic<float>* soundParam = nullptr;
    std::atomic<float>* previewParam = nullptr;
    std::atomic<float>* playLaneParam = nullptr;
    std::atomic<float>* rhythmParam = nullptr;
    std::atomic<float>* melodySoundParam = nullptr;
    std::atomic<float>* chorusParam = nullptr;
    std::atomic<float>* reverbParam = nullptr;
    std::atomic<float>* strumParam = nullptr;
    std::atomic<float>* octaveParam = nullptr;
    std::atomic<float>* velocityParam = nullptr;
    std::atomic<float>* humaniseParam = nullptr;
    std::atomic<float>* swingParam = nullptr;
    int swingDelay = 0;  // samples the next lane or melody note start is pushed late (set per 1/16 step in runLane)
    juce::Random random { 0x5eed };

    juce::AbstractFifo padFifo { 64 };
    std::array<int, 64> padEvents {};

    pc::midiout::NoteTracker tracker;
    pc::midiout::Strummer strummer;
    int64_t blockStart = 0;  // samples processed so far: the strum's clock
    enum Owner { liveOwner = 0, laneOwner = 1, melodyOwner = 2 };
    pc::synth::PreviewSynth synth;
    pc::fx::PreviewFx fx;
    juce::MidiBuffer out;

    Source source = Source::none;
    int currentDegree = -1, triggerChannel = 0, triggerNote = -1;
    juce::uint8 chordVelocity = 100;
    Settings chordSettings;
    pc::voicing::Voiced previousClose, playing;
    std::array<std::array<juce::uint8, 128>, 16> passHeld {};  // note-ons passed through and not yet released
    bool previewWasOn = true;

    // Progression lane playback
    double sampleRate = 48000.0, expectedPpq = 0.0;
    bool laneRunning = false;
    int laneCurrent = -1;
    uint8_t laneCode = 0;
    Settings laneSettings;
    pc::voicing::Voiced lanePreviousClose, laneVoicing, lanePlaying;  // voicing of the slot, and the notes sounding now
    int melodyNote = -1;
    bool blockPlaying = false;
    double blockPpq = 0.0, blockBpm = 120.0, freeQuarters = 0.0;

    juce::AbstractFifo captureFifo { 8192 };
    std::array<pc::midiexport::CaptureEvent, 8192> captureEvents {};

    std::atomic<bool> flushRequested { false };
    std::atomic<int> activeDegree { -1 };
    std::atomic<uint64_t> activeNotes { ~uint64_t (0) };
    std::array<std::atomic<uint64_t>, 2> outputNotes {};
    std::atomic<int> laneSlot { -1 }, melodyStepShown { -1 };
    std::atomic<bool> hostPlaying { false }, capturing { false };
    std::atomic<double> lastBpm { 120.0 }, lastBarQuarters { 4.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PotatoChordsProcessor)
};
