#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::Identifier versionProperty { "version" };
}

PotatoChordsProcessor::PotatoChordsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PotatoChords", pc::params::createLayout())
{
    apvts.state.setProperty (versionProperty, stateVersion, nullptr);
}

void PotatoChordsProcessor::prepareToPlay (double, int) {}

bool PotatoChordsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PotatoChordsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // Skeleton: incoming MIDI passes through unchanged; the audio bus only exists for a future preview sound
    buffer.clear();
}

juce::AudioProcessorEditor* PotatoChordsProcessor::createEditor()
{
    return new PotatoChordsEditor (*this);
}

void PotatoChordsProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (versionProperty, stateVersion, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PotatoChordsProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            // Migrations from older versions go here, keyed on state[versionProperty]
            state.setProperty (versionProperty, stateVersion, nullptr);
            apvts.replaceState (state);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PotatoChordsProcessor();
}
