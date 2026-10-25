#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Every host parameter: ID, range and default. IDs are permanent once released.
namespace pc::params
{
    inline const juce::ParameterID key   { "key", 1 };
    inline const juce::ParameterID scale { "scale", 1 };

    inline const juce::StringArray keyNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    inline const juce::StringArray scaleNames { "Major", "Minor" };

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (std::make_unique<juce::AudioParameterChoice> (key, "Key", keyNames, 0));
        layout.add (std::make_unique<juce::AudioParameterChoice> (scale, "Scale", scaleNames, 1));
        return layout;
    }
}
