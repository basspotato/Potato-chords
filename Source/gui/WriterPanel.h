#pragma once
#include "Controls.h"
#include "../Writer.h"

// The Write panel: pick a style and a mood, then Write a whole progression or Complete the empty bars
namespace pc::gui
{
    class WriterPanel : public juce::Component
    {
    public:
        std::function<void (writer::Style, writer::Mood)> onWrite, onComplete;
        std::function<void()> onClose;

        WriterPanel()
        {
            for (auto* c : std::initializer_list<juce::Component*> { &style, &mood, &write, &complete, &close })
                addAndMakeVisible (c);
            write.onClick = [this] { if (onWrite) onWrite (getStyle(), getMood()); };
            complete.onClick = [this] { if (onComplete) onComplete (getStyle(), getMood()); };
            close.onClick = [this] { if (onClose) onClose(); };
            style.onChange = mood.onChange = [this] (int) { if (onChoice) onChoice (style.getSelected(), mood.getSelected()); };
            write.setTooltip ("Writes a new progression for this style and mood: bars 5-8 are the chorus loop, bars 1-4 the "
                              "verse. Press again for another take. Also sets the scale and rhythm to suit the style.");
            complete.setTooltip ("Keeps your chords and fills only the empty bars with a loop that fits them.");
        }

        writer::Style getStyle() const { return (writer::Style) style.getSelected(); }
        writer::Mood getMood() const { return (writer::Mood) mood.getSelected(); }
        void setChoice (int s, int m) { style.setSelected (s); mood.setSelected (m); }
        std::function<void (int, int)> onChoice;  // style, mood

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (4.0f);
            look::glow (g, r, 12.0f, colours::notes[5], 0.5f);
            look::raised (g, r, 12.0f, colours::panel, false);
            g.setFont (look::font (14.0f));
            g.setColour (colours::text);
            g.drawText ("WRITE A PROGRESSION", r.reduced (16.0f, 10.0f).withHeight (18.0f), juce::Justification::centredLeft);
            g.setFont (look::font (12.0f, false));
            g.setColour (colours::textDim);
            g.drawText ("Style", labelArea (style), juce::Justification::centredLeft);
            g.drawText ("Mood", labelArea (mood), juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (20, 14);
            close.setBounds (r.getRight() - 30, r.getY() - 4, 34, 28);
            r.removeFromTop (26);
            style.setBounds (r.removeFromTop (30).withTrimmedLeft (54));
            r.removeFromTop (6);
            mood.setBounds (r.removeFromTop (30).withTrimmedLeft (54));
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

        static juce::StringArray names (const char* const* list, int n)
        {
            juce::StringArray a;
            for (int i = 0; i < n; ++i) a.add (list[i]);
            return a;
        }

        TabStrip style { names (writer::styleNames.data(), writer::numStyles) },
                 mood { names (writer::moodNames.data(), writer::numMoods) };
        ActionButton write { "Write" }, complete { "Complete empty bars" }, close { "X" };
    };
}
