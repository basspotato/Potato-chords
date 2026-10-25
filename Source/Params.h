#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PreviewSynth.h"
#include "Sequencer.h"

// Every host parameter: ID, range and default. IDs are permanent once released.
namespace pc::params
{
    constexpr double strumMaxSeconds = 0.04;
    constexpr double swingMaxStep = 1.0 / 3.0;    // at full swing every odd 1/16 lands a third of a step late (triplet feel)
    constexpr double humaniseMaxSeconds = 0.015;  // latest a humanised note can land
    constexpr int humaniseMaxVelocity = 20;       // most a humanised velocity can move either way

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
    inline const juce::ParameterID melodySound { "melodySound", 1 };
    inline const juce::ParameterID chorus    { "chorus", 1 };
    inline const juce::ParameterID reverb    { "reverb", 1 };
    inline const juce::ParameterID strum     { "strum", 1 };
    inline const juce::ParameterID octave    { "chordOctave", 1 };
    inline const juce::ParameterID velocity  { "velocity", 1 };
    inline const juce::ParameterID swing     { "swing", 1 };
    inline const juce::ParameterID melodyOctave { "melodyOctave", 1 };
    inline const juce::ParameterID humanise  { "humanise", 1 };

    inline const juce::StringArray keyNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    inline const juce::StringArray scaleNames { "Major", "Minor" };
    // Order matches pc::theory::Flavour, pc::voicing::Spread, pc::synth::presets and pc::sequencer::patterns
    inline const juce::StringArray flavourNames { "Triad", "7th", "sus2", "sus4", "add9", "Power" };
    inline const juce::StringArray spreadNames { "Close", "Wide" };
    inline const juce::StringArray melodyOctaveNames { "-1", "0", "+1" };  // index 1 = no shift
    inline const juce::StringArray octaveNames { "-2", "-1", "0", "+1", "+2" };  // index 2 = no shift
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
        layout.add (std::make_unique<juce::AudioParameterChoice> (melodySound, "Melody sound", soundNames, 23));
        // Pushes the odd 1/16 steps of the lane and melody late, 0-1
        layout.add (std::make_unique<juce::AudioParameterFloat> (swing, "Swing", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterChoice> (melodyOctave, "Melody octave", melodyOctaveNames, 1));
        // Velocity of pad, lane and melody notes (chord keys keep the velocity played)
        layout.add (std::make_unique<juce::AudioParameterInt> (velocity, "Velocity", 1, 127, 100));
        // Random velocity and late timing on every generated note, 0-1
        layout.add (std::make_unique<juce::AudioParameterFloat> (humanise, "Humanise", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterChoice> (octave, "Chord octave", octaveNames, 2));
        // Chord notes roll in from low to high, 0-1 = up to strumMaxSeconds between notes (MIDI out too)
        layout.add (std::make_unique<juce::AudioParameterFloat> (strum, "Strum", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
        // Effects on the preview sound only, 0-1
        layout.add (std::make_unique<juce::AudioParameterFloat> (chorus, "Chorus", juce::NormalisableRange<float> (0.0f, 1.0f), 0.15f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (reverb, "Reverb", juce::NormalisableRange<float> (0.0f, 1.0f), 0.25f));
        return layout;
    }
}
