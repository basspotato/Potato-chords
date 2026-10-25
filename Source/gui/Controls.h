#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Look.h"

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
}
