// Renders the plugin editor to a PNG without a host, so GUI work can be checked headlessly.
// Usage: PotatoChordsSnapshot out.png [width] [pad] [preset]
//   width 0 = default; pad 1-7 holds that chord pad (0 = none); preset 1-8 fills the progression lane;
//   a 5th argument "melody" adds a short demo melody, "song" shows the Song view
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png"));

    std::unique_ptr<juce::AudioProcessor> processor (createPluginFilter());
    if (argc > 3)
    {
        auto& proc = dynamic_cast<PotatoChordsProcessor&> (*processor);
        proc.setPlayConfigDetails (0, 2, 48000.0, 256);
        proc.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        proc.processBlock (buffer, midi);
        if (argc > 4)
            if (const int preset = juce::String (argv[4]).getIntValue(); preset >= 1 && preset <= (int) pc::presets::progressions.size())
                proc.lane.set (pc::presets::fill (pc::presets::progressions[(size_t) preset - 1], pc::theory::Flavour::triad));
        if (argc > 5 && juce::String (argv[5]) == "melody")
            for (int bar = 0; bar < 8; ++bar)
                for (auto [step, length, row] : { std::tuple<int, int, int> { 0, 4, 7 }, { 4, 2, 9 }, { 6, 2, 8 }, { 8, 6, 7 }, { 14, 2, 4 } })
                    proc.melody.addNote (bar * 16 + step, length, (row + bar % 4) % 14);
        if (const int pad = juce::String (argv[3]).getIntValue(); pad >= 1)
            proc.padDown (pad - 1);
        midi.clear();
        proc.processBlock (buffer, midi);
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorAndMakeActive());
    if (argc > 5 && juce::String (argv[5]) == "song")
        if (auto* e = dynamic_cast<PotatoChordsEditor*> (editor.get())) e->setSongView (true);
    if (argc > 2 && juce::String (argv[2]).getIntValue() > 0)
    {
        const int w = juce::String (argv[2]).getIntValue();
        editor->setSize (w, w * editor->getHeight() / editor->getWidth());
    }

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    editor.reset();

    juce::FileOutputStream stream (out);
    if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
        return 1;
    return 0;
}
