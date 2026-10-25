#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Look.h"
#include <functional>

// Own-look controls bound to host parameters. Kept here so the look can be replaced later.
namespace pc::gui
{
    // A row of buttons for a choice parameter; the selected one is lit
    class ChoiceStrip : public juce::Component
    {
    public:
        ChoiceStrip (juce::RangedAudioParameter& p, juce::StringArray labels)
            : names (std::move (labels)),
              attachment (p, [this] (float v) { selected = (int) v; repaint(); })
        {
            attachment.sendInitialUpdate();
        }

        // Optional label drawn inside the strip, on the left
        void setCaption (juce::String text, int width) { caption = std::move (text); captionWidth = width; repaint(); }

        void paint (juce::Graphics& g) override
        {
            look::inset (g, getLocalBounds().toFloat(), 9.0f);
            if (captionWidth > 0)
            {
                g.setFont (look::font (13.0f));
                g.setColour (colours::text);
                g.drawText (caption, juce::Rectangle<int> (8, 0, captionWidth - 8, getHeight()), juce::Justification::centredLeft);
            }
            const auto w = (float) (getWidth() - captionWidth) / (float) names.size();
            for (int i = 0; i < names.size(); ++i)
            {
                auto r = juce::Rectangle<float> ((float) captionWidth + w * (float) i, 0.0f, w, (float) getHeight()).reduced (3.0f, 3.5f);
                const bool on = i == selected;
                if (on) look::glow (g, r, 7.0f, colours::accent, 1.0f);
                look::raised (g, r, 7.0f, on ? colours::accent : colours::panelLight, on);
                if (on) r = r.translated (0.0f, 1.5f);
                g.setFont (look::font (14.0f, on));
                g.setColour (on ? colours::text : colours::textDim);
                g.drawText (names[i], r, juce::Justification::centred);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.x < captionWidth) return;
            const int i = juce::jlimit (0, names.size() - 1,
                                        (int) ((float) (e.x - captionWidth) / (float) (getWidth() - captionWidth) * (float) names.size()));
            attachment.setValueAsCompleteGesture ((float) i);
        }

    private:
        juce::StringArray names;
        juce::String caption;
        int selected = 0, captionWidth = 0;
        juce::ParameterAttachment attachment;
    };

    // A pill switch with a label for a bool parameter
    class Switch : public juce::Component
    {
    public:
        Switch (juce::RangedAudioParameter& p, juce::String labelText)
            : label (std::move (labelText)),
              attachment (p, [this] (float v) { on = v > 0.5f; repaint(); })
        {
            attachment.sendInitialUpdate();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (2.0f, 3.0f);
            look::raised (g, r, 8.0f, colours::panelLight, on);
            if (on) r = r.translated (0.0f, 1.5f);
            look::led (g, { r.getX() + 16.0f, r.getCentreY() }, 5.0f, colours::accent, on);
            g.setFont (look::font (14.0f, on));
            g.setColour (on ? colours::text : colours::textDim);
            g.drawText (label, r.withTrimmedLeft (28.0f).withTrimmedRight (6.0f), juce::Justification::centred);
        }

        void mouseDown (const juce::MouseEvent&) override { attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

    private:
        juce::String label;
        bool on = false;
        juce::ParameterAttachment attachment;
    };

