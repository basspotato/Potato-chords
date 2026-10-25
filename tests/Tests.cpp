// Headless tests for chord theory and the plugin processor.
// Build with -DPOTATO_DEV_TOOLS=ON, run with ctest or ./build/PotatoChordsTests.
#include "PluginProcessor.h"
#include "Theory.h"
#include <iostream>

namespace
{
int failures = 0;

#define CHECK(cond, what)                                                         \
    do {                                                                          \
        if (! (cond)) { ++failures; std::cerr << "FAIL: " << what << "\n"; }      \
        else          { std::cout << "ok:   " << what << "\n"; }                   \
    } while (false)

constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;

using pc::theory::Scale;
using Triad = std::array<int, 3>;

void theoryTests()
{
    CHECK ((pc::theory::triad (60, Scale::major, 0) == Triad { 60, 64, 67 }), "C major I = C E G");
    CHECK ((pc::theory::triad (60, Scale::major, 4) == Triad { 67, 71, 74 }), "C major V = G B D");
    CHECK ((pc::theory::triad (60, Scale::major, 6) == Triad { 71, 74, 77 }), "C major vii° = B D F");
    CHECK ((pc::theory::triad (57, Scale::minor, 0) == Triad { 57, 60, 64 }), "A minor i = A C E");
    CHECK ((pc::theory::triad (57, Scale::minor, 5) == Triad { 65, 69, 72 }), "A minor VI = F A C");
    CHECK (pc::theory::stepToSemitone (Scale::major, -1) == -1, "step below the root wraps down an octave");
    CHECK (pc::theory::stepToSemitone (Scale::minor, 14) == 24, "two octaves up");
}

void processorTests()
{
    std::unique_ptr<PotatoChordsProcessor> proc { dynamic_cast<PotatoChordsProcessor*> (createPluginFilter()) };
    proc->setPlayConfigDetails (0, 2, sampleRate, blockSize);
    proc->prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);
    buffer.applyGain (0.0f);
    buffer.setSample (0, 10, 1.0f);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 17);
    proc->processBlock (buffer, midi);

    CHECK (buffer.getMagnitude (0, blockSize) <= 0.0f, "audio output is silent");
    CHECK (midi.getNumEvents() == 1, "incoming MIDI passes through");
    for (const auto meta : midi)
        CHECK (meta.samplePosition == 17 && meta.getMessage().getNoteNumber() == 60, "MIDI keeps its sample offset");

    // State round trip into a fresh instance
    proc->apvts.getParameter (pc::params::key.getParamID())->setValueNotifyingHost (
        proc->apvts.getParameter (pc::params::key.getParamID())->convertTo0to1 (7.0f));
    juce::MemoryBlock state;
    proc->getStateInformation (state);
    std::unique_ptr<PotatoChordsProcessor> other { dynamic_cast<PotatoChordsProcessor*> (createPluginFilter()) };
    other->setStateInformation (state.getData(), (int) state.getSize());
    CHECK ((int) other->apvts.getRawParameterValue (pc::params::key.getParamID())->load() == 7, "state restores the key");
    CHECK ((int) other->apvts.state.getProperty ("version") == PotatoChordsProcessor::stateVersion, "state carries its version");
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    theoryTests();
    processorTests();
    std::cout << (failures == 0 ? "All tests passed\n" : "Tests failed\n");
    return failures == 0 ? 0 : 1;
}
