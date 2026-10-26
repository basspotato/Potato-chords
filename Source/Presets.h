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

    // Up to three chords that commonly follow `degree`, strongest first (degrees 0-6, -1 = no more).
    // Plain common-practice and pop tendencies: the V wants home, IV lifts to V, vi falls to IV, and so on.
    using Ranked = std::array<int8_t, 3>;
    constexpr Ranked nextChordRanked (theory::Scale s, int degree)
    {
        constexpr Ranked major[7] {
            { 3, 4, 5 },    // I   -> IV, V, vi
            { 4, 6, -1 },   // ii  -> V, vii
            { 5, 3, -1 },   // iii -> vi, IV
            { 4, 0, 1 },    // IV  -> V, I, ii
            { 0, 5, -1 },   // V   -> I, vi
            { 3, 1, 4 },    // vi  -> IV, ii, V
            { 0, 2, -1 },   // vii -> I, iii
        };
        constexpr Ranked minor[7] {
            { 3, 5, 6 },    // i   -> iv, VI, VII
            { 4, 0, -1 },   // ii  -> v/V, i
            { 5, 3, -1 },   // III -> VI, iv
            { 4, 0, 6 },    // iv  -> v/V, i, VII
            { 0, 5, -1 },   // v   -> i, VI
            { 2, 6, 3 },    // VI  -> III, VII, iv
            { 2, 0, -1 },   // VII -> III, i
        };
        if (degree < 0 || degree > 6) return { -1, -1, -1 };
        return s == theory::Scale::major ? major[degree] : minor[degree];
    }

    // The same chords as a bit mask of degrees (bit d = degree d)
    constexpr uint8_t nextChordHints (theory::Scale s, int degree)
    {
        uint8_t mask = 0;
        for (const int d : nextChordRanked (s, degree))
            if (d >= 0) mask = (uint8_t) (mask | (1 << d));
        return mask;
    }

    // 1 = the strongest next chord, 2 and 3 the runners-up, 0 = not suggested
    constexpr int nextChordRank (theory::Scale s, int degree, int candidate)
    {
        const auto list = nextChordRanked (s, degree);
        for (int i = 0; i < 3; ++i)
            if (list[(size_t) i] == candidate) return i + 1;
        return 0;
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
