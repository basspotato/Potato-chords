#include "PluginEditor.h"

PotatoChordsEditor::PotatoChordsEditor (PotatoChordsProcessor& p)
    : AudioProcessorEditor (p),
      keyAttachment (p.apvts, pc::params::key.getParamID(), key),
      scaleAttachment (p.apvts, pc::params::scale.getParamID(), scale)
{
    key.addItemList (pc::params::keyNames, 1);
    scale.addItemList (pc::params::scaleNames, 1);
    // Attachments were made before the items existed, so resync the shown selection
    key.setSelectedItemIndex ((int) p.apvts.getRawParameterValue (pc::params::key.getParamID())->load(), juce::dontSendNotification);
    scale.setSelectedItemIndex ((int) p.apvts.getRawParameterValue (pc::params::scale.getParamID())->load(), juce::dontSendNotification);
    addAndMakeVisible (key);
    addAndMakeVisible (scale);
    setSize (720, 420);
}

void PotatoChordsEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15122b));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("POTATO-CHORDS", getLocalBounds().removeFromTop (60).reduced (16, 0), juce::Justification::centredLeft);
}

void PotatoChordsEditor::resized()
{
    auto row = getLocalBounds().reduced (16).withTrimmedTop (52).removeFromTop (28);
    key.setBounds (row.removeFromLeft (90));
    row.removeFromLeft (8);
    scale.setBounds (row.removeFromLeft (120));
}
