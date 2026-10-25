#pragma once
#include "Look.h"
#include "Progression.h"
#include <functional>

namespace pc::gui
{
    // What one lane slot shows; filled in by the editor from the chord theory
    struct SlotView
    {
        bool empty = true;
        juce::String numeral, name;
        int root = 0;
        std::array<int, 4> pitchClasses {};
        int size = 0;

        bool operator== (const SlotView& o) const
        {
            return empty == o.empty && numeral == o.numeral && name == o.name && root == o.root && pitchClasses == o.pitchClasses && size == o.size;
        }
    };

    // The 8-bar progression lane. Click a slot to select it (then tap a pad to fill it), click the small
    // split mark to put two chords in a bar, right-click for more. The playing slot glows.
    class ProgressionLane : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::function<void (int)> onSelect;          // slot index, or -1 to deselect
        std::function<void (int, bool)> onSplit;     // bar, split
        std::function<void (int)> onClear;           // slot index

        void update (const std::array<SlotView, progression::numSlots>& viewsIn, uint8_t splitsIn, int playingIn, int selectedIn)
        {
            if (viewsIn == views && splitsIn == splits && playingIn == playing && selectedIn == selected)
                return;
            views = viewsIn;
            splits = splitsIn;
            playing = playingIn;
            selected = selectedIn;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            look::inset (g, getLocalBounds().toFloat(), 12.0f);
            for (int bar = 0; bar < progression::numBars; ++bar)
            {
                const auto b = barBounds (bar);
                g.setColour (colours::textDim);
                g.setFont (look::font (11.0f));
                g.drawText (juce::String (bar + 1), b.withHeight (14.0f).withTrimmedLeft (4.0f), juce::Justification::centredLeft, false);
                drawSplitMark (g, bar);

                if (isSplit (bar))
                {
                    drawSlot (g, bar * 2, slotBounds (bar * 2));
                    drawSlot (g, bar * 2 + 1, slotBounds (bar * 2 + 1));
                }
                else
                {
                    drawSlot (g, bar * 2, slotBounds (bar * 2));
                }
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto p = e.position;
            for (int bar = 0; bar < progression::numBars; ++bar)
            {
                if (splitMarkBounds (bar).expanded (3.0f).contains (p))
                {
                    if (onSplit) onSplit (bar, ! isSplit (bar));
                    return;
                }
                for (int half = 0; half < (isSplit (bar) ? 2 : 1); ++half)
                {
                    const int slot = bar * 2 + half;
                    if (! slotBounds (slot).contains (p)) continue;
                    if (e.mods.isPopupMenu())
                        showMenu (slot);
                    else if (onSelect)
                        onSelect (slot == selected ? -1 : slot);
                    return;
                }
            }
        }

    private:
        bool isSplit (int bar) const { return ((splits >> bar) & 1) != 0; }

        juce::Rectangle<float> barBounds (int bar) const
        {
            auto area = getLocalBounds().toFloat().reduced (8.0f, 6.0f);
            const float w = area.getWidth() / (float) progression::numBars;
            return { area.getX() + w * (float) bar, area.getY(), w, area.getHeight() };
        }

        juce::Rectangle<float> splitMarkBounds (int bar) const
        {
            const auto b = barBounds (bar);
            return { b.getRight() - 24.0f, b.getY() + 1.0f, 18.0f, 12.0f };
        }

        juce::Rectangle<float> slotBounds (int slot) const
        {
            const int bar = slot / 2;
            auto b = barBounds (bar).withTrimmedTop (16.0f).reduced (3.0f, 0.0f);
            if (! isSplit (bar)) return b;
            return slot % 2 == 0 ? b.removeFromLeft (b.getWidth() / 2.0f - 2.0f) : b.removeFromRight (b.getWidth() / 2.0f - 2.0f);
        }

        void drawSplitMark (juce::Graphics& g, int bar) const
        {
            const auto r = splitMarkBounds (bar);
            g.setColour (isSplit (bar) ? colours::accent : colours::outline.brighter (0.3f));
            g.drawRoundedRectangle (r, 3.0f, 1.0f);
            g.drawVerticalLine ((int) r.getCentreX(), r.getY() + 2.0f, r.getBottom() - 2.0f);
        }

        void drawSlot (juce::Graphics& g, int slot, juce::Rectangle<float> r) const
        {
            const auto& v = views[(size_t) slot];
            const bool isPlaying = playing == slot || (playing == slot + 1 && slot % 2 == 0 && ! isSplit (slot / 2));
            const bool isSelected = selected == slot;
            const auto colour = colours::forNote (v.root);

            if (v.empty)
            {
                look::inset (g, r, 8.0f, colours::well.brighter (0.15f));
                g.setColour (colours::textDim.withAlpha (0.6f));
                g.setFont (look::font (18.0f, false));
                g.drawText ("+", r, juce::Justification::centred, false);
            }
            else
            {
                if (isPlaying) look::glow (g, r, 8.0f, colour, 1.4f);
                look::raised (g, r, 8.0f, isPlaying ? colours::panel.interpolatedWith (colour, 0.3f) : colours::panel.brighter (0.12f), isPlaying);
                auto inner = r.reduced (4.0f, 3.0f);

                // Note colour stripes along the bottom
                auto stripes = inner.removeFromBottom (7.0f);
                const float w = stripes.getWidth() / (float) juce::jmax (1, v.size);
                for (int i = 0; i < v.size; ++i)
                {
                    g.setColour (colours::forNote (v.pitchClasses[(size_t) i]));
                    g.fillRoundedRectangle (stripes.removeFromLeft (w).reduced (0.75f, 0.0f), 2.0f);
                }

                g.setFont (look::font (inner.getWidth() < 50.0f ? 15.0f : 19.0f));
                look::glowText (g, v.numeral, inner.removeFromTop (inner.getHeight() * 0.55f), colour);
                g.setColour (colours::text);
                g.setFont (look::font (inner.getWidth() < 50.0f ? 11.0f : 13.0f));
                g.drawFittedText (v.name, inner.toNearestInt(), juce::Justification::centred, 1);
            }

            if (isSelected)
            {
                look::glow (g, r, 8.0f, colours::accent, 1.0f);
                g.setColour (colours::accent);
                g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 2.0f);
            }
        }

        void showMenu (int slot)
        {
            const int bar = slot / 2;
            juce::PopupMenu m;
            m.addItem (1, isSplit (bar) ? "One chord in this bar" : "Two chords in this bar");
            m.addItem (2, "Clear this chord", ! views[(size_t) slot].empty);
            juce::Component::SafePointer<ProgressionLane> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, slot, bar] (int result)
            {
                if (safe == nullptr) return;
                if (result == 1 && safe->onSplit) safe->onSplit (bar, ! safe->isSplit (bar));
                if (result == 2 && safe->onClear) safe->onClear (slot);
            });
        }

        std::array<SlotView, progression::numSlots> views {};
        uint8_t splits = 0;
        int playing = -1, selected = -1;
    };
}
