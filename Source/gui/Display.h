#pragma once
#include "Look.h"
#include "Theory.h"
#include "Voicing.h"

namespace pc::gui
{
    // Glass display: the chord playing now (name, numeral, its notes as lit chips), or a hint when idle
    class Display : public juce::Component
    {
    public:
        void update (const juce::String& nameIn, const juce::String& numeralIn, int rootIn, const voicing::Voiced& notesIn,
                     bool flatsIn, const juce::String& idleIn, const juce::String& statusIn)
        {
            if (nameIn == name && numeralIn == numeral && rootIn == root && notesIn.size == notes.size
                && notesIn.notes == notes.notes && flatsIn == flats && idleIn == idle && statusIn == status)
                return;
            status = statusIn;
            name = nameIn;
            numeral = numeralIn;
            root = rootIn;
            notes = notesIn;
            flats = flatsIn;
            idle = idleIn;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            look::inset (g, r, 10.0f);
            auto glass = r.reduced (5.0f);
            g.setGradientFill (juce::ColourGradient (colours::screen.brighter (0.15f), 0.0f, glass.getY(),
                                                     colours::screen, 0.0f, glass.getBottom(), false));
            g.fillRoundedRectangle (glass, 7.0f);

            // Faint scan lines and a glass reflection
            g.setColour (juce::Colours::white.withAlpha (0.025f));
            for (float y = glass.getY() + 2.0f; y < glass.getBottom(); y += 3.0f)
                g.drawHorizontalLine ((int) y, glass.getX() + 4.0f, glass.getRight() - 4.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), 0.0f, glass.getY(),
                                                     juce::Colours::transparentWhite, 0.0f, glass.getCentreY(), false));
            g.fillRoundedRectangle (glass.withHeight (glass.getHeight() * 0.5f), 7.0f);

            auto inner = glass.reduced (14.0f, 4.0f);
            if (status.isNotEmpty())
            {
                // Transport status in the corner, like a hardware screen's top line
                g.setFont (look::font (11.5f));
                const auto c = status.startsWith ("PLAY") ? colours::notes[7] : colours::textDim;
                look::glowText (g, status, inner.removeFromRight (150.0f), c, juce::Justification::centredRight);
            }
            if (name.isEmpty())
            {
                g.setFont (look::font (15.0f, false));
                look::glowText (g, idle, inner, colours::textDim, juce::Justification::centredLeft);
                return;
            }

            const auto colour = colours::forNote (root);
            g.setFont (look::font (26.0f));
            look::glowText (g, name, inner.removeFromLeft (130.0f), colour, juce::Justification::centredLeft);
            g.setFont (look::font (18.0f));
            look::glowText (g, numeral, inner.removeFromLeft (90.0f), colours::text, juce::Justification::centredLeft);

            for (int i = 0; i < notes.size; ++i)
            {
                const int note = notes.notes[(size_t) i];
                auto chip = inner.removeFromLeft (56.0f).reduced (4.0f, 6.0f);
                const auto c = colours::forNote (note);
                look::glow (g, chip, 7.0f, c, 0.8f);
                g.setGradientFill (juce::ColourGradient (c.brighter (0.3f), 0.0f, chip.getY(), c, 0.0f, chip.getBottom(), false));
                g.fillRoundedRectangle (chip, 7.0f);
                g.setColour (colours::onNote);
                g.setFont (look::font (15.0f));
                // Octave numbers as Cubase shows them (MIDI 60 = C3)
                g.drawText (juce::String (theory::noteName (note, flats)) + juce::String (note / 12 - 2), chip, juce::Justification::centred);
            }
        }

    private:
        juce::String name, numeral;
        int root = 0;
        voicing::Voiced notes;
        bool flats = false;
        juce::String idle, status;
    };
}
