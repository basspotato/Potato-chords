#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <string>

// Song mode: a song is a list of sections (intro, verse, chorus, drop...), each a multiple of 4 bars.
// Templates and lengths come from the Pop/EDM section-length research. Each section loops one half of the
// 8-bar lane: loop A = bars 1-4 (verse-type sections), loop B = bars 5-8 (chorus-type sections).
// Pure and allocation-free; the MIDI side is in MidiExport.h.
namespace pc::song
{
    constexpr int minBpm = 40, maxBpm = 250;  // the Song view's tempo field

    enum class Kind { intro, verse, pre, chorus, bridge, build, drop, breakdown, outro };
    constexpr int numKinds = 9;

    struct KindInfo
    {
        const char* name;
        int loop;        // 0 = lane bars 1-4, 1 = lane bars 5-8
        int rhythm;      // index into pc::sequencer::patterns
        bool melody;     // whether the melody plays in this section
        int minBars, maxBars;
    };

    // Order matches Kind
    inline constexpr std::array<KindInfo, numKinds> kinds {{
        { "Intro",     0, 0, false, 4, 32 },   // Sustain
        { "Verse",     0, 7, true,  8, 16 },   // Half notes
        { "Pre",       0, 3, true,  4, 8 },    // 8th pulse
        { "Chorus",    1, 1, true,  8, 16 },   // Quarters
        { "Bridge",    0, 7, true,  8, 8 },    // Half notes
        { "Build",     1, 6, false, 4, 16 },   // 16th pulse
        { "Drop",      1, 4, true,  8, 32 },   // Syncopated
        { "Breakdown", 0, 0, true,  8, 16 },   // Sustain
        { "Outro",     1, 0, false, 4, 32 },   // Sustain
    }};

    constexpr const KindInfo& info (Kind k) { return kinds[(size_t) k]; }

    struct Section
    {
        Kind kind = Kind::verse;
        int bars = 8;
    };

    constexpr int maxSections = 16;

    struct Song
    {
        std::array<Section, maxSections> sections {};
        int count = 0;

        int totalBars() const
        {
            int n = 0;
            for (int i = 0; i < count; ++i) n += sections[(size_t) i].bars;
            return n;
        }

        bool operator== (const Song& o) const
        {
            if (count != o.count) return false;
            for (int i = 0; i < count; ++i)
                if (sections[(size_t) i].kind != o.sections[(size_t) i].kind || sections[(size_t) i].bars != o.sections[(size_t) i].bars)
                    return false;
            return true;
        }
        bool operator!= (const Song& o) const { return ! (*this == o); }

        int startBar (int index) const
        {
            int n = 0;
            for (int i = 0; i < index && i < count; ++i) n += sections[(size_t) i].bars;
            return n;
        }

        bool add (Kind k, int bars)
        {
            if (count >= maxSections) return false;
            sections[(size_t) count++] = { k, bars };
            return true;
        }

        void insert (int index, Kind k, int bars)
        {
            if (count >= maxSections || index < 0 || index > count) return;
            for (int i = count; i > index; --i) sections[(size_t) i] = sections[(size_t) i - 1];
            sections[(size_t) index] = { k, bars };
            ++count;
        }

        void remove (int index)
        {
            if (index < 0 || index >= count) return;
            for (int i = index; i < count - 1; ++i) sections[(size_t) i] = sections[(size_t) i + 1];
            --count;
        }

        // Text form for the saved state: "kind:bars" pairs by kind index, e.g. "0:8 1:16 3:8"
        std::string toString() const
        {
            std::string s;
            for (int i = 0; i < count; ++i)
            {
                if (i > 0) s += ' ';
                s += std::to_string ((int) sections[(size_t) i].kind) + ":" + std::to_string (sections[(size_t) i].bars);
            }
            return s;
        }

        static Song fromString (const std::string& text)
        {
            Song song;
            size_t pos = 0;
            while (pos < text.size() && song.count < maxSections)
            {
                const auto end = text.find (' ', pos);
                const auto item = text.substr (pos, end == std::string::npos ? std::string::npos : end - pos);
                pos = end == std::string::npos ? text.size() : end + 1;
                const auto colon = item.find (':');
                if (colon == std::string::npos || colon == 0 || colon + 1 >= item.size()) continue;
                int kind = 0, bars = 0;
                bool ok = true;
                for (size_t i = 0; i < colon; ++i) ok = ok && item[i] >= '0' && item[i] <= '9';
                for (size_t i = colon + 1; i < item.size(); ++i) ok = ok && item[i] >= '0' && item[i] <= '9';
                if (! ok || colon > 2 || item.size() - colon > 3) continue;
                kind = std::stoi (item.substr (0, colon));
                bars = std::stoi (item.substr (colon + 1));
                if (kind < 0 || kind >= numKinds || bars < 4 || bars > 64 || bars % 4 != 0) continue;
                song.add ((Kind) kind, bars);
            }
            return song;
        }
    };

