#pragma once
#include "Theory.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>

// The progression lane: 8 bars, each holding one chord or two (split into halves).
// Slots are packed into atomics so the GUI can edit while the audio thread plays, without locks.
namespace pc::progression
{
    constexpr int numBars = 8;
    constexpr int numSlots = numBars * 2;  // slot 2*b is bar b (or its first half), 2*b+1 its second half

    // One chord in a slot, as a byte: 0 = empty
    struct Slot
    {
        int degree = -1;  // 0-6, -1 = empty
        theory::Flavour flavour = theory::Flavour::triad;
        bool majorFive = false;

        constexpr bool empty() const { return degree < 0; }

        constexpr uint8_t encode() const
        {
            if (empty()) return 0;
            return (uint8_t) (0x80 | (degree & 7) | (((int) flavour & 7) << 3) | (majorFive ? 0x40 : 0));
        }

        static constexpr Slot decode (uint8_t b)
        {
            Slot s;
            if ((b & 0x80) == 0 || (b & 7) > 6 || ((b >> 3) & 7) >= theory::numFlavours) return s;
            s.degree = b & 7;
            s.flavour = (theory::Flavour) ((b >> 3) & 7);
            s.majorFive = (b & 0x40) != 0;
            return s;
        }

        constexpr bool operator== (const Slot& o) const { return encode() == o.encode(); }
        constexpr bool operator!= (const Slot& o) const { return ! (*this == o); }
    };

    // A plain copy of the lane, for the audio thread and for tests
    struct Snapshot
    {
        std::array<uint8_t, numSlots> slots {};
        uint8_t splits = 0;  // bit b set = bar b holds two chords

        bool isSplit (int bar) const { return ((splits >> bar) & 1) != 0; }
        Slot slot (int index) const { return Slot::decode (slots[(size_t) index]); }

        // Length of the lane in quarter notes, for a bar of `barQuarters`
        static double lengthQuarters (double barQuarters) { return barQuarters * numBars; }

        // Which slot plays at `pos` quarters into the lane (0 <= pos < length)
        int slotAt (double pos, double barQuarters) const
        {
            const int bar = std::min (numBars - 1, std::max (0, (int) (pos / barQuarters)));
            const bool second = isSplit (bar) && pos - bar * barQuarters >= barQuarters * 0.5 - 1.0e-9;
            return bar * 2 + (second ? 1 : 0);
        }

        // Start of the next slot strictly after `pos` (may equal the lane length, i.e. the loop point)
        double nextBoundary (double pos, double barQuarters) const
        {
            const int bar = std::min (numBars - 1, std::max (0, (int) (pos / barQuarters)));
            const double barStart = bar * barQuarters;
            if (isSplit (bar) && pos < barStart + barQuarters * 0.5 - 1.0e-9)
                return barStart + barQuarters * 0.5;
            return barStart + barQuarters;
        }

        // The chord that sounds in `index`: the slot itself, or the whole bar's chord for an unsplit bar's second half
        Slot chordIn (int index) const
        {
            const int bar = index / 2;
            return (index % 2 == 1 && ! isSplit (bar)) ? slot (bar * 2) : slot (index);
        }

        // Text form for saved state: 16 two-digit hex bytes then the split mask, e.g. "80 81 ... | 05"
        std::string toString() const
        {
            static const char* hex = "0123456789abcdef";
            std::string s;
            for (auto b : slots) { s += hex[b >> 4]; s += hex[b & 15]; s += ' '; }
            s += "| ";
            s += hex[splits >> 4];
            s += hex[splits & 15];
            return s;
        }

        static Snapshot fromString (const std::string& s)
        {
            auto value = [] (char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
            Snapshot snap;
            int index = 0;
            size_t i = 0;
            for (; i + 1 < s.size() && index < numSlots; ++i)
            {
                if (s[i] == '|') break;
                const int hi = value (s[i]), lo = value (s[i + 1]);
                if (hi >= 0 && lo >= 0)
                {
                    snap.slots[(size_t) index++] = (uint8_t) (hi * 16 + lo);
                    ++i;
                }
            }
            if (const auto bar = s.find ('|'); bar != std::string::npos)
                for (size_t j = bar + 1; j + 1 < s.size(); ++j)
                    if (value (s[j]) >= 0 && value (s[j + 1]) >= 0)
                    {
                        snap.splits = (uint8_t) (value (s[j]) * 16 + value (s[j + 1]));
                        break;
                    }
            for (int k = 0; k < numSlots; ++k)  // drop anything that doesn't decode
                snap.slots[(size_t) k] = Slot::decode (snap.slots[(size_t) k]).encode();
            return snap;
        }
    };

    // The shared lane. Any thread may read; the GUI (or state load) writes.
    class Lane
    {
    public:
        Snapshot snapshot() const
        {
            Snapshot s;
            for (int i = 0; i < numSlots; ++i)
                s.slots[(size_t) i] = slots[(size_t) i].load (std::memory_order_relaxed);
            s.splits = splits.load (std::memory_order_relaxed);
            return s;
        }

        void set (const Snapshot& s)
        {
            for (int i = 0; i < numSlots; ++i)
                slots[(size_t) i].store (s.slots[(size_t) i], std::memory_order_relaxed);
            splits.store (s.splits, std::memory_order_relaxed);
        }

        void setSlot (int index, Slot s) { slots[(size_t) index].store (s.encode(), std::memory_order_relaxed); }
        Slot slot (int index) const      { return Slot::decode (slots[(size_t) index].load (std::memory_order_relaxed)); }

        void setSplit (int bar, bool split)
        {
            auto m = splits.load (std::memory_order_relaxed);
            m = (uint8_t) (split ? (m | (1 << bar)) : (m & ~(1 << bar)));
            splits.store (m, std::memory_order_relaxed);
        }
        bool isSplit (int bar) const { return ((splits.load (std::memory_order_relaxed) >> bar) & 1) != 0; }

        void clear() { set ({}); }

    private:
        std::array<std::atomic<uint8_t>, numSlots> slots {};
        std::atomic<uint8_t> splits { 0 };
    };
}
