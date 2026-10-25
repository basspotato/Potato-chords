#pragma once
#include "Look.h"
#include "Theory.h"
#include <bitset>

namespace pc::gui
{
    // Coloured keyboard: every key carries its note colour and name; sounding notes light up and glow.
    // When Chord keys is on, the chord octave is marked with the numeral each white key plays.
    class Keyboard : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        static constexpr int lowNote = 36, highNote = 96;  // C1 to C6 in Cubase's naming

        // Returns true when something changed, so the caller only repaints then
        void update (const std::bitset<128>& sounding, bool flatsIn, bool chordKeysOn, int chordLow,
                     const std::array<juce::String, 7>& numeralsIn)
        {
            if (sounding == lit && flatsIn == flats && chordKeysOn == chordKeys && chordLow == chordLowNote && numeralsIn == numerals)
                return;
            lit = sounding;
            flats = flatsIn;
            chordKeys = chordKeysOn;
            chordLowNote = chordLow;
            numerals = numeralsIn;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto area = getLocalBounds().toFloat();
            look::inset (g, area, 12.0f);
            area = area.reduced (10.0f, 8.0f);

            auto zone = area.removeFromTop (22.0f);
            keys = area;
            const float w = whiteWidth();

            // Chord-keys zone: a lit bracket over the octave with each white key's numeral
            if (chordKeys)
            {
                const float x0 = keyX (chordLowNote), x1 = keyX (chordLowNote + 11) + w;
                const auto bar = juce::Rectangle<float> (x0 + 2.0f, zone.getBottom() - 5.0f, x1 - x0 - 4.0f, 3.0f);
                look::glow (g, bar, 1.5f, colours::accent, 1.2f);
                g.setColour (colours::accent);
                g.fillRoundedRectangle (bar, 1.5f);
                g.setFont (look::font (12.0f));
                for (int note = chordLowNote; note < chordLowNote + 12; ++note)
                    if (const int degree = theory::whiteKeyDegree (note); degree >= 0)
                        look::glowText (g, numerals[(size_t) degree], { keyX (note), zone.getY(), w, 15.0f }, colours::text);
            }
            else
            {
                g.setColour (colours::textDim);
                g.setFont (look::font (11.0f));
                g.drawText ("NOTES PLAYING", zone.withTrimmedBottom (4.0f), juce::Justification::centredLeft);
            }

            for (int note = lowNote; note <= highNote; ++note)
                if (! isBlack (note)) drawWhite (g, note);
            for (int note = lowNote; note <= highNote; ++note)
                if (isBlack (note)) drawBlack (g, note);
        }

    private:
        static bool isBlack (int note) { return theory::whiteKeyDegree (note) < 0; }

        static int whiteIndex (int note)
        {
            int n = 0;
            for (int i = lowNote; i < note; ++i) n += isBlack (i) ? 0 : 1;
            return n;
        }

        float whiteWidth() const
        {
            const int whites = whiteIndex (highNote) + 1;
            return keys.getWidth() / (float) whites;
        }

        // Left edge of a white key, or of the white key below a black one
        float keyX (int note) const { return keys.getX() + (float) whiteIndex (isBlack (note) ? note - 1 : note) * whiteWidth(); }

        juce::String label (int note) const
        {
            juce::String s (theory::noteName (note, flats));
            if (note % 12 == 0) s << (note / 12 - 2);  // octave number on every C, Cubase style (MIDI 60 = C3)
            return s;
        }

        void drawWhite (juce::Graphics& g, int note) const
        {
            const float w = whiteWidth();
            auto r = juce::Rectangle<float> (keyX (note), keys.getY(), w, keys.getHeight()).reduced (1.0f, 0.0f);
            const auto colour = colours::forNote (note);
            const bool on = lit[(size_t) note];

            if (on)
            {
                r = r.withTrimmedTop (1.0f).translated (0.0f, 2.0f);  // pressed down
                g.setGradientFill (juce::ColourGradient (colour.brighter (0.35f), 0.0f, r.getY(), colour, 0.0f, r.getBottom(), false));
            }
            else
            {
                g.setGradientFill (juce::ColourGradient (colours::whiteKey.darker (0.25f), 0.0f, r.getY(), colours::whiteKey, 0.0f, r.getY() + 26.0f, false));
            }
            g.fillRoundedRectangle (r, 4.0f);

            // Front lip gives the key its depth
            auto lip = r.removeFromBottom (on ? 4.0f : 7.0f);
            g.setColour ((on ? colour : colours::whiteKey).darker (0.45f));
            g.fillRoundedRectangle (lip, 3.0f);

            // Colour band and name, always visible
            auto band = r.removeFromBottom (5.0f).reduced (3.0f, 0.0f);
            g.setColour (on ? colour.darker (0.25f) : colour);
            g.fillRoundedRectangle (band, 2.0f);
            g.setColour (on ? colours::onNote : colours::backgroundBottom.withAlpha (0.75f));
            g.setFont (look::font (w < 26.0f ? 10.0f : 11.5f));
            g.drawText (label (note), r.removeFromBottom (18.0f), juce::Justification::centred, false);

            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawRoundedRectangle (juce::Rectangle<float> (keyX (note), keys.getY(), w, keys.getHeight()).reduced (1.0f, 0.0f), 4.0f, 0.8f);
        }

        void drawBlack (juce::Graphics& g, int note) const
        {
            const float w = whiteWidth() * 0.62f;
            const float x = keyX (note) + whiteWidth() - w / 2.0f;
            auto r = juce::Rectangle<float> (x, keys.getY(), w, keys.getHeight() * 0.6f);
            const auto colour = colours::forNote (note);
            const bool on = lit[(size_t) note];

            look::shadow (g, r, 3.0f, 0.6f);
            if (on) look::glow (g, r, 3.0f, colour, 1.2f);
            g.setGradientFill (juce::ColourGradient (on ? colour : colours::blackKey.brighter (0.2f), 0.0f, r.getY(),
                                                     on ? colour.darker (0.4f) : colours::blackKey.darker (0.6f), 0.0f, r.getBottom(), false));
            g.fillRoundedRectangle (r, 3.0f);

            // Top face highlight, shorter when pressed
            auto face = r.reduced (2.5f, 0.0f).withTrimmedBottom (on ? 4.0f : 8.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (on ? 0.25f : 0.12f), 0.0f, face.getY(),
                                                     juce::Colours::transparentWhite, 0.0f, face.getBottom(), false));
            g.fillRoundedRectangle (face, 2.0f);

            auto band = r.removeFromBottom (on ? 4.0f : 9.0f).removeFromTop (3.0f).reduced (2.5f, 0.0f);
            g.setColour (on ? colour.brighter (0.3f) : colour);
            g.fillRoundedRectangle (band, 1.5f);
            g.setColour (on ? colours::onNote : colours::text.withAlpha (0.75f));
            g.setFont (look::font (8.5f));
            g.drawText (theory::noteName (note, flats), r.removeFromBottom (14.0f), juce::Justification::centred, false);

            g.setColour (juce::Colours::black);
            g.drawRoundedRectangle (juce::Rectangle<float> (x, keys.getY(), w, keys.getHeight() * 0.6f), 3.0f, 0.8f);
        }

        std::bitset<128> lit;
        bool flats = false, chordKeys = false;
        int chordLowNote = 60;
        std::array<juce::String, 7> numerals;
        juce::Rectangle<float> keys;
    };
}
