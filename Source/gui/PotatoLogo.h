#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Colours.h"
#include <cmath>

namespace pc::gui
{
    // The smiling potato emblem from Potato-Kicker, recoloured with this plugin's palette.
    // The eyes glow brighter while a chord plays.
    class PotatoLogo : public juce::Component
    {
    public:
        void setActivity (float a)
        {
            a = juce::jlimit (0.0f, 1.0f, a);
            if (std::abs (a - act) > 0.01f) { act = a; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            using namespace juce;
            const auto yellow = colours::notes[0], orange = colours::notes[1], pink = colours::notes[2];
            const auto b = getLocalBounds().toFloat();
            const Point<float> c (b.getCentreX(), b.getCentreY());
            const float R = jmin (b.getWidth() / 2.9f, b.getHeight() / 2.4f);
            const Path spud = potatoPath (c, R);

            // backlight
            g.setGradientFill (ColourGradient (orange.withAlpha (0.18f + 0.18f * act), c.x, c.y, orange.withAlpha (0.0f), c.x + R * 1.7f, c.y, true));
            g.fillEllipse (Rectangle<float> (R * 3.4f, R * 2.7f).withCentre (c));

            // body and glowing outline
            g.setGradientFill (ColourGradient (Colour (0xff3a2410), c.x - R * 0.35f, c.y - R * 0.45f, Colour (0xff100a06), c.x + R * 0.95f, c.y + R * 0.8f, true));
            g.fillPath (spud);
            const auto sb = spud.getBounds();
            auto rampFill = [&] (float a) { return ColourGradient (orange.withAlpha (a), sb.getX(), sb.getBottom(), yellow.withAlpha (a), sb.getRight(), sb.getY(), false); };
            g.setGradientFill (rampFill (0.16f)); g.strokePath (spud, PathStrokeType (R * 0.32f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setGradientFill (rampFill (0.40f)); g.strokePath (spud, PathStrokeType (R * 0.14f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setGradientFill (rampFill (1.0f));  g.strokePath (spud, PathStrokeType (jmax (1.2f, R * 0.06f), PathStrokeType::curved, PathStrokeType::rounded));

            // skin highlight and the little potato "eyes" (sprout spots)
            Path hl;
            hl.addCentredArc (c.x - R * 0.1f, c.y + R * 0.02f, R * 1.0f, R * 0.66f, degreesToRadians (-14.0f), degreesToRadians (-78.0f), degreesToRadians (-22.0f), true);
            g.setColour (yellow.withAlpha (0.35f));
            g.strokePath (hl, PathStrokeType (1.2f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (orange.withAlpha (0.5f));
            for (auto p : { Point<float> (-0.86f, 0.34f), Point<float> (0.88f, -0.40f), Point<float> (0.66f, 0.52f), Point<float> (-0.48f, 0.62f) })
                g.drawEllipse (Rectangle<float> (R * 0.12f, R * 0.08f).withCentre (c + p * R), 1.0f);

            // face: round eyes with glowing pupils, a smile and rosy cheeks
            for (int i = 0; i < 2; ++i)
            {
                const Point<float> e = c + Point<float> (i == 0 ? -0.34f : 0.30f, i == 0 ? -0.16f : -0.19f) * R;
                const auto socket = Rectangle<float> (R * 0.30f, R * 0.36f).withCentre (e);
                g.setColour (Colour (0xff080402)); g.fillEllipse (socket);
                g.setColour (orange.withAlpha (0.85f)); g.drawEllipse (socket, 1.1f);
                const float pr = R * (0.075f + 0.02f * act);
                const Point<float> pp = e + Point<float> (R * 0.025f, R * 0.03f);
                g.setColour (orange.withAlpha (0.25f + 0.4f * act)); g.fillEllipse (Rectangle<float> (pr * 4.0f, pr * 4.0f).withCentre (pp));
                g.setGradientFill (ColourGradient (Colour (0xfffff3c8), pp.x - pr * 0.3f, pp.y - pr * 0.3f, yellow, pp.x + pr, pp.y + pr, true));
                g.fillEllipse (Rectangle<float> (pr * 2.0f, pr * 2.0f).withCentre (pp));
                g.setColour (Colours::white.withAlpha (0.85f));
                g.fillEllipse (Rectangle<float> (pr * 0.6f, pr * 0.6f).withCentre (pp - Point<float> (pr * 0.35f, pr * 0.4f)));

                g.setColour (pink.withAlpha (0.35f + 0.25f * act));
                g.fillEllipse (Rectangle<float> (R * 0.2f, R * 0.11f).withCentre (e + Point<float> (i == 0 ? -0.08f : 0.1f, 0.36f) * R));
            }
            Path mouth;
            mouth.startNewSubPath (c.x - R * 0.22f, c.y + R * 0.30f);
            mouth.quadraticTo (c.x - R * 0.02f, c.y + R * 0.48f, c.x + R * 0.22f, c.y + R * 0.27f);
            g.setColour (orange.withAlpha (0.3f)); g.strokePath (mouth, PathStrokeType (R * 0.16f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (yellow);                  g.strokePath (mouth, PathStrokeType (jmax (1.2f, R * 0.07f), PathStrokeType::curved, PathStrokeType::rounded));
        }

    private:
        static juce::Path potatoPath (juce::Point<float> c, float R)
        {
            using namespace juce;
            const float radii[16] { 1.02f, 0.97f, 0.90f, 0.93f, 0.99f, 0.95f, 0.91f, 0.98f, 1.06f, 1.03f, 0.94f, 0.90f, 0.96f, 1.04f, 1.00f, 1.05f };
            const float tilt = degreesToRadians (-10.0f), ct = std::cos (tilt), st = std::sin (tilt);
            Point<float> pts[16];
            for (int i = 0; i < 16; ++i)
            {
                const float a = degreesToRadians ((float) i * 22.5f);
                const float x = std::cos (a) * R * 1.22f * radii[i], y = std::sin (a) * R * radii[i];
                pts[i] = c + Point<float> (x * ct - y * st, x * st + y * ct);
            }
            Path p;
            auto mid = [&] (int i, int j) { return (pts[i] + pts[j]) * 0.5f; };
            p.startNewSubPath (mid (15, 0));
            for (int i = 0; i < 16; ++i) p.quadraticTo (pts[i], mid (i, (i + 1) % 16));
            p.closeSubPath();
            return p;
        }

        float act = 0.0f;
    };
}
