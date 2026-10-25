#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Colours.h"

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
            const auto w = (float) getWidth() / (float) names.size();
            for (int i = 0; i < names.size(); ++i)
            {
                auto r = juce::Rectangle<float> (w * (float) i, 0.0f, w, (float) getHeight()).reduced (2.0f);
                const bool on = i == selected;
                g.setColour (on ? colours::accent : colours::panel);
                g.fillRoundedRectangle (r, 7.0f);
                g.setColour (on ? colours::text : colours::outline);
                g.drawRoundedRectangle (r, 7.0f, 1.0f);
                g.setColour (on ? colours::text : colours::textDim);
                g.setFont (juce::FontOptions (14.0f, on ? juce::Font::bold : juce::Font::plain));
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
            auto r = getLocalBounds().toFloat().reduced (2.0f);
            g.setColour (on ? colours::accent : colours::panel);
            g.fillRoundedRectangle (r, r.getHeight() / 2.0f);
            g.setColour (on ? colours::text : colours::outline);
            g.drawRoundedRectangle (r, r.getHeight() / 2.0f, 1.0f);
            auto knob = r.removeFromLeft (r.getHeight()).reduced (4.0f);
            g.setColour (on ? colours::text : colours::textDim);
            g.fillEllipse (knob);
            g.setFont (juce::FontOptions (14.0f, on ? juce::Font::bold : juce::Font::plain));
            g.drawText (label, r.withTrimmedRight (8.0f), juce::Justification::centred);
        }

        void mouseDown (const juce::MouseEvent&) override { attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

    private:
        juce::String label;
        bool on = false;
        juce::ParameterAttachment attachment;
    };
}
