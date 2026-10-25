#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PreviewSynth.h"
#include "Sequencer.h"

// Every host parameter: ID, range and default. IDs are permanent once released.
namespace pc::params
{
    inline const juce::ParameterID key       { "key", 1 };
    inline const juce::ParameterID scale     { "scale", 1 };
    inline const juce::ParameterID flavour   { "flavour", 1 };
    inline const juce::ParameterID spread    { "spread", 1 };
    inline const juce::ParameterID majorFive { "majorFive", 1 };
    inline const juce::ParameterID chordKeys { "chordKeys", 1 };
    inline const juce::ParameterID sound     { "chordSound", 1 };
    inline const juce::ParameterID preview   { "preview", 1 };
    inline const juce::ParameterID playLane  { "playLane", 1 };
    inline const juce::ParameterID rhythm    { "laneRhythm", 1 };
    inline const juce::ParameterID learn     { "learn", 1 };

    inline const juce::StringArray keyNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    inline const juce::StringArray scaleNames { "Major", "Minor" };
    // Order matches pc::theory::Flavour, pc::voicing::Spread, pc::synth::presets and pc::sequencer::patterns
    inline const juce::StringArray flavourNames { "Triad", "7th", "sus2", "sus4", "add9", "Power" };
    inline const juce::StringArray spreadNames { "Close", "Wide" };
    inline const juce::StringArray soundNames = []
    {
        juce::StringArray names;
        for (auto& p : pc::synth::presets) names.add (p.name);
        return names;
    }();

    inline const juce::StringArray rhythmNames = []
    {
        juce::StringArray names;
        for (auto& p : pc::sequencer::patterns) names.add (p.name);
        return names;
    }();

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (std::make_unique<juce::AudioParameterChoice> (key, "Key", keyNames, 0));
        layout.add (std::make_unique<juce::AudioParameterChoice> (scale, "Scale", scaleNames, 1));
        layout.add (std::make_unique<juce::AudioParameterChoice> (flavour, "Flavour", flavourNames, 0));
        layout.add (std::make_unique<juce::AudioParameterChoice> (spread, "Spread", spreadNames, 0));
        layout.add (std::make_unique<juce::AudioParameterBool> (majorFive, "Major V in minor", false));
        layout.add (std::make_unique<juce::AudioParameterBool> (chordKeys, "Chord keys", false));
        layout.add (std::make_unique<juce::AudioParameterChoice> (sound, "Chord sound", soundNames, 0));
        layout.add (std::make_unique<juce::AudioParameterBool> (preview, "Preview sound", true));
        layout.add (std::make_unique<juce::AudioParameterBool> (playLane, "Play progression", true));
        layout.add (std::make_unique<juce::AudioParameterChoice> (rhythm, "Rhythm", rhythmNames, 0));
        layout.add (std::make_unique<juce::AudioParameterBool> (learn, "Learn mode", true));
        return layout;
    }
}
