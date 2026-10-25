#include "PluginEditor.h"

namespace colours = pc::gui::colours;
namespace look = pc::gui::look;

namespace
{
    juce::RangedAudioParameter& param (PotatoChordsProcessor& p, const juce::ParameterID& id)
    {
        return *p.apvts.getParameter (id.getParamID());
    }

    // Engraved caption: a dark shadow under dim text, like printing on a hardware panel
    void drawCaption (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r)
    {
        g.setFont (look::font (12.5f));
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawText (text.toUpperCase(), r.translated (0, 1), juce::Justification::centredLeft);
        g.setColour (colours::textDim);
        g.drawText (text.toUpperCase(), r, juce::Justification::centredLeft);
    }

    juce::String fromLabel (const pc::theory::Label& l) { return juce::String::fromUTF8 (l.text); }

    // Fixed layout rows, shared by paint() and resized()
    constexpr int width = 900, height = 664, margin = 26, topY = 22, topH = 42, flavourY = 78, rowH = 34,
                  padsY = 122, padsH = 226, fiveY = padsY + padsH + 2, soundY = 388, displayY = 432, displayH = 56,
                  keyboardY = 498, keyboardH = 148;
}

PotatoChordsEditor::PotatoChordsEditor (PotatoChordsProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      keyAttachment (p.apvts, pc::params::key.getParamID(), key),
      scale (param (p, pc::params::scale), pc::params::scaleNames),
      flavour (param (p, pc::params::flavour), pc::params::flavourNames),
      spread (param (p, pc::params::spread), pc::params::spreadNames),
      sound (param (p, pc::params::sound), pc::params::soundNames),
      chordKeys (param (p, pc::params::chordKeys), "Chord keys"),
      preview (param (p, pc::params::preview), "Preview"),
      majorFive (param (p, pc::params::majorFive), "Major V")
{
    setLookAndFeel (&look);

    key.addItemList (pc::params::keyNames, 1);
    // Attachment was made before the items existed, so resync the shown selection
    key.setSelectedItemIndex ((int) p.apvts.getRawParameterValue (pc::params::key.getParamID())->load(), juce::dontSendNotification);
    key.setTooltip ("Key");

    for (auto* c : std::initializer_list<juce::Component*> { &key, &scale, &flavour, &spread, &sound, &chordKeys, &preview,
                                                            &majorFive, &logo, &display, &keyboard })
        addAndMakeVisible (c);

    for (int i = 0; i < pc::theory::numDegrees; ++i)
    {
        pads[(size_t) i] = std::make_unique<pc::gui::ChordPad> (i);
        pads[(size_t) i]->onDown = [this] (int degree) { proc.padDown (degree); };
        pads[(size_t) i]->onUp = [this] (int degree) { proc.padUp (degree); };
        addAndMakeVisible (*pads[(size_t) i]);
    }

    setSize (width, height);
    refresh();
    startTimerHz (30);
}

PotatoChordsEditor::~PotatoChordsEditor()
{
    setLookAndFeel (nullptr);
}

void PotatoChordsEditor::timerCallback() { refresh(); }

void PotatoChordsEditor::refresh()
{
    const int held = proc.heldDegree();
    const bool flats = proc.keyUsesFlats();
    std::array<juce::String, pc::theory::numDegrees> numerals;
    for (int i = 0; i < pc::theory::numDegrees; ++i)
    {
        const auto chord = proc.chordFor (i);
        pads[(size_t) i]->setChord (chord, flats, held == i);
        numerals[(size_t) i] = fromLabel (pc::theory::romanNumeral (chord, i));
    }

    const bool minor = (int) proc.apvts.getRawParameterValue (pc::params::scale.getParamID())->load() == 1;
    const bool keysOn = proc.apvts.getRawParameterValue (pc::params::chordKeys.getParamID())->load() > 0.5f;
    if (minor != minorKey)
    {
        minorKey = minor;
        majorFive.setVisible (minor);
    }

    if (held >= 0)
    {
        const auto chord = proc.chordFor (held);
        display.update (fromLabel (pc::theory::chordName (chord, flats)), numerals[(size_t) held], chord.root,
                        proc.soundingNotes(), flats, keysOn);
    }
    else
    {
        display.update ({}, {}, 0, {}, flats, keysOn);
    }

    std::bitset<128> sounding;
    for (int note = 0; note < 128; ++note)
        sounding[(size_t) note] = proc.isNoteSounding (note);
    keyboard.update (sounding, flats, keysOn, PotatoChordsProcessor::chordKeysLow, numerals);

    logoGlow = held >= 0 ? juce::jmin (1.0f, logoGlow + 0.25f) : logoGlow * 0.85f;
    logo.setActivity (logoGlow);
}

void PotatoChordsEditor::paint (juce::Graphics& g)
{
    if (background.isValid()) g.drawImageAt (background, 0, 0);
    else                      g.fillAll (colours::background);

    // The device body
    const auto body = getLocalBounds().toFloat().reduced (10.0f, 10.0f);
    look::raised (g, body, 18.0f, colours::chassis);
    // Corner screws
    for (auto c : { body.getTopLeft() + juce::Point<float> (12, 12), body.getTopRight() + juce::Point<float> (-12, 12),
                    body.getBottomLeft() + juce::Point<float> (12, -12), body.getBottomRight() + juce::Point<float> (-12, -12) })
    {
        const auto r = juce::Rectangle<float> (7.0f, 7.0f).withCentre (c);
        g.setGradientFill (juce::ColourGradient (colours::panelLight.brighter (0.4f), r.getX(), r.getY(), colours::well, r.getRight(), r.getBottom(), false));
        g.fillEllipse (r);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawLine (r.getX() + 1.5f, r.getCentreY(), r.getRight() - 1.5f, r.getCentreY(), 1.0f);
    }

    // Title, one letter per note colour, embossed
    const juce::String title ("POTATO-CHORDS");
    g.setFont (look::font (25.0f));
    float x = 92.0f;
    for (int i = 0; i < title.length(); ++i)
    {
        const auto letter = title.substring (i, i + 1);
        const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), letter) + 1.0f;
        const juce::Rectangle<float> r (x, (float) topY, w, (float) topH);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawText (letter, r.translated (0.0f, 2.0f), juce::Justification::centredLeft, false);
        look::glowText (g, letter, r, letter == "-" ? colours::textDim : colours::forNote (i * 7), juce::Justification::centredLeft);
        x += w;
    }

    drawCaption (g, "Flavour", { margin, flavourY, 70, rowH });
    drawCaption (g, "Spread", { 618, flavourY, 64, rowH });
    drawCaption (g, "Sound", { margin, soundY, 70, rowH });

    // Grooves between sections
    for (int y : { flavourY - 7, padsY - 4, soundY - 6 })
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawHorizontalLine (y, (float) margin, (float) (getWidth() - margin));
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine (y + 1, (float) margin, (float) (getWidth() - margin));
    }
}

