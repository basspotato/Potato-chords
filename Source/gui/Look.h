#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Colours.h"

// Drawing helpers for the 3D hardware look: textured background, raised and recessed surfaces, glows, LEDs.
// Everything visual lives in gui/ so the look can be swapped out later.
namespace pc::gui::look
{
    inline juce::Font font (float size, bool bold = true)
    {
        return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain);
    }

    // Deep indigo with a fine brushed grain, coloured ambient light and a vignette. Built once per size.
    inline juce::Image makeBackground (int w, int h)
    {
        juce::Image img (juce::Image::ARGB, juce::jmax (1, w), juce::jmax (1, h), true);
        {
            juce::Graphics g (img);
            g.setGradientFill (juce::ColourGradient (colours::backgroundTop, 0.0f, 0.0f, colours::backgroundBottom, 0.0f, (float) h, false));
            g.fillAll();
            // Ambient light from the palette: pink top-left, cyan bottom-right
            g.setGradientFill (juce::ColourGradient (colours::notes[2].withAlpha (0.10f), 0.0f, 0.0f,
                                                     juce::Colours::transparentBlack, (float) w * 0.55f, (float) h * 0.45f, true));
            g.fillAll();
            g.setGradientFill (juce::ColourGradient (colours::notes[5].withAlpha (0.08f), (float) w, (float) h,
                                                     juce::Colours::transparentBlack, (float) w * 0.4f, (float) h * 0.5f, true));
            g.fillAll();
        }

        // Grain: per-row streaks plus per-pixel noise reads as brushed metal
        juce::Random random (0x7a7e);
        juce::Image::BitmapData data (img, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < h; ++y)
        {
            const float row = random.nextFloat() - 0.5f;
            for (int x = 0; x < w; ++x)
            {
                const float n = (0.55f * row + 0.45f * (random.nextFloat() - 0.5f)) * 0.09f;
                const auto c = data.getPixelColour (x, y);
                data.setPixelColour (x, y, n > 0 ? c.brighter (n) : c.darker (-n * 1.4f));
            }
        }

        juce::Graphics g (img);
        juce::ColourGradient vignette (juce::Colours::transparentBlack, (float) w / 2.0f, (float) h / 2.0f,
                                       juce::Colours::black.withAlpha (0.5f), 0.0f, 0.0f, true);
        vignette.addColour (0.6, juce::Colours::transparentBlack);
        g.setGradientFill (vignette);
        g.fillAll();
        return img;
    }

    inline void shadow (juce::Graphics& g, juce::Rectangle<float> r, float radius, float depth = 1.0f)
    {
        for (int i = 4; i >= 1; --i)
        {
            const float spread = (float) i * 1.6f * depth;
            g.setColour (juce::Colours::black.withAlpha (0.11f));
            g.fillRoundedRectangle (r.expanded (spread * 0.6f).translated (0.0f, spread), radius + spread * 0.6f);
        }
    }

    // A raised surface lit from above: soft drop shadow, top-to-bottom gradient, highlight on the top edge
    inline void raised (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour base, bool pressed = false)
    {
        if (! pressed) shadow (g, r, radius);
        else           r = r.translated (0.0f, 1.5f);

        g.setGradientFill (juce::ColourGradient (base.brighter (pressed ? 0.05f : 0.22f), 0.0f, r.getY(),
                                                 base.darker (pressed ? 0.15f : 0.35f), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);

        juce::ColourGradient rim (juce::Colours::white.withAlpha (pressed ? 0.06f : 0.22f), 0.0f, r.getY(),
                                  juce::Colours::transparentWhite, 0.0f, r.getY() + juce::jmin (18.0f, r.getHeight() * 0.5f), false);
        g.setGradientFill (rim);
        g.drawRoundedRectangle (r.reduced (1.0f), radius - 1.0f, 1.2f);

        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (r, radius, 1.0f);
    }

    // A recessed well: darker, shadowed at the top, a faint lit lip at the bottom
    inline void inset (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour base = colours::well)
    {
        g.setGradientFill (juce::ColourGradient (base.darker (0.5f), 0.0f, r.getY(), base, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);

        juce::ColourGradient top (juce::Colours::black.withAlpha (0.55f), 0.0f, r.getY(),
                                  juce::Colours::transparentBlack, 0.0f, r.getY() + 10.0f, false);
        g.setGradientFill (top);
        g.fillRoundedRectangle (r, radius);

        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawLine (r.getX() + radius, r.getBottom() + 0.5f, r.getRight() - radius, r.getBottom() + 0.5f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r, radius, 1.0f);
    }

    // Soft light spilling around a lit shape
    inline void glow (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour c, float strength = 1.0f)
    {
        for (int i = 6; i >= 1; --i)
        {
            const float e = (float) i * 2.2f;
            g.setColour (c.withAlpha (0.07f * strength * (1.0f - (float) i / 7.5f)));
            g.fillRoundedRectangle (r.expanded (e), radius + e);
        }
    }

    inline void led (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour c, bool on)
    {
        const auto r = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
        if (on)
        {
            glow (g, r, radius, c, 1.4f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white, centre.x, centre.y - radius * 0.3f,
                                                     c, centre.x, centre.y + radius, true));
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (c.withMultipliedBrightness (0.35f).withMultipliedSaturation (0.6f), centre.x, centre.y - radius,
                                                     juce::Colours::black, centre.x, centre.y + radius, false));
        }
        g.fillEllipse (r);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (r, 0.8f);
    }

    // Text with a soft halo in its own colour, for lit labels
    inline void glowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Colour c,
                          juce::Justification just = juce::Justification::centred)
    {
        g.setColour (c.withAlpha (0.25f));
        for (auto offset : { juce::Point<float> (-1.0f, 0.0f), { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
            g.drawText (text, r.translated (offset.x, offset.y), just, false);
        g.setColour (c);
        g.drawText (text, r, just, false);
    }

    // LookAndFeel for the stock JUCE parts (the key combo box and its menu)
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setColourScheme ({ colours::panel, colours::backgroundBottom, colours::panel, colours::outline, colours::text,
                               colours::panelLight, colours::text, colours::accent, colours::text });
            setColour (juce::PopupMenu::backgroundColourId, colours::panel);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
            setColour (juce::ComboBox::textColourId, colours::text);
        }

        void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
        {
            const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (2.0f, 3.0f);
            raised (g, r, 7.0f, colours::panelLight, box.isPopupActive());
            juce::Path arrow;
            const float cx = r.getRight() - 14.0f, cy = r.getCentreY();
            arrow.addTriangle (cx - 5.0f, cy - 2.5f, cx + 5.0f, cy - 2.5f, cx, cy + 3.5f);
            g.setColour (colours::accent);
            g.fillPath (arrow);
        }

        juce::Font getComboBoxFont (juce::ComboBox&) override { return font (17.0f); }

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
            label.setFont (getComboBoxFont (box));
        }

        juce::Font getPopupMenuFont() override { return font (16.0f, false); }
    };
}
