#pragma once
#include "Melody.h"
#include "Writer.h"
#include <algorithm>
#include <cstdlib>

// Write a melody over the lane's chords: rhythm first, then notes that fit each chord, then the phrase repeated
// with one tweak. Works in scale rows (row 0 = the key's root), so every note is in the key.
// Bars 1-4 (verse) stay low; bars 5-8 (chorus) peak a little higher. Each half is two 2-bar phrases: the first
// ends away from home (a question), the second repeats it with one change and lands home (the answer).
// Pure and allocation-free; the same seed always gives the same melody.
namespace pc::melodywriter
{
    enum class Shape { singAlong, riff, arp, whoop };
    enum class Busy { sparse, medium, busy };
    constexpr int numShapes = 4, numBusy = 3;
    inline constexpr std::array<const char*, numShapes> shapeNames { "Sing-along", "Riff", "Arp", "Whoop" };
    inline constexpr std::array<const char*, numBusy> busyNames { "Sparse", "Medium", "Busy" };

    constexpr int phraseSteps = 2 * melody::stepsPerBar;  // a 2-bar phrase
    constexpr int maxNotes = 32;

    struct Hit { int8_t step, length; };
    struct Rhythm
    {
        int count;
        std::array<Hit, maxNotes> hits;
    };

    // 2-bar rhythm motifs. Sing-along: speech-like eighths with rests. Riff: syncopated sixteenths. Arp: even
    // notes. Whoop: straight eighths.
    namespace detail
    {
        inline constexpr Rhythm singAlong[numBusy] {
            { 6,  {{ { 0, 4 }, { 6, 2 }, { 8, 6 }, { 16, 4 }, { 22, 2 }, { 24, 6 } }} },
            { 9,  {{ { 0, 2 }, { 2, 2 }, { 4, 2 }, { 8, 4 }, { 14, 2 }, { 16, 2 }, { 18, 2 }, { 20, 2 }, { 24, 6 } }} },
            { 13, {{ { 0, 2 }, { 2, 2 }, { 4, 2 }, { 6, 2 }, { 8, 2 }, { 10, 2 }, { 12, 3 }, { 16, 2 }, { 18, 2 }, { 20, 2 },
                     { 22, 2 }, { 24, 2 }, { 26, 4 } }} },
        };
        inline constexpr Rhythm riff[numBusy] {
            { 8,  {{ { 0, 2 }, { 3, 2 }, { 6, 2 }, { 10, 2 }, { 16, 2 }, { 19, 2 }, { 22, 2 }, { 26, 4 } }} },
            { 12, {{ { 0, 2 }, { 3, 1 }, { 6, 2 }, { 8, 1 }, { 11, 2 }, { 14, 2 }, { 16, 2 }, { 19, 1 }, { 22, 2 }, { 24, 1 },
                     { 27, 2 }, { 30, 2 } }} },
            { 19, {{ { 0, 1 }, { 2, 1 }, { 3, 1 }, { 5, 1 }, { 6, 2 }, { 8, 1 }, { 10, 1 }, { 11, 1 }, { 13, 1 }, { 14, 2 },
                     { 16, 1 }, { 18, 1 }, { 19, 1 }, { 21, 1 }, { 22, 2 }, { 24, 1 }, { 26, 1 }, { 27, 1 }, { 30, 2 } }} },
        };
        constexpr Rhythm even (int every, int length, int skipFrom = 99)
        {
            Rhythm r { 0, {} };
            for (int s = 0; s < phraseSteps && r.count < maxNotes; s += every)
                if (s < skipFrom) r.hits[(size_t) r.count++] = { (int8_t) s, (int8_t) length };
            return r;
        }
        inline constexpr Rhythm arp[numBusy] { even (4, 3), even (2, 2), even (1, 1) };
        inline constexpr Rhythm whoop[numBusy] { even (4, 3), even (2, 2, 28), even (2, 2) };
    }

    constexpr const Rhythm& rhythmFor (Shape s, Busy b)
    {
        switch (s)
        {
            case Shape::riff:  return detail::riff[(int) b];
            case Shape::arp:   return detail::arp[(int) b];
            case Shape::whoop: return detail::whoop[(int) b];
            case Shape::singAlong: break;
        }
        return detail::singAlong[(int) b];
    }

    // The chord sounding at a melody step (an empty bar counts as the home chord)
    inline int chordAt (const progression::Snapshot& lane, int step)
    {
        const int bar = step / melody::stepsPerBar;
        const int slot = bar * 2 + (lane.isSplit (bar) && step % melody::stepsPerBar >= melody::stepsPerBar / 2 ? 1 : 0);
        const int degree = lane.chordIn (slot).degree;
        return degree < 0 ? 0 : degree;
    }

