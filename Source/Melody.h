#pragma once
#include "Theory.h"
#include "Progression.h"
#include <atomic>
#include <cstdint>
#include <string>

// The melody lane: 8 bars of 16 sixteenth-steps, one note at a time, using only notes of the key.
// A note is a start cell plus "hold" cells after it. Stored in atomics so the GUI can edit while it plays.
namespace pc::melody
{
    constexpr int stepsPerBar = 16;
    constexpr int numSteps = progression::numBars * stepsPerBar;
    constexpr int numRows = 14;  // two octaves of the scale, row 0 = the key's root

    // One cell: 0 = empty, else bit 7 set, bits 0-3 = row, bit 4 = hold (continues the note before it)
    struct Cell
    {
        int row = -1;
        bool hold = false;

        constexpr bool empty() const { return row < 0; }
        constexpr uint8_t encode() const { return empty() ? 0 : (uint8_t) (0x80 | (row & 15) | (hold ? 0x10 : 0)); }
        static constexpr Cell decode (uint8_t b)
        {
            Cell c;
            if ((b & 0x80) == 0 || (b & 15) >= numRows) return c;
            c.row = b & 15;
            c.hold = (b & 0x10) != 0;
            return c;
        }
    };

    // MIDI note for a row: the key's root around C5, then up the scale
    constexpr int rowToNote (int row, int key, theory::Scale s)
    {
        const int base = 72 + key - (key > 5 ? 12 : 0);
        return base + theory::stepToSemitone (s, row);
    }

    struct Snapshot
    {
        std::array<uint8_t, numSteps> cells {};

        Cell cell (int step) const { return Cell::decode (cells[(size_t) step]); }

        // The note covering `step`: its row and the step it starts on, or row -1
        std::pair<int, int> noteAt (int step) const
        {
            for (int s = step; s >= 0; --s)
            {
                const auto c = cell (s);
                if (c.empty()) return { -1, -1 };
                if (! c.hold) return { c.row, s };
            }
            return { -1, -1 };
        }

        // How many steps the note starting at `start` lasts
        int lengthFrom (int start) const
        {
            int n = 1;
            while (start + n < numSteps && cell (start + n).hold) ++n;
            return n;
        }

        std::string toString() const
        {
            static const char* hex = "0123456789abcdef";
            std::string s;
            for (auto b : cells) { s += hex[b >> 4]; s += hex[b & 15]; }
            return s;
        }

        static Snapshot fromString (const std::string& s)
        {
            auto value = [] (char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
            Snapshot snap;
            for (int i = 0; i < numSteps && (size_t) i * 2 + 1 < s.size(); ++i)
            {
                const int hi = value (s[(size_t) i * 2]), lo = value (s[(size_t) i * 2 + 1]);
                if (hi < 0 || lo < 0) break;
                snap.cells[(size_t) i] = Cell::decode ((uint8_t) (hi * 16 + lo)).encode();
            }
            // A hold with nothing to continue becomes empty
            for (int i = 0; i < numSteps; ++i)
                if (snap.cell (i).hold && (i == 0 || snap.cell (i - 1).empty() || snap.cell (i - 1).row != snap.cell (i).row))
                    snap.cells[(size_t) i] = 0;
            return snap;
        }
    };

    class Lane
    {
    public:
        Snapshot snapshot() const
        {
            Snapshot s;
            for (int i = 0; i < numSteps; ++i) s.cells[(size_t) i] = cells[(size_t) i].load (std::memory_order_relaxed);
            return s;
        }

        void set (const Snapshot& s)
        {
            for (int i = 0; i < numSteps; ++i) cells[(size_t) i].store (s.cells[(size_t) i], std::memory_order_relaxed);
        }

        void clear() { set ({}); }

        // Editing helpers for the GUI
        void addNote (int start, int length, int row)
        {
            auto s = snapshot();
            removeNoteAt (s, start);
            for (int i = 0; i < length && start + i < numSteps; ++i)
            {
                removeNoteAt (s, start + i);
                s.cells[(size_t) (start + i)] = Cell { row, i > 0 }.encode();
            }
            // A note that used to continue past the end loses its orphaned holds
            for (int i = start + length; i < numSteps && s.cell (i).hold; ++i) s.cells[(size_t) i] = 0;
            set (s);
        }

        // Copies bars 1-4 over bars 5-8
        void repeatFirstHalf()
        {
            auto s = snapshot();
            for (int i = 0; i < numSteps / 2; ++i) s.cells[(size_t) (numSteps / 2 + i)] = s.cells[(size_t) i];
            set (s);
        }

        void removeNote (int step)
        {
            auto s = snapshot();
            removeNoteAt (s, step);
            set (s);
        }

    private:
        static void removeNoteAt (Snapshot& s, int step)
        {
            const auto [row, start] = s.noteAt (step);
            if (row < 0) return;
            const int length = s.lengthFrom (start);
            for (int i = 0; i < length; ++i) s.cells[(size_t) (start + i)] = 0;
        }

        std::array<std::atomic<uint8_t>, numSteps> cells {};
    };
}
