#pragma once
#include "Look.h"
#include "Melody.h"
#include <functional>

namespace pc::gui
{
    // The melody grid: one row per note of the key (two octaves), 16 steps per bar. Click an empty cell and drag
    // right to draw a note; click a note to delete it. Rows that belong to the chord under each bar are tinted,
    // and notes that are in that chord glow in their colour.
    class MelodyLane : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::function<void (int, int, int)> onAdd;  // start step, length, row
        std::function<void (int)> onRemove;         // any step of the note

        struct View
        {
            melody::Snapshot tune;
            std::array<int, melody::numRows> rowPitchClass {};
            std::array<juce::String, melody::numRows> rowName;
            std::array<uint16_t, melody::numSteps> chordMask {};  // pitch classes of the chord under each step
            int stepsInBar = melody::stepsPerBar;
            int playingStep = -1;

            bool operator== (const View& o) const
            {
                return tune.cells == o.tune.cells && rowPitchClass == o.rowPitchClass && rowName == o.rowName
                    && chordMask == o.chordMask && stepsInBar == o.stepsInBar && playingStep == o.playingStep;
            }
        };

        void update (const View& v)
        {
            if (v == view) return;
            view = v;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            look::inset (g, getLocalBounds().toFloat(), 12.0f);
            const auto grid = gridArea();
            const float cw = cellWidth(), rh = rowHeight();

            // Row labels in their note colours
            for (int row = 0; row < melody::numRows; ++row)
            {
                const auto r = juce::Rectangle<float> (6.0f, rowY (row), gutter - 10.0f, rh);
                g.setColour (colours::forNote (view.rowPitchClass[(size_t) row]));
                g.setFont (look::font (juce::jmin (11.0f, rh + 1.0f)));
                g.drawText (view.rowName[(size_t) row], r, juce::Justification::centredRight, false);
            }

            // Chord-tone tint, beat and bar lines
            for (int step = 0; step < melody::numSteps; ++step)
            {
                const float x = grid.getX() + (float) step * cw;
                const bool usable = step % melody::stepsPerBar < view.stepsInBar;
                for (int row = 0; row < melody::numRows; ++row)
                {
                    const int pc = view.rowPitchClass[(size_t) row];
                    if (usable && ((view.chordMask[(size_t) step] >> pc) & 1) != 0)
                    {
                        g.setColour (colours::forNote (pc).withAlpha (0.13f));
                        g.fillRect (x, rowY (row), cw, rh);
                    }
                }
                if (! usable)
                {
                    g.setColour (juce::Colours::black.withAlpha (0.35f));
                    g.fillRect (x, grid.getY(), cw, grid.getHeight());
                }
                const bool barLine = step % melody::stepsPerBar == 0, beatLine = step % 4 == 0;
                g.setColour (juce::Colours::white.withAlpha (barLine ? 0.22f : beatLine ? 0.08f : 0.03f));
                g.drawVerticalLine ((int) x, grid.getY(), grid.getBottom());
            }
            for (int row = 0; row <= melody::numRows; ++row)
            {
                g.setColour (juce::Colours::white.withAlpha (row % 7 == 0 ? 0.12f : 0.035f));
                g.drawHorizontalLine ((int) (grid.getBottom() - (float) row * rh), grid.getX(), grid.getRight());
            }

            // Playhead
            if (view.playingStep >= 0)
            {
                const float x = grid.getX() + (float) view.playingStep * cw;
                g.setColour (colours::text.withAlpha (0.12f));
                g.fillRect (x, grid.getY(), cw, grid.getHeight());
            }

            // Notes
            for (int step = 0; step < melody::numSteps; ++step)
            {
                const auto c = view.tune.cell (step);
                if (c.empty() || c.hold) continue;
                drawNote (g, step, view.tune.lengthFrom (step), c.row, false);
            }
            if (drawing) drawNote (g, dragStart, juce::jmax (1, dragEnd - dragStart + 1), dragRow, true);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int step = stepAt (e.position.x), row = rowAt (e.position.y);
            if (step < 0 || row < 0 || step % melody::stepsPerBar >= view.stepsInBar) return;
            if (! view.tune.cell (step).empty())
            {
                if (onRemove) onRemove (step);
                return;
            }
            drawing = true;
            dragStart = dragEnd = step;
            dragRow = row;
            repaint();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! drawing) return;
            // A note stays inside its bar
            const int barEnd = (dragStart / melody::stepsPerBar) * melody::stepsPerBar + view.stepsInBar - 1;
            const int step = juce::jlimit (dragStart, barEnd, stepAt (juce::jmax (gridArea().getX(), e.position.x)));
            if (step != dragEnd) { dragEnd = step; repaint(); }
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (! drawing) return;
            drawing = false;
            if (onAdd) onAdd (dragStart, dragEnd - dragStart + 1, dragRow);
            repaint();
        }

    private:
        static constexpr float gutter = 40.0f;

        juce::Rectangle<float> gridArea() const { return getLocalBounds().toFloat().reduced (6.0f, 6.0f).withTrimmedLeft (gutter - 6.0f); }
        float cellWidth() const { return gridArea().getWidth() / (float) melody::numSteps; }
        float rowHeight() const { return gridArea().getHeight() / (float) melody::numRows; }
        float rowY (int row) const { return gridArea().getBottom() - (float) (row + 1) * rowHeight(); }

        int stepAt (float x) const
        {
            const auto grid = gridArea();
            if (x < grid.getX() || x >= grid.getRight()) return -1;
            return juce::jlimit (0, melody::numSteps - 1, (int) ((x - grid.getX()) / cellWidth()));
        }

        int rowAt (float y) const
        {
            const auto grid = gridArea();
            if (y < grid.getY() || y >= grid.getBottom()) return -1;
            return juce::jlimit (0, melody::numRows - 1, (int) ((grid.getBottom() - y) / rowHeight()));
        }

        void drawNote (juce::Graphics& g, int start, int length, int row, bool preview) const
        {
            const auto grid = gridArea();
            const int pc = view.rowPitchClass[(size_t) row];
            const auto colour = colours::forNote (pc);
            const bool inChord = ((view.chordMask[(size_t) start] >> pc) & 1) != 0;
            const auto r = juce::Rectangle<float> (grid.getX() + (float) start * cellWidth(), rowY (row),
                                                   (float) length * cellWidth(), rowHeight()).reduced (0.5f, 0.5f);
            if (inChord || preview) look::glow (g, r, 2.5f, colour, 0.9f);
            g.setGradientFill (juce::ColourGradient (colour.brighter (0.3f), 0.0f, r.getY(), colour.darker (inChord ? 0.0f : 0.3f), 0.0f, r.getBottom(), false));
            g.setOpacity (inChord || preview ? 1.0f : 0.7f);
            g.fillRoundedRectangle (r, 2.5f);
            g.setOpacity (1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawRoundedRectangle (r, 2.5f, 0.8f);
        }

        View view;
        bool drawing = false;
        int dragStart = 0, dragEnd = 0, dragRow = 0;
    };
}