    constexpr bool isChordTone (int row, int degree)
    {
        const int d = ((row % 7) - degree + 7) % 7;
        return d == 0 || d == 2 || d == 4;
    }

    // Major pentatonic 1 2 3 5 6, minor pentatonic 1 b3 4 5 b7
    constexpr bool isPentatonic (theory::Scale s, int row)
    {
        const int d = row % 7;
        return s == theory::Scale::major ? (d == 0 || d == 1 || d == 2 || d == 4 || d == 5)
                                         : (d == 0 || d == 2 || d == 3 || d == 4 || d == 6);
    }

    constexpr bool strongBeat (int step) { return step % (melody::stepsPerBar / 2) == 0; }  // beats 1 and 3

    // The rows each half may use: about a fifth for the verse, peaking 2 rows (3-5 semitones) higher in the chorus
    struct Range { int lo, hi; };
    constexpr Range rangeFor (int half) { return half == 0 ? Range { 0, 4 } : Range { 2, 6 }; }

    struct Phrase
    {
        int count = 0;
        std::array<Hit, maxNotes> hits {};
        std::array<int, maxNotes> rows {};
    };

    // One 2-bar phrase starting at `start`, shaped rise-then-fall, with at most one leap followed by a step back
    inline Phrase makePhrase (const progression::Snapshot& lane, theory::Scale scale, Shape shape, const Rhythm& rhythm,
                              int start, Range range, writer::Dice& dice)
    {
        Phrase p;
        p.count = rhythm.count;
        p.hits = rhythm.hits;
        const int leapAt = rhythm.count > 3 ? 1 + dice.below (std::max (1, rhythm.count * 3 / 5 - 1)) : -1;
        int prev = range.lo + dice.below (2);
        bool leapt = false, stepBack = false;
        for (int i = 0; i < p.count; ++i)
        {
            const int step = start + p.hits[(size_t) i].step;
            const int degree = chordAt (lane, step);
            int row = prev;
            if (shape == Shape::whoop)
            {
                row = i % 2 == 0 ? 4 : 2;  // the key's 5th and 3rd, back and forth
            }
            else if (shape == Shape::arp)
            {
                // Walk the chord up from its lowest tone in the range, then start again
                int tones[8] {}, n = 0;
                for (int r = range.lo; r <= range.hi + 2 && n < 8; ++r)
                    if (isChordTone (r, degree)) tones[n++] = r;
                row = n > 0 ? tones[i % std::min (n, 4)] : range.lo;
            }
            else
            {
                const float t = (float) i / (float) std::max (1, p.count - 1);
                const float target = (float) range.lo + (float) (range.hi - range.lo) * (t < 0.6f ? t / 0.6f : 1.0f - (t - 0.6f) / 0.4f * 0.7f);
                const bool strong = strongBeat (step) || i == 0;
                int best = prev, bestScore = -100000;
                // Pass 0 keeps every rule; pass 1, only if nothing fit, keeps just the chord tone and the range
                for (int pass = 0; pass < 2 && bestScore == -100000; ++pass)
                for (int r = range.lo; r <= range.hi; ++r)
                {
                    const int move = r - prev;
                    const bool strict = pass == 0;
                    if (strict && stepBack && move != -1) continue;               // after the leap, one step back down
                    const bool leapNow = i == leapAt && ! leapt && move >= 3 && move <= 5;
                    if (strict && ! strong && std::abs (move) > 1) continue;      // between strong beats: step or repeat
                    if (strict && strong && std::abs (move) > 2 && ! leapNow) continue;  // only the one leap jumps further
                    if (strong && ! isChordTone (r, degree)) continue;            // beats 1 and 3 sit on the chord
                    int score = -(int) (std::abs ((float) r - target) * 20.0f) - std::abs (move) * 6 + dice.below (12);
                    if (strong && (r % 7 == 0 || r % 7 == 4)) score += 10;      // the key's 1 and 5 weigh most
                    if (! strong && isPentatonic (scale, r)) score += 8;
                    if (leapNow) score += 40;
                    if (move == 0 && i > 0 && p.rows[(size_t) i - 1] == prev && i > 1 && p.rows[(size_t) i - 2] == prev) score -= 30;
                    if (score > bestScore) { bestScore = score; best = r; }
                }
                row = best;
                stepBack = false;
                if (row - prev >= 3) { leapt = true; stepBack = true; }
            }
            p.rows[(size_t) i] = row;
            prev = row;
        }
        return p;
    }

