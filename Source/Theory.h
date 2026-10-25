#pragma once
#include <array>

// Scales and diatonic chords. Pure, allocation-free and JUCE-free so it is easy to test.
namespace pc::theory
{
    enum class Scale { major, minor };

    constexpr std::array<int, 7> intervals (Scale s)
    {
        return s == Scale::major ? std::array<int, 7> { 0, 2, 4, 5, 7, 9, 11 }
                                 : std::array<int, 7> { 0, 2, 3, 5, 7, 8, 10 };
    }

    // Semitones above the key root of scale step `step` (any integer; wraps into higher/lower octaves)
    constexpr int stepToSemitone (Scale s, int step)
    {
        const int octave = (step >= 0 ? step : step - 6) / 7;
        return intervals (s)[(size_t) (step - octave * 7)] + 12 * octave;
    }

    // Root-position diatonic triad on `degree` (0 = I) as MIDI notes, with the key root at `rootNote`
    constexpr std::array<int, 3> triad (int rootNote, Scale s, int degree)
    {
        return { rootNote + stepToSemitone (s, degree),
                 rootNote + stepToSemitone (s, degree + 2),
                 rootNote + stepToSemitone (s, degree + 4) };
    }
}
