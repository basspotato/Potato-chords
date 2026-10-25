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

        void paint (juce::Graphics& g) override
        {
            look::inset (g, getLocalBounds().toFloat(), 9.0f);
            const auto w = (float) getWidth() / (float) names.size();
            for (int i = 0; i < names.size(); ++i)
            {
                auto r = juce::Rectangle<float> (w * (float) i, 0.0f, w, (float) getHeight()).reduced (3.0f, 3.5f);
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
            const int i = juce::jlimit (0, names.size() - 1, (int) ((float) e.x / (float) getWidth() * (float) names.size()));
            attachment.setValueAsCompleteGesture ((float) i);
        }

    private:
        juce::StringArray names;
        int selected = 0;
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

    // A push button in the same raised style
    class ActionButton : public juce::Component
    {
    public:
        std::function<void()> onClick;

        explicit ActionButton (juce::String labelText) : label (std::move (labelText)) {}

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (2.0f, 3.0f);
            look::raised (g, r, 8.0f, colours::panelLight, down);
            if (down) r = r.translated (0.0f, 1.5f);
            g.setFont (look::font (14.0f));
            g.setColour (colours::text);
            g.drawText (label, r, juce::Justification::centred);
        }

        void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            down = false;
            repaint();
            if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
        }

    private:
        juce::String label;
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