    // A horizontal fader for a 0-1 parameter: label on the left, percentage on the right.
    // Drag (or click) to set, double-click for the default.
    class Fader : public juce::Component
    {
    public:
        // showValue: print the parameter's own value (e.g. a velocity) instead of a percentage
        Fader (juce::RangedAudioParameter& p, juce::String labelText, bool showValue = false)
            : param (p), label (std::move (labelText)), rawValue (showValue),
              attachment (p, [this] (float v) { value = v; repaint(); })
        {
            attachment.sendInitialUpdate();
        }

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (1.0f, 2.0f);
            look::inset (g, r, 8.0f);
            const auto track = r.reduced (3.0f);
            const float norm = param.convertTo0to1 (value);
            if (norm > 0.0f)
            {
                const auto fill = track.withWidth (juce::jmax (track.getHeight(), track.getWidth() * norm));
                look::glow (g, fill, 6.0f, colours::accent, 0.35f + 0.65f * norm);
                g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.55f), fill.getX(), 0.0f,
                                                         colours::accent, fill.getRight(), 0.0f, false));
                g.fillRoundedRectangle (fill, 6.0f);
            }
            g.setFont (look::font (13.0f));
            g.setColour (colours::text);
            g.drawText (label, track.withTrimmedLeft (8.0f), juce::Justification::centredLeft);
            g.drawText (rawValue ? juce::String (juce::roundToInt (value)) : juce::String (juce::roundToInt (norm * 100.0f)) + "%", track.withTrimmedRight (8.0f), juce::Justification::centredRight);
        }

        void mouseDown (const juce::MouseEvent& e) override { attachment.beginGesture(); set (e); }
        void mouseDrag (const juce::MouseEvent& e) override { set (e); }
        void mouseUp (const juce::MouseEvent&) override { attachment.endGesture(); }
        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
        }

    private:
        void set (const juce::MouseEvent& e)
        {
            const float norm = juce::jlimit (0.0f, 1.0f, (float) (e.x - 3) / (float) juce::jmax (1, getWidth() - 6));
            attachment.setValueAsPartOfGesture (param.convertFrom0to1 (norm));
        }

        juce::RangedAudioParameter& param;
        juce::String label;
        bool rawValue = false;
        float value = 0.0f;
        juce::ParameterAttachment attachment;
    };

    // A push button in the same raised style
    class ActionButton : public juce::Component
    {
    public:
        std::function<void()> onClick;

        // arrow: 0 = text label, -1 = undo arrow, 1 = redo arrow
        explicit ActionButton (juce::String labelText, int arrowIcon = 0) : label (std::move (labelText)), arrow (arrowIcon) {}

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (2.0f, 3.0f);
            look::raised (g, r, 8.0f, colours::panelLight, down);
            if (down) r = r.translated (0.0f, 1.5f);
            const auto ink = isEnabled() ? colours::text : colours::textDim.withAlpha (0.45f);
            if (arrow == 0)
            {
                g.setFont (look::font (14.0f));
                g.setColour (ink);
                g.drawText (label, r, juce::Justification::centred);
                return;
            }
            // A curved arrow, drawn for undo and mirrored for redo
            const auto c = r.getCentre();
            juce::Path arc, head;
            arc.addCentredArc (c.x, c.y + 3.0f, 7.0f, 6.0f, 0.0f, -1.9f, 1.9f, true);
            head.addTriangle (-4.5f, 0.0f, 2.5f, -4.0f, 2.5f, 4.0f);
            head.applyTransform (juce::AffineTransform::rotation (-0.35f).translated (c.x - 7.0f, c.y + 0.5f));
            if (arrow > 0)
            {
                const auto mirror = juce::AffineTransform::scale (-1.0f, 1.0f, c.x, c.y);
                arc.applyTransform (mirror);
                head.applyTransform (mirror);
            }
            g.setColour (ink);
            g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.fillPath (head);
        }

        void enablementChanged() override { repaint(); }

        void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            down = false;
            repaint();
            if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
        }

    private:
        juce::String label;
        int arrow = 0;
        bool down = false;
    };
}

namespace pc::gui
{
    // A chip you drag out of the plugin onto a DAW track; `makeFile` writes the MIDI file when the drag starts
    class DragChip : public juce::Component
    {
    public:
        std::function<juce::File()> makeFile;

        explicit DragChip (juce::String labelText) : label (std::move (labelText)) {}

        void setEnabledLook (bool on, const juce::String& detailText = {})
        {
            if (on == enabled && detailText == detail) return;
            enabled = on;
            detail = detailText;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (2.0f, 3.0f);
            if (enabled) look::glow (g, r, 8.0f, colours::notes[5], 0.6f);
            look::raised (g, r, 8.0f, enabled ? colours::panelLight : colours::panel, false);
            // Grip dots
            auto grip = r.removeFromLeft (18.0f).reduced (5.0f, 7.0f);
            g.setColour (enabled ? colours::notes[5] : colours::outline);
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 2; ++col)
                    g.fillEllipse (grip.getX() + (float) col * 5.0f, grip.getY() + (float) row * (grip.getHeight() / 2.5f), 2.5f, 2.5f);
            g.setColour (enabled ? colours::text : colours::textDim);
            g.setFont (look::font (13.5f));
            g.drawText (detail.isEmpty() ? label : label + "  " + detail, r.withTrimmedRight (6.0f), juce::Justification::centred, false);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! enabled || dragging || e.getDistanceFromDragStart() < 4 || ! makeFile) return;
            const auto file = makeFile();
            if (! file.existsAsFile()) return;
            dragging = true;
            juce::Component::SafePointer<DragChip> safe (this);
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                         [safe] { if (safe != nullptr) safe->dragging = false; });
        }

        void mouseUp (const juce::MouseEvent&) override { dragging = false; }

    private:
        juce::String label, detail;
        bool enabled = false, dragging = false;
    };
}

namespace pc::gui
{
    // A latching button with an LED, not tied to a parameter (e.g. Capture)
    class LedToggle : public juce::Component
    {
    public:
        std::function<void (bool)> onChange;

        LedToggle (juce::String labelText, juce::Colour ledColour) : label (std::move (labelText)), led (ledColour) {}

        void setOn (bool v) { if (v != on) { on = v; repaint(); } }
        bool isOn() const { return on; }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (2.0f, 3.0f);
            look::raised (g, r, 8.0f, colours::panelLight, on);
            if (on) r = r.translated (0.0f, 1.5f);
            look::led (g, { r.getX() + 15.0f, r.getCentreY() }, 5.0f, led, on);
            g.setFont (look::font (14.0f));
            g.setColour (on ? colours::text : colours::textDim);
            g.drawText (label, r.withTrimmedLeft (26.0f).withTrimmedRight (4.0f), juce::Justification::centred);
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            on = ! on;
            repaint();
            if (onChange) onChange (on);
        }

    private:
        juce::String label;
        juce::Colour led;
        bool on = false;
    };
}
