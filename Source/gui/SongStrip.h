#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Look.h"
#include "../Song.h"
#include <functional>

// The Song view: one block per section, as wide as its bars. Drag a block into the DAW for that section's MIDI;
// right-click to change its length or kind, add a section after it, or remove it.
namespace pc::gui
{
    class SongStrip : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::function<juce::File (int)> makeSectionFile;  // section index -> temp .mid
        std::function<void (const song::Song&)> onEdit;   // the user changed the sections

        // playing = the section the host is in (-1 when Song play is off or stopped)
        void update (const song::Song& s, int playing = -1)
        {
            if (s == current && playing == playingNow) return;
            current = s;
            playingNow = playing;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const auto area = getLocalBounds().toFloat();
            look::inset (g, area, 10.0f);
            const auto inner = area.reduced (8.0f, 10.0f);
            blocks.clear();
            const int total = juce::jmax (1, current.totalBars());
            g.setFont (look::font (12.0f, false));
            g.setColour (colours::textDim);
            g.drawText (juce::String (current.count) + " sections, " + juce::String (current.totalBars()) + " bars. "
                            "Drag a block into your DAW for that section's MIDI. Right-click to edit.",
                        inner.withHeight (16.0f), juce::Justification::centredLeft);

            auto row = inner.withTrimmedTop (22.0f);
            float x = row.getX();
            for (int i = 0; i < current.count; ++i)
            {
                const auto& sec = current.sections[(size_t) i];
                const float w = row.getWidth() * (float) sec.bars / (float) total;
                auto r = juce::Rectangle<float> (x, row.getY(), w, row.getHeight()).reduced (2.0f, 0.0f);
                blocks.push_back (r);
                x += w;

                const auto c = colours::forSection ((int) sec.kind);
                const bool hot = i == hover || i == playingNow;
                look::glow (g, r, 6.0f, c, hot ? 0.9f : 0.35f);
                look::raised (g, r, 7.0f, colours::panelLight, i == playingNow);
                // Coloured top band and loop marker
                g.setColour (c.withAlpha (hot ? 0.95f : 0.8f));
                g.fillRoundedRectangle (r.withHeight (6.0f).reduced (3.0f, 0.0f).translated (0.0f, 4.0f), 3.0f);

                auto text = r.reduced (3.0f, 12.0f);
                g.setColour (c);
                g.setFont (look::font (r.getWidth() < 46.0f ? 11.0f : 14.0f));
                g.drawText (song::info (sec.kind).name, text.removeFromTop (20.0f), juce::Justification::centred, true);
                g.setColour (colours::text);
                g.setFont (look::font (12.0f, false));
                g.drawText (juce::String (sec.bars) + " bars", text.removeFromTop (16.0f), juce::Justification::centred, true);
                g.setColour (colours::textDim);
                g.setFont (look::font (11.0f, false));
                g.drawText (song::info (sec.kind).loop == 0 ? "loop A" : "loop B", text.removeFromTop (14.0f),
                            juce::Justification::centred, true);
                if (song::info (sec.kind).melody)
                    g.drawText ("+ melody", text.removeFromTop (14.0f), juce::Justification::centred, true);
            }
        }

        void mouseMove (const juce::MouseEvent& e) override { setHover (blockAt (e.position)); }
        void mouseExit (const juce::MouseEvent&) override { setHover (-1); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragging = false;
            if (e.mods.isPopupMenu())
                if (const int i = blockAt (e.position); i >= 0) showMenu (i);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragging || e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 4 || ! makeSectionFile) return;
            const int i = blockAt (e.mouseDownPosition);
            if (i < 0) return;
            const auto file = makeSectionFile (i);
            if (! file.existsAsFile()) return;
            dragging = true;
            juce::Component::SafePointer<SongStrip> safe (this);
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                         [safe] { if (safe != nullptr) safe->dragging = false; });
        }

        void mouseUp (const juce::MouseEvent&) override { dragging = false; }

    private:
        int blockAt (juce::Point<float> p) const
        {
            for (size_t i = 0; i < blocks.size(); ++i)
                if (blocks[i].contains (p)) return (int) i;
            return -1;
        }

        void setHover (int i)
        {
            if (i == hover) return;
            hover = i;
            repaint();
        }

        void showMenu (int index)
        {
            juce::PopupMenu lengths, kindsMenu, addAfter;
            const auto& sec = current.sections[(size_t) index];
            for (int bars : { 4, 8, 16, 32 })
                lengths.addItem (100 + bars, juce::String (bars) + " bars", true, sec.bars == bars);
            for (int k = 0; k < song::numKinds; ++k)
            {
                kindsMenu.addItem (200 + k, song::kinds[(size_t) k].name, true, (int) sec.kind == k);
                addAfter.addItem (300 + k, song::kinds[(size_t) k].name, current.count < song::maxSections);
            }
            juce::PopupMenu m;
            m.addSubMenu ("Length", lengths);
            m.addSubMenu ("Section type", kindsMenu);
            m.addSubMenu ("Add after", addAfter);
            m.addItem (1, "Remove", current.count > 1);
            juce::Component::SafePointer<SongStrip> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, index] (int result)
            {
                if (safe == nullptr || result == 0 || index >= safe->current.count) return;
                auto s = safe->current;
                auto& sec = s.sections[(size_t) index];
                if (result == 1) s.remove (index);
                else if (result >= 300) s.insert (index + 1, (song::Kind) (result - 300), 8);
                else if (result >= 200) sec.kind = (song::Kind) (result - 200);
                else if (result >= 100) sec.bars = result - 100;
                if (safe->onEdit) safe->onEdit (s);
            });
        }

        song::Song current;
        std::vector<juce::Rectangle<float>> blocks;
        int hover = -1, playingNow = -1;
        bool dragging = false;
    };
}