    struct Template
    {
        const char* name;
        int introMin, introMax;  // intro and outro range: short for radio songs, long for DJ-friendly club tracks
        int count;
        std::array<Section, maxSections> sections;
    };

    using K = Kind;
    inline constexpr std::array<Template, 3> templates {{
        { "Pop", 4, 8, 10, {{ { K::intro, 8 }, { K::verse, 16 }, { K::pre, 8 }, { K::chorus, 8 }, { K::verse, 16 }, { K::pre, 8 },
                              { K::chorus, 8 }, { K::bridge, 8 }, { K::chorus, 16 }, { K::outro, 8 } }} },
        { "Pop-EDM", 4, 8, 8, {{ { K::intro, 8 }, { K::verse, 16 }, { K::build, 8 }, { K::drop, 16 }, { K::verse, 8 }, { K::build, 8 },
                                 { K::drop, 16 }, { K::outro, 8 } }} },
        { "EDM club", 16, 32, 8, {{ { K::intro, 32 }, { K::breakdown, 16 }, { K::build, 8 }, { K::drop, 32 }, { K::breakdown, 16 },
                                    { K::build, 8 }, { K::drop, 32 }, { K::outro, 32 } }} },
    }};
    constexpr int numTemplates = (int) templates.size();

    inline Song fromTemplate (int index)
    {
        const auto& t = templates[(size_t) (index < 0 ? 0 : index >= numTemplates ? numTemplates - 1 : index)];
        Song s;
        for (int i = 0; i < t.count; ++i) s.add (t.sections[(size_t) i].kind, t.sections[(size_t) i].bars);
        return s;
    }

    // Bars for a length in minutes, rounded to a multiple of 8 (at least 8)
    inline int barsForMinutes (double minutes, double bpm, double beatsPerBar)
    {
        if (minutes <= 0.0 || bpm <= 0.0 || beatsPerBar <= 0.0) return 0;
        const int bars = (int) std::lround (minutes * bpm / beatsPerBar / 8.0) * 8;
        return bars < 8 ? 8 : bars;
    }

