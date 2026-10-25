#pragma once
#include "Progression.h"

// Factory progressions and next-chord hints. Presets fill the lane; they are never host programs.
namespace pc::presets
{
    struct ProgressionPreset
    {
        const char* name;     // numerals, the same in every key
        const char* nickname;
        theory::Scale scale;
        std::array<int, 4> degrees;
        uint8_t majorFiveMask;  // bit i = chord i is the major V (minor presets only)
    };

    inline constexpr std::array<ProgressionPreset, 8> progressions {{
        { "I-V-vi-IV",    "Pop anthem",      theory::Scale::major, { 0, 4, 5, 3 }, 0 },
        { "vi-IV-I-V",    "Emotional pop",   theory::Scale::major, { 5, 3, 0, 4 }, 0 },
        { "I-vi-IV-V",    "Classic",         theory::Scale::major, { 0, 5, 3, 4 }, 0 },
        { "IV-V-vi-I",    "Uplifting build", theory::Scale::major, { 3, 4, 5, 0 }, 0 },
        { "i-VI-III-VII", "Epic EDM",        theory::Scale::minor, { 0, 5, 2, 6 }, 0 },
        { "i-VI-iv-VII",  "Dark drop",       theory::Scale::minor, { 0, 5, 3, 6 }, 0 },
        { "i-VII-VI-VII", "Festival",        theory::Scale::minor, { 0, 6, 5, 6 }, 0 },
        { "i-iv-VI-V",    "Dramatic",        theory::Scale::minor, { 0, 3, 5, 4 }, 0b1000 },
    }};

    // Four chords, one per bar, played twice across the 8 bars
    inline progression::Snapshot fill (const ProgressionPreset& p, theory::Flavour flavour)
    {
        progression::Snapshot s;
        for (int bar = 0; bar < progression::numBars; ++bar)
        {
            const int i = bar % 4;
            progression::Slot slot;
            slot.degree = p.degrees[(size_t) i];
            slot.flavour = flavour;
            slot.majorFive = ((p.majorFiveMask >> i) & 1) != 0;
            s.slots[(size_t) bar * 2] = slot.encode();
        }
        return s;
    }

    // Chords that commonly follow `degree`, strongest first, as a bit mask of degrees (bit d = degree d).
    // Plain common-practice and pop tendencies: the V wants home, IV lifts to V, vi falls to IV, and so on.
    constexpr uint8_t nextChordHints (theory::Scale s, int degree)
    {
        constexpr uint8_t majorHints[7] {
            (1 << 3) | (1 << 4) | (1 << 5),  // I   -> IV, V, vi
            (1 << 4) | (1 << 6),             // ii  -> V, vii
            (1 << 5) | (1 << 3),             // iii -> vi, IV
            (1 << 4) | (1 << 0) | (1 << 1),  // IV  -> V, I, ii
            (1 << 0) | (1 << 5),             // V   -> I, vi
            (1 << 3) | (1 << 1) | (1 << 4),  // vi  -> IV, ii, V
            (1 << 0) | (1 << 2),             // vii -> I, iii
        };
        constexpr uint8_t minorHints[7] {
            (1 << 3) | (1 << 5) | (1 << 6),  // i   -> iv, VI, VII
            (1 << 4) | (1 << 0),             // ii  -> v/V, i
            (1 << 5) | (1 << 3),             // III -> VI, iv
            (1 << 4) | (1 << 0) | (1 << 6),  // iv  -> v/V, i, VII
            (1 << 0) | (1 << 5),             // v   -> i, VI
            (1 << 2) | (1 << 6) | (1 << 3),  // VI  -> III, VII, iv
            (1 << 2) | (1 << 0),             // VII -> III, i
        };
        if (degree < 0 || degree > 6) return 0;
        return s == theory::Scale::major ? majorHints[degree] : minorHints[degree];
    }

    // One word for how each chord feels in its key, for the teaching display
    constexpr const char* moodWord (theory::Scale s, int degree, bool majorFive)
    {
        constexpr const char* major[7] { "home", "soft", "dreamy", "lift", "pull", "sad", "tense" };
        constexpr const char* minor[7] { "home", "tense", "bright", "dark", "drift", "epic", "rise" };
        if (degree < 0 || degree > 6) return "";
        if (s == theory::Scale::minor && degree == 4 && majorFive) return "pull";
        return s == theory::Scale::major ? major[degree] : minor[degree];
    }
}
