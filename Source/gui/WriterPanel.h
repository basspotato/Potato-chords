#pragma once
#include "Controls.h"

// A Write panel: two choice rows (style and mood, or shape and busyness), a Write button and an optional
// Complete button. Used for writing a progression and for writing a melody.
namespace pc::gui
{
    class WriterPanel : public juce::Component
    {
    public:
        std::function<void (int, int)> onWrite, onComplete, onChoice;  // the two choices
        std::function<void()> onClose;

        WriterPanel (juce::String titleText, juce::String firstLabel, juce::StringArray firstNames, juce::String secondLabel,
                     juce::StringArray secondNames, juce::String completeText = {})
            : title (std::move (titleText)), labelA (std::move (firstLabel)), labelB (std::move (secondLabel)),
              first (std::move (firstNames)), second (std::move (secondNames)), complete (completeText)
        {
            for (auto* c : std::initializer_list<juce::Component*> { &first, &second, &write, &close })
                addAndMakeVisible (c);
            addChildComponent (complete);
            complete.setVisible (completeText.isNotEmpty());
            write.onClick = [this] { if (onWrite) onWrite (first.getSelected(), second.getSelected()); };
            complete.onClick = [this] { if (onComplete) onComplete (first.getSelected(), second.getSelected()); };
            close.onClick = [this] { if (onClose) onClose(); };
            first.onChange = second.onChange = [this] (int) { if (onChoice) onChoice (first.getSelected(), second.getSelected()); };
        }

        void setChoice (int a, int b) { first.setSelected (a); second.setSelected (b); }
        void setTips (const juce::String& writeTip, const juce::String& completeTip)
        {
            write.setTooltip (writeTip);
            complete.setTooltip (completeTip);
        }

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (4.0f);
            look::glow (g, r, 12.0f, colours::notes[5], 0.5f);
            look::raised (g, r, 12.0f, colours::panel, false);
            g.setFont (look::font (14.0f));
            g.setColour (colours::text);
            g.drawText (title, r.reduced (16.0f, 10.0f).withHeight (18.0f), juce::Justification::centredLeft);
            g.setFont (look::font (12.0f, false));
            g.setColour (colours::textDim);
            g.drawText (labelA, labelArea (first), juce::Justification::centredLeft);
            g.drawText (labelB, labelArea (second), juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (20, 14);
            close.setBounds (r.getRight() - 30, r.getY() - 4, 34, 28);
            r.removeFromTop (26);
            first.setBounds (r.removeFromTop (30).withTrimmedLeft (54));
            r.removeFromTop (6);
            second.setBounds (r.removeFromTop (30).withTrimmedLeft (54));
            r.removeFromTop (10);
            auto buttons = r.removeFromTop (32);
            write.setBounds (buttons.removeFromRight (120));
            buttons.removeFromRight (8);
            complete.setBounds (buttons.removeFromRight (190));
        }

    private:
        juce::Rectangle<float> labelArea (const juce::Component& c) const
        {
            return juce::Rectangle<float> ((float) c.getX() - 54.0f, (float) c.getY(), 50.0f, (float) c.getHeight());
        }

        juce::String title, labelA, labelB;
        TabStrip first, second;
        ActionButton write { "Write" }, complete, close { "X" };
    };

    // Names from a list of C strings, for the choice rows
    template <size_t N>
    juce::StringArray namesOf (const std::array<const char*, N>& list)
    {
        juce::StringArray a;
        for (auto* n : list) a.add (n);
        return a;
    }
}