    // Stretches or shrinks a template's song towards `targetBars`, in the research's order: intro and outro first,
    // then double or halve choruses and drops, then verses; if that is not enough, whole verse-chorus groups are
    // added or taken out (and the bridge goes first when shrinking). Every section stays a multiple of 4 bars and
    // within its range. The result is as close as the rules allow.
    inline Song fitToBars (int templateIndex, int targetBars)
    {
        const int ti = templateIndex < 0 ? 0 : templateIndex >= numTemplates ? numTemplates - 1 : templateIndex;
        auto song = fromTemplate (ti);
        if (targetBars <= 0) return song;
        const auto& t = templates[(size_t) ti];
        auto diff = [&] { return targetBars - song.totalBars(); };
        auto isEnd = [] (Kind k) { return k == Kind::intro || k == Kind::outro; };
        auto isHook = [] (Kind k) { return k == Kind::chorus || k == Kind::drop; };
        auto isVerse = [] (Kind k) { return k == Kind::verse || k == Kind::breakdown; };
        auto isLift = [] (Kind k) { return k == Kind::pre || k == Kind::build; };
        auto range = [&] (Kind k, int& lo, int& hi)
        {
            lo = isEnd (k) ? t.introMin : info (k).minBars;
            hi = isEnd (k) ? t.introMax : info (k).maxBars;
        };

        auto adjustLengths = [&]
        {
            // Intro and outro, in 4-bar steps within the template's range
            for (bool changed = true; changed && std::abs (diff()) >= 4;)
            {
                changed = false;
                for (int i = 0; i < song.count && std::abs (diff()) >= 4; ++i)
                {
                    auto& s = song.sections[(size_t) i];
                    if (! isEnd (s.kind)) continue;
                    int lo, hi;
                    range (s.kind, lo, hi);
                    const int step = diff() > 0 ? 4 : -4;
                    if (s.bars + step >= lo && s.bars + step <= hi) { s.bars += step; changed = true; }
                }
            }
            // Double or halve hooks, then verses, while that brings the total closer
            for (int pass = 0; pass < 2; ++pass)
                for (int i = 0; i < song.count; ++i)
                {
                    auto& s = song.sections[(size_t) i];
                    if (pass == 0 ? ! isHook (s.kind) : ! isVerse (s.kind)) continue;
                    int lo, hi;
                    range (s.kind, lo, hi);
                    const int d = diff();
                    if (d >= s.bars && s.bars * 2 <= hi) s.bars *= 2;
                    else if (d <= -s.bars / 2 && s.bars / 2 >= lo && s.bars % 8 == 0) s.bars /= 2;
                }
        };

        // A group is a verse (or breakdown), its pre-chorus (or build) and its chorus (or drop): [start, end)
        auto groupAt = [&] (int start)
        {
            int end = start;
            if (end < song.count && isVerse (song.sections[(size_t) end].kind)) ++end; else return start;
            if (end < song.count && isLift (song.sections[(size_t) end].kind)) ++end;
            if (end < song.count && isHook (song.sections[(size_t) end].kind)) ++end;
            return end;
        };
        auto groupBars = [&] (int start, int end)
        {
            int n = 0;
            for (int i = start; i < end; ++i) n += song.sections[(size_t) i].bars;
            return n;
        };

        adjustLengths();

        // Too long: the bridge goes first, then repeated groups from the middle (the first group always stays)
        if (diff() <= -8)
            for (int i = 0; i < song.count; ++i)
                if (song.sections[(size_t) i].kind == Kind::bridge) { song.remove (i); break; }
        for (int guard = 0; guard < 4 && diff() <= -8; ++guard)
        {
            int seen = 0, start = -1, end = -1;
            for (int i = 0; i < song.count; ++i)
                if (const int e = groupAt (i); e > i && seen++ == 1) { start = i; end = e; break; }
            if (start < 0) break;
            const int bars = groupBars (start, end);
            if (std::abs (diff() + bars) >= std::abs (diff())) break;
            for (int i = start; i < end; ++i) song.remove (start);
        }

        // Too short: repeat the first group after the last one, while that brings the total closer
        for (int guard = 0; guard < 4 && diff() >= 8; ++guard)
        {
            int firstStart = -1, firstEnd = -1, lastEnd = -1;
            for (int i = 0; i < song.count; ++i)
                if (const int e = groupAt (i); e > i)
                {
                    if (firstStart < 0) { firstStart = i; firstEnd = e; }
                    lastEnd = e;
                    i = e - 1;
                }
            if (firstStart < 0 || song.count + (firstEnd - firstStart) > maxSections) break;
            const int bars = groupBars (firstStart, firstEnd);
            if (std::abs (diff() - bars) >= std::abs (diff())) break;
            std::array<Section, maxSections> copy {};
            for (int i = firstStart; i < firstEnd; ++i) copy[(size_t) (i - firstStart)] = song.sections[(size_t) i];
            for (int i = 0; i < firstEnd - firstStart; ++i) song.insert (lastEnd + i, copy[(size_t) i].kind, copy[(size_t) i].bars);
        }

        adjustLengths();
        return song;
    }

    // The song as the audio thread reads it: one atomic per section, so the GUI can edit while it plays
    class SongLane
    {
    public:
        Song snapshot() const
        {
            Song s;
            const int n = count.load (std::memory_order_relaxed);
            for (int i = 0; i < n && i < maxSections; ++i)
            {
                const auto v = cells[(size_t) i].load (std::memory_order_relaxed);
                s.sections[(size_t) i] = { (Kind) (v & 15), v >> 4 };
            }
            s.count = n < 0 ? 0 : n > maxSections ? maxSections : n;
            return s;
        }

        void set (const Song& s)
        {
            count.store (0, std::memory_order_relaxed);  // readers see no section half-written as part of the song
            for (int i = 0; i < s.count; ++i)
                cells[(size_t) i].store ((uint16_t) ((int) s.sections[(size_t) i].kind | (s.sections[(size_t) i].bars << 4)),
                                         std::memory_order_relaxed);
            count.store (s.count, std::memory_order_release);
        }

    private:
        std::array<std::atomic<uint16_t>, maxSections> cells {};
        std::atomic<int> count { 0 };
    };
}
