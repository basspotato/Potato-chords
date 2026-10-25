#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Colours.h"
#include "Theory.h"
#include <functional>

namespace pc::gui
{
    // One chord pad: Roman numeral, chord name and one colour stripe per chord note (with its name).
    // Press to play, release to stop.
    class ChordPad : public juce::Component
    {
    public:
        std::function<void (int)> onDown, onUp;

        explicit ChordPad (int degreeIndex) : degree (degreeIndex) {}

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
            auto r = getLocalBounds().toFloat().reduced (4.0f);
            const auto rootColour = colours::forNote (chord.root);

            g.setColour (active ? colours::panelLight : colours::panel);
            g.fillRoundedRectangle (r, 12.0f);
            g.setColour (active ? rootColour : colours::outline);
            g.drawRoundedRectangle (r, 12.0f, active ? 3.0f : 1.0f);

            auto inner = r.reduced (10.0f);
            g.setColour (rootColour);
            g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
            g.drawFittedText (numeral, inner.removeFromTop (40.0f).toNearestInt(), juce::Justification::centred, 1);
            g.setColour (colours::text);
            g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
            g.drawFittedText (name, inner.removeFromTop (28.0f).toNearestInt(), juce::Justification::centred, 1);

            // Colour stripes, lowest chord tone at the bottom, each labelled with its note name
            inner.removeFromTop (6.0f);
            const int notes = std::max (1, chord.size);
            const float h = std::min (24.0f, inner.getHeight() / (float) notes);
            auto stripes = inner.removeFromBottom (h * (float) notes);
            for (int i = 0; i < chord.size; ++i)
            {
                const int pc = (chord.root + chord.intervals[(size_t) i]) % 12;
                auto s = stripes.removeFromBottom (h).reduced (0.0f, 1.5f);
                g.setColour (colours::forNote (pc));
                g.fillRoundedRectangle (s, 5.0f);
                g.setColour (colours::onNote);
                g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
                g.drawText (theory::noteName (pc, flats), s, juce::Justification::centred);
            }
        }

        void mouseDown (const juce::MouseEvent&) override { if (onDown) onDown (degree); }
        void mouseUp (const juce::MouseEvent&) override   { if (onUp) onUp (degree); }

    private:
        int degree;
        theory::Chord chord;
        bool flats = false, active = false;
        juce::String name, numeral;
    };
}
