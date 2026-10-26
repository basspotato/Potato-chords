#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Look.h"
#include "Theory.h"
#include <functional>

namespace pc::gui
{
    // One chord pad: Roman numeral, chord name and one colour stripe per chord note (with its name).
    // Press to play, release to stop.
    class ChordPad : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::function<void (int)> onDown, onUp;

        explicit ChordPad (int degreeIndex) : degree (degreeIndex) {}

        // The mood word under the name (empty hides it)
        void setMood (const juce::String& m)
        {
            if (m != mood) { mood = m; repaint(); }
        }

        // Marks the pad as a good next chord: 1 = best, 2 and 3 = runners-up, 0 = none
        void setHint (int rank)
        {
            if (rank != hint) { hint = rank; repaint(); }
        }

        void setChord (const theory::Chord& c, bool useFlats, bool isActive)
        {
            const auto n = theory::chordName (c, useFlats);
            const auto r = theory::romanNumeral (c, degree);
            const auto newName = juce::String::fromUTF8 (n.text);
            const auto newNumeral = juce::String::fromUTF8 (r.text);
            if (newName == name && newNumeral == numeral && isActive == active && useFlats == flats
                && c.root == chord.root && c.intervals == chord.intervals && c.size == chord.size)
                return;
            chord = c;
            flats = useFlats;
            name = newName;
            numeral = newNumeral;
            active = isActive;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f, 6.0f).withTrimmedBottom (4.0f);
            const auto rootColour = colours::forNote (chord.root);

            // Lit pads glow in the root's colour and sit pressed into the panel
            if (active) look::glow (g, r, 14.0f, rootColour, 1.6f);
            look::raised (g, r, 14.0f, active ? colours::panel.interpolatedWith (rootColour, 0.22f) : colours::panel.brighter (0.15f), active);
            if (active) r = r.translated (0.0f, 1.5f);
            g.setColour (active ? rootColour : rootColour.withAlpha (0.35f));
            g.drawRoundedRectangle (r.reduced (2.0f), 12.0f, active ? 2.0f : 1.0f);

            if (hint > 0 && ! active)
            {
                const float strength = hint == 1 ? 1.0f : hint == 2 ? 0.65f : 0.4f;
                g.setColour (colours::accent.withAlpha (0.8f * strength));
                g.drawRoundedRectangle (r.reduced (1.0f), 13.0f, 1.5f);
                auto tag = juce::Rectangle<float> (40.0f, 16.0f).withCentre ({ r.getCentreX(), r.getY() + 1.0f });
                look::glow (g, tag, 8.0f, colours::accent, 0.8f * strength);
                g.setColour (colours::accent.withAlpha (0.5f + 0.5f * strength));
                g.fillRoundedRectangle (tag, 8.0f);
                g.setColour (colours::text);
                g.setFont (look::font (10.5f));
                g.drawText (hint == 1 ? "NEXT" : hint == 2 ? "2nd" : "3rd", tag, juce::Justification::centred, false);
            }

            auto inner = r.reduced (10.0f);
            look::led (g, { inner.getRight() - 4.0f, inner.getY() + 4.0f }, 3.5f, rootColour, active);

            g.setFont (look::font (32.0f));
            look::glowText (g, numeral, inner.removeFromTop (42.0f), rootColour);
            g.setFont (look::font (19.0f));
            g.setColour (colours::text);
            g.drawFittedText (name, inner.removeFromTop (26.0f).toNearestInt(), juce::Justification::centred, 1);
            if (mood.isNotEmpty())
            {
                g.setColour (rootColour.withAlpha (0.85f));
                g.setFont (look::font (12.5f, false));
                g.drawText (mood, inner.removeFromTop (16.0f), juce::Justification::centred, false);
            }

            // Colour stripes as LED bars in a small recessed window, lowest chord tone at the bottom
            inner.removeFromTop (8.0f);
            const int count = std::max (1, chord.size);
            const float h = std::min (24.0f, (inner.getHeight() - 8.0f) / (float) count);
            auto window = inner.removeFromBottom (h * (float) count + 8.0f);
            look::inset (g, window, 7.0f);
            auto stripes = window.reduced (4.0f);
            for (int i = 0; i < chord.size; ++i)
            {
                const int pc = (chord.root + chord.intervals[(size_t) i]) % 12;
                const auto c = colours::forNote (pc);
                auto s = stripes.removeFromBottom (h).reduced (0.0f, 1.5f);
                if (active) look::glow (g, s, 4.0f, c, 0.6f);
                g.setGradientFill (juce::ColourGradient (c.brighter (0.25f), 0.0f, s.getY(), c.darker (0.1f), 0.0f, s.getBottom(), false));
                g.fillRoundedRectangle (s, 4.0f);
                g.setColour (juce::Colours::white.withAlpha (0.3f));
                g.drawHorizontalLine ((int) s.getY() + 1, s.getX() + 4.0f, s.getRight() - 4.0f);
                g.setColour (colours::onNote);
                g.setFont (look::font (13.0f));
                g.drawText (theory::noteName (pc, flats), s, juce::Justification::centred);
            }
        }

        void mouseDown (const juce::MouseEvent&) override { if (onDown) onDown (degree); }
        void mouseUp (const juce::MouseEvent&) override   { if (onUp) onUp (degree); }

    private:
        int degree;
        theory::Chord chord;
        bool flats = false, active = false;
        int hint = 0;
        juce::String name, numeral, mood;
    };
}
