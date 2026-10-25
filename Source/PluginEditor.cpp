#include "PluginEditor.h"

namespace colours = pc::gui::colours;

namespace
{
    juce::RangedAudioParameter& param (PotatoChordsProcessor& p, const juce::ParameterID& id)
    {
        return *p.apvts.getParameter (id.getParamID());
    }

    void drawCaption (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r)
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (text.toUpperCase(), r, juce::Justification::centredLeft);
    }

    // Fixed layout rows, shared by paint() and resized()
    constexpr int margin = 16, topY = 14, topH = 34, flavourY = 64, rowH = 32, padsY = 108, padsH = 236,
                  fiveY = padsY + padsH + 4, soundY = 392, readoutY = 438;
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
    look.setColourScheme ({ colours::panel, colours::background, colours::panel, colours::outline, colours::text,
                            colours::panelLight, colours::text, colours::accent, colours::text });
    setLookAndFeel (&look);

    key.addItemList (pc::params::keyNames, 1);
    // Attachment was made before the items existed, so resync the shown selection
    key.setSelectedItemIndex ((int) p.apvts.getRawParameterValue (pc::params::key.getParamID())->load(), juce::dontSendNotification);
    key.setTooltip ("Key");

    for (auto* c : std::initializer_list<juce::Component*> { &key, &scale, &flavour, &spread, &sound, &chordKeys, &preview, &majorFive })
        addAndMakeVisible (c);

    for (int i = 0; i < pc::theory::numDegrees; ++i)
    {
        pads[(size_t) i] = std::make_unique<pc::gui::ChordPad> (i);
        pads[(size_t) i]->onDown = [this] (int degree) { proc.padDown (degree); };
        pads[(size_t) i]->onUp = [this] (int degree) { proc.padUp (degree); };
        addAndMakeVisible (*pads[(size_t) i]);
    }

    setSize (900, 500);
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
    for (int i = 0; i < pc::theory::numDegrees; ++i)
        pads[(size_t) i]->setChord (proc.chordFor (i), flats, held == i);

    const bool minor = (int) proc.apvts.getRawParameterValue (pc::params::scale.getParamID())->load() == 1;
    const bool keysOn = proc.apvts.getRawParameterValue (pc::params::chordKeys.getParamID())->load() > 0.5f;
    if (minor != minorKey) majorFive.setVisible (minor);

    const auto notes = proc.soundingNotes();
    const auto name = held >= 0 ? juce::String::fromUTF8 (pc::theory::chordName (proc.chordFor (held), flats).text) : juce::String();
    if (minor != minorKey || keysOn != chordKeysOn || name != nowPlaying || notes.size != shownNotes.size || notes.notes != shownNotes.notes)
    {
        minorKey = minor;
        chordKeysOn = keysOn;
        nowPlaying = name;
        shownNotes = notes;
        repaint (0, soundY - 30, getWidth(), getHeight() - soundY + 30);
    }
}

void PotatoChordsEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    auto area = getLocalBounds().reduced (margin, 0);

    // Title in the note colours, one letter per colour
    const juce::String title ("POTATO-CHORDS");
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    float x = (float) margin;
    for (int i = 0; i < title.length(); ++i)
    {
        const auto letter = title.substring (i, i + 1);
        g.setColour (letter == "-" ? colours::textDim : colours::forNote (i * 7));
        const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), letter);
        g.drawText (letter, juce::Rectangle<float> (x, (float) topY, w + 1.0f, (float) topH), juce::Justification::centredLeft);
        x += w + 1.0f;
    }

    drawCaption (g, "Flavour", { margin, flavourY, 70, rowH });
    drawCaption (g, "Spread", { 600, flavourY, 64, rowH });
    drawCaption (g, "Sound", { margin, soundY, 70, rowH });

    // Now playing: chord name and its notes in their colours
    auto readout = area.withY (readoutY).withHeight (44);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (readout.toFloat(), 10.0f);
    auto inner = readout.reduced (14, 6);
    if (nowPlaying.isEmpty())
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText (chordKeysOn ? "Chord keys on: the white keys C3 to B3 (MIDI 60-71) play I to vii. Or tap a pad."
                                : "Tap a pad to hear its chord.",
                    inner, juce::Justification::centredLeft);
        return;
    }

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText (nowPlaying, inner.removeFromLeft (140), juce::Justification::centredLeft);
    const bool flats = proc.keyUsesFlats();
    for (int i = 0; i < shownNotes.size; ++i)
    {
        const int note = shownNotes.notes[(size_t) i];
        auto chip = inner.removeFromLeft (52).reduced (3, 2).toFloat();
        g.setColour (colours::forNote (note));
        g.fillRoundedRectangle (chip, 8.0f);
        g.setColour (colours::onNote);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        // MIDI octave numbering as Cubase shows it (middle C = C3)
        g.drawText (juce::String (pc::theory::noteName (note, flats)) + juce::String (note / 12 - 2), chip, juce::Justification::centred);
    }
}

void PotatoChordsEditor::resized()
{
    const int right = getWidth() - margin;
    auto top = juce::Rectangle<int> (230, topY, right - 230, topH);
    key.setBounds (top.removeFromLeft (74));
    top.removeFromLeft (8);
    scale.setBounds (top.removeFromLeft (170));
    preview.setBounds (top.removeFromRight (120));
    top.removeFromRight (8);
    chordKeys.setBounds (top.removeFromRight (150));

    flavour.setBounds (margin + 74, flavourY, 480, rowH);
    spread.setBounds (668, flavourY, right - 668, rowH);

    const int padW = (getWidth() - 2 * margin) / pc::theory::numDegrees;
    for (int i = 0; i < pc::theory::numDegrees; ++i)
        pads[(size_t) i]->setBounds (margin + i * padW, padsY, padW, padsH);
    majorFive.setBounds (margin + 4 * padW + 8, fiveY, padW - 16, 28);

    sound.setBounds (margin + 74, soundY, right - margin - 74, rowH);
}