void PotatoChordsEditor::resized()
{
    background = look::makeBackground (getWidth(), getHeight());

    const int right = getWidth() - margin;
    logo.setBounds (margin - 4, topY - 6, 66, topH + 12);
    auto top = juce::Rectangle<int> (318, topY, right - 318, topH);
    key.setBounds (top.removeFromLeft (76));
    top.removeFromLeft (8);
    scale.setBounds (top.removeFromLeft (170));
    preview.setBounds (top.removeFromRight (122));
    top.removeFromRight (8);
    chordKeys.setBounds (top.removeFromRight (148));

    flavour.setBounds (margin + 74, flavourY, 500, rowH);
    spread.setBounds (686, flavourY, right - 686, rowH);

    const int padW = (getWidth() - 2 * margin) / pc::theory::numDegrees;
    for (int i = 0; i < pc::theory::numDegrees; ++i)
        pads[(size_t) i]->setBounds (margin + i * padW, padsY, padW, padsH);
    majorFive.setBounds (margin + 4 * padW + 10, fiveY, padW - 20, 30);

    sound.setBounds (margin + 74, soundY, right - margin - 74, rowH);
    display.setBounds (margin, displayY, right - margin, displayH);
    keyboard.setBounds (margin, keyboardY, right - margin, keyboardH);
}