    // The nearest row in range that is a chord tone of `degree` and passes `ok`
    template <typename Ok>
    int nearestChordTone (int from, int degree, Range range, Ok ok)
    {
        for (int dist = 0; dist <= 13; ++dist)
            for (int sign : { -1, 1 })
            {
                const int r = from + sign * dist;
                if (r >= range.lo && r <= range.hi && isChordTone (r, degree) && ok (r)) return r;
            }
        return from;
    }

    inline void putPhrase (melody::Snapshot& out, const Phrase& p, int start)
    {
        for (int i = 0; i < p.count; ++i)
        {
            const int s = start + p.hits[(size_t) i].step;
            int length = p.hits[(size_t) i].length;
            if (i + 1 < p.count) length = std::min (length, (int) p.hits[(size_t) i + 1].step - p.hits[(size_t) i].step);
            length = std::min (length, start + phraseSteps - s);
            for (int k = 0; k < length; ++k)
                out.cells[(size_t) (s + k)] = melody::Cell { p.rows[(size_t) i], k > 0 }.encode();
        }
    }

    // Home, or the chord's third (just past the range if need be), whichever is nearer the note before.
    // If that would be a second leap in the phrase, the nearest chord tone instead.
    inline bool restsHome (int row, int degree) { return (row % 7 == 0 && isChordTone (row, degree)) || (row - degree + 7) % 7 == 2; }

    inline int answerEnding (const Phrase& p, int degree, Range range)
    {
        const int before = p.count > 1 ? p.rows[(size_t) p.count - 2] : p.rows[0];
        bool leapt = false;
        for (int i = 1; i < p.count - 1; ++i) leapt = leapt || p.rows[(size_t) i] - p.rows[(size_t) i - 1] >= 3;
        int best = -1;
        for (int r = range.lo; r <= range.hi + 2; ++r)
            if (restsHome (r, degree) && (best < 0 || std::abs (r - before) < std::abs (best - before))) best = r;
        if (best >= 0 && (best - before < 3 || ! leapt)) return best;
        return nearestChordTone (before, degree, range, [before] (int r) { return r - before < 3; });
    }

    inline melody::Snapshot write (const progression::Snapshot& lane, theory::Scale scale, Shape shape, Busy busy, uint32_t seed)
    {
        writer::Dice dice (seed ^ 0x6d656cu);
        melody::Snapshot out;
        const auto& rhythm = rhythmFor (shape, busy);
        for (int half = 0; half < 2; ++half)
        {
            const auto range = rangeFor (half);
            const int first = half * 2 * phraseSteps, second = first + phraseSteps;
            auto question = makePhrase (lane, scale, shape, rhythm, first, range, dice);
            auto answer = question;  // the whoop simply repeats

            if (shape == Shape::singAlong || shape == Shape::riff)
            {
                // The question ends away from home
                auto& qLast = question.rows[(size_t) question.count - 1];
                if (qLast % 7 == 0)
                    qLast = nearestChordTone (qLast, chordAt (lane, first + question.hits[(size_t) question.count - 1].step), range,
                                              [] (int r) { return r % 7 != 0; });

                // The answer: the same phrase with one note nudged, refitted to its own chords, landing home
                if (answer.count > 2)
                {
                    const int i = 1 + dice.below (answer.count - 2);
                    if (! strongBeat (answer.hits[(size_t) i].step))
                    {
                        const int nudged = answer.rows[(size_t) i] + (dice.below (2) == 0 ? 1 : -1);
                        const int before = answer.rows[(size_t) i - 1], after = answer.rows[(size_t) i + 1];
                        if (nudged >= range.lo && nudged <= range.hi && std::abs (nudged - before) <= 1 && std::abs (after - nudged) <= 2)
                            answer.rows[(size_t) i] = nudged;
                    }
                }
                for (int i = 0; i < answer.count; ++i)
                {
                    const int step = second + answer.hits[(size_t) i].step;
                    if (strongBeat (step) && ! isChordTone (answer.rows[(size_t) i], chordAt (lane, step)))
                        answer.rows[(size_t) i] = nearestChordTone (answer.rows[(size_t) i], chordAt (lane, step), range, [] (int) { return true; });
                }
                const int lastStep = second + answer.hits[(size_t) answer.count - 1].step;
                const int lastChord = chordAt (lane, lastStep);
                answer.rows[(size_t) answer.count - 1] = answerEnding (answer, lastChord, range);
            }
            else if (shape == Shape::arp)
            {
                answer = makePhrase (lane, scale, shape, rhythm, second, range, dice);
            }
            putPhrase (out, question, first);
            putPhrase (out, answer, second);
        }
        return out;
    }
}
