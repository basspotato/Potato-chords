// Renders the plugin editor to a PNG without a host, so GUI work can be checked headlessly.
// Usage: PotatoChordsSnapshot out.png [width] [pad]   (pad 1-7 holds that chord pad, to see the lit state)
#include "PluginProcessor.h"

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
        proc.padDown (juce::String (argv[3]).getIntValue() - 1);
        midi.clear();
        proc.processBlock (buffer, midi);
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorAndMakeActive());
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
