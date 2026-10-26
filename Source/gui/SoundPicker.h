#pragma once
#include "Look.h"
#include "../PreviewSynth.h"
#include <functional>

// Picks one of the preview synth's sounds: arrows step through them, a click opens a menu with a sub-menu per group.
// The group's colour marks the sound, so you can tell a pad from a pluck at a glance.
namespace pc::gui
{
    class SoundPicker : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit SoundPicker (juce::RangedAudioParameter& p)
            : attachment (p, [this] (float v) { selected = juce::jlimit (0, synth::numPresets - 1, (int) v); repaint(); })
        {
            attachment.sendInitialUpdate();
        }

        void paint (juce::Graphics& g) override
        {
            auto area = getLocalBounds().toFloat();
            look::inset (g, area, 9.0f);
            const auto& preset = synth::presets[(size_t) selected];
            const auto tint = groupColour (selected);
            const float arrowW = juce::jmin (18.0f, area.getWidth() * 0.14f);

            auto left = area.removeFromLeft (arrowW), right = area.removeFromRight (arrowW);
            drawArrow (g, left, -1, hover == 1);
            drawArrow (g, right, 1, hover == 2);

            auto mid = area.reduced (2.0f, 4.0f);
            const auto chip = mid.removeFromLeft (4.0f).reduced (0.0f, 3.0f);
            look::glow (g, chip, 2.0f, tint, 0.9f);
            g.setColour (tint);
            g.fillRoundedRectangle (chip, 2.0f);
            mid.removeFromLeft (6.0f);

            g.setFont (look::font (10.5f));
            g.setColour (tint.withAlpha (0.9f));
            g.drawText (juce::String (preset.group).toUpperCase(), mid.removeFromTop (mid.getHeight() * 0.38f),
                        juce::Justification::bottomLeft, true);
            g.setFont (look::font (14.0f));
            g.setColour (colours::text);
            g.drawText (preset.name, mid, juce::Justification::topLeft, true);
        }

        void mouseMove (const juce::MouseEvent& e) override { setHover (zoneAt (e.x)); }
        void mouseExit (const juce::MouseEvent&) override { setHover (0); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! isEnabled()) return;
            const int zone = zoneAt (e.x);
            if (zone == 1) step (-1);
            else if (zone == 2) step (1);
            else showMenu();
        }

        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            if (isEnabled() && w.deltaY != 0.0f) step (w.deltaY > 0.0f ? -1 : 1);
        }

    private:
        static juce::Colour groupColour (int index)
        {
            return colours::notes[(size_t) (index / synth::presetsPerGroup * 5 + 1) % colours::notes.size()];  // 5 apart: no two groups share a colour
        }

        void drawArrow (juce::Graphics& g, juce::Rectangle<float> r, int direction, bool hot) const
        {
            const auto c = r.getCentre();
            juce::Path p;
            const float s = 4.5f;
            p.addTriangle (c.x - direction * s * 0.6f, c.y - s, c.x - direction * s * 0.6f, c.y + s, c.x + direction * s * 0.8f, c.y);
            g.setColour (isEnabled() ? (hot ? colours::accent : colours::textDim) : colours::textDim.withAlpha (0.4f));
            g.fillPath (p);
        }

        int zoneAt (int x) const
        {
            const int arrowW = (int) juce::jmin (18.0f, (float) getWidth() * 0.14f);
            return x < arrowW ? 1 : x >= getWidth() - arrowW ? 2 : 0;
        }

        void setHover (int zone)
        {
            if (zone != hover) { hover = zone; repaint(); }
        }

        void step (int by)
        {
            const int next = (selected + by + synth::numPresets) % synth::numPresets;
            choose (next);
        }

        void choose (int index)
        {
            attachment.setValueAsCompleteGesture ((float) index);
        }

        void showMenu()
        {
            juce::PopupMenu menu;
            juce::PopupMenu group;
            juce::String groupName;
            auto flush = [&]
            {
                if (groupName.isNotEmpty()) menu.addSubMenu (groupName, group, true, nullptr, false, 0);
                group = juce::PopupMenu();
            };
            for (int i = 0; i < synth::numPresets; ++i)
            {
                const auto& preset = synth::presets[(size_t) i];
                if (groupName != preset.group)
                {
                    flush();
                    groupName = preset.group;
                }
                group.addItem (i + 1, preset.name, true, i == selected);
            }
            flush();

            juce::Component::SafePointer<SoundPicker> safe (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                                [safe] (int result)
                                {
                                    if (safe != nullptr && result > 0) safe->choose (result - 1);
                                });
        }

        int selected = 0, hover = 0;
        juce::ParameterAttachment attachment;
    };
}
