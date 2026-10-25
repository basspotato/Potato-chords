#include "PluginEditor.h"
#include "Presets.h"

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

    void setParam (PotatoChordsProcessor& p, const juce::ParameterID& id, float value)
    {
        auto& prm = param (p, id);
        prm.beginChangeGesture();
        prm.setValueNotifyingHost (prm.convertTo0to1 (value));
        prm.endChangeGesture();
    }

    // Fixed layout rows, shared by paint() and resized()
    constexpr int width = 1000, height = 790, margin = 26, topY = 22, topH = 42, flavourY = 78, rowH = 34,
                  padsY = 122, padsH = 226, fiveY = padsY + padsH + 2, laneHeadY = 390, laneY = 426, laneH = 96,
                  displayY = 534, displayH = 56, keyboardY = 600, keyboardH = 160;
}

PotatoChordsEditor::PotatoChordsEditor (PotatoChordsProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      keyAttachment (p.apvts, pc::params::key.getParamID(), key),
      soundAttachment (p.apvts, pc::params::sound.getParamID(), sound),
      rhythmAttachment (p.apvts, pc::params::rhythm.getParamID(), rhythm),
      scale (param (p, pc::params::scale), pc::params::scaleNames),
      flavour (param (p, pc::params::flavour), pc::params::flavourNames),
      spread (param (p, pc::params::spread), pc::params::spreadNames),
      chordKeys (param (p, pc::params::chordKeys), "Chord keys"),
      preview (param (p, pc::params::preview), "Preview"),
      majorFive (param (p, pc::params::majorFive), "Major V"),
      playLane (param (p, pc::params::playLane), "Play with host"),
      learn (param (p, pc::params::learn), "Learn")
{
    setLookAndFeel (&look);

    key.addItemList (pc::params::keyNames, 1);
    // Attachments were made before the items existed, so resync the shown selection
    key.setSelectedItemIndex ((int) p.apvts.getRawParameterValue (pc::params::key.getParamID())->load(), juce::dontSendNotification);

    // Sound presets grouped by type under headings
    const char* group = "";
    for (int i = 0; i < pc::synth::numPresets; ++i)
    {
        const auto& preset = pc::synth::presets[(size_t) i];
        if (juce::String (preset.group) != group)
        {
            group = preset.group;
            sound.addSectionHeading (group);
        }
        sound.addItem (preset.name, i + 1);
    }
    sound.setSelectedItemIndex ((int) p.apvts.getRawParameterValue (pc::params::sound.getParamID())->load(), juce::dontSendNotification);

    for (int i = 0; i < pc::params::rhythmNames.size(); ++i)
        rhythm.addItem ("Rhythm: " + pc::params::rhythmNames[i], i + 1);
    rhythm.setSelectedItemIndex ((int) p.apvts.getRawParameterValue (pc::params::rhythm.getParamID())->load(), juce::dontSendNotification);

    presets.setTextWhenNothingSelected ("Starter progressions");
    for (int i = 0; i < (int) pc::presets::progressions.size(); ++i)
    {
        const auto& pr = pc::presets::progressions[(size_t) i];
        if (i == 0) presets.addSectionHeading ("Major");
        if (i == 4) presets.addSectionHeading ("Minor");
        presets.addItem (juce::String (pr.name) + "   " + pr.nickname, i + 1);
    }
    presets.onChange = [this]
    {
        if (const int id = presets.getSelectedId(); id > 0) loadPreset (id - 1);
        presets.setSelectedId (0, juce::dontSendNotification);
    };

    clearLane.onClick = [this] { proc.lane.clear(); selectedSlot = 0; };

    lane.onSelect = [this] (int slot) { selectedSlot = slot; refresh(); };
    lane.onSplit = [this] (int bar, bool split)
    {
        proc.lane.setSplit (bar, split);
        if (! split && selectedSlot == bar * 2 + 1) selectedSlot = bar * 2;
        refresh();
    };
    lane.onClear = [this] (int slot) { proc.lane.setSlot (slot, {}); refresh(); };

    dragLane.makeFile = [this] { return laneFile(); };
    exportMidi.onClick = [this] { exportLane(); };
    dragTake.makeFile = [this]
    {
        return pc::midiexport::writeTemp (take.toMidiFile (proc.hostBpm(), proc.barQuarters()), "Potato-Chords capture");
    };
    capture.onChange = [this] (bool on)
    {
        if (on)
        {
            proc.drainCapture ([] (const auto&) {});  // drop anything left from before
            take.clear();
        }
        proc.setCapturing (on);
    };

    for (auto* c : std::initializer_list<juce::Component*> { &key, &scale, &flavour, &spread, &sound, &chordKeys, &preview, &majorFive,
                                                            &playLane, &learn, &rhythm, &exportMidi, &presets, &clearLane, &dragLane, &dragTake, &capture,
                                                            &logo, &display, &lane, &keyboard })
        addAndMakeVisible (c);

    for (int i = 0; i < pc::theory::numDegrees; ++i)
    {
        pads[(size_t) i] = std::make_unique<pc::gui::ChordPad> (i);
        pads[(size_t) i]->onDown = [this] (int degree) { padPressed (degree); };
        pads[(size_t) i]->onUp = [this] (int degree) { proc.padUp (degree); };
        addAndMakeVisible (*pads[(size_t) i]);
    }

    setSize (width, height);
    refresh();
    startTimerHz (30);
}

PotatoChordsEditor::~PotatoChordsEditor()
{
    proc.setCapturing (false);
    setLookAndFeel (nullptr);
}

// A pad plays its chord; with a lane slot selected it also writes the chord there and moves to the next slot
void PotatoChordsEditor::padPressed (int degree)
{
    proc.padDown (degree);
    lastDegree = degree;
    if (selectedSlot < 0) return;

    pc::progression::Slot slot;
    slot.degree = degree;
    slot.flavour = (pc::theory::Flavour) (int) proc.apvts.getRawParameterValue (pc::params::flavour.getParamID())->load();
    slot.majorFive = proc.apvts.getRawParameterValue (pc::params::majorFive.getParamID())->load() > 0.5f;
    proc.lane.setSlot (selectedSlot, slot);

    int next = selectedSlot + 1;
    if (next < pc::progression::numSlots && next % 2 == 1 && ! proc.lane.isSplit (next / 2)) ++next;
    selectedSlot = next < pc::progression::numSlots ? next : -1;
    refresh();
}

juce::File PotatoChordsEditor::laneFile()
{
    auto value = [this] (const juce::ParameterID& id) { return (int) proc.apvts.getRawParameterValue (id.getParamID())->load(); };
    return pc::midiexport::writeTemp (
        pc::midiexport::laneToMidi (proc.lane.snapshot(), value (pc::params::key), (pc::theory::Scale) value (pc::params::scale),
                                    (pc::voicing::Spread) value (pc::params::spread), proc.hostBpm(), proc.barQuarters(),
                                    value (pc::params::rhythm)),
        "Potato-Chords progression");
}

// Saves the lane as a .mid file wherever the user picks
void PotatoChordsEditor::exportLane()
{
    const auto source = laneFile();
    if (! source.existsAsFile()) return;
    chooser = std::make_unique<juce::FileChooser> ("Export progression as MIDI",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                       .getChildFile ("Potato-Chords progression.mid"),
                                                   "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [source] (const juce::FileChooser& fc)
                          {
                              auto target = fc.getResult();
                              if (target == juce::File()) return;
                              if (! target.hasFileExtension ("mid")) target = target.withFileExtension ("mid");
                              source.copyFileTo (target);
                          });
}

void PotatoChordsEditor::loadPreset (int index)
{
    const auto& pr = pc::presets::progressions[(size_t) index];
    setParam (proc, pc::params::scale, pr.scale == pc::theory::Scale::major ? 0.0f : 1.0f);
    proc.lane.set (pc::presets::fill (pr, (pc::theory::Flavour) (int) proc.apvts.getRawParameterValue (pc::params::flavour.getParamID())->load()));
    selectedSlot = -1;
    refresh();
}

// The chord the "next" hints follow: the last filled slot before the selected one, else the last chord played
int PotatoChordsEditor::hintSource() const
{
    if (selectedSlot >= 0)
    {
        for (int i = selectedSlot - 1; i >= 0; --i)
            if (i % 2 == 0 || proc.lane.isSplit (i / 2))
                if (const auto s = proc.lane.slot (i); ! s.empty()) return s.degree;
        return -1;
    }
    if (proc.heldDegree() >= 0) return proc.heldDegree();
    if (const int playing = proc.playingSlot(); playing >= 0)
        if (const auto s = proc.lane.snapshot().chordIn (playing); ! s.empty()) return s.degree;
    return lastDegree;
}

juce::String PotatoChordsEditor::statusText() const
{
    if (! proc.hostIsPlaying()) return "STOPPED";
    if (proc.playingSlot() < 0) return "PLAYING  (lane off)";
    return "PLAYING  BAR " + juce::String (proc.playingSlot() / 2 + 1) + " / 8";
}

void PotatoChordsEditor::timerCallback() { refresh(); }

void PotatoChordsEditor::refresh()
{
    const int held = proc.heldDegree();
    const bool flats = proc.keyUsesFlats();
    const bool minor = (int) proc.apvts.getRawParameterValue (pc::params::scale.getParamID())->load() == 1;
    const bool keysOn = proc.apvts.getRawParameterValue (pc::params::chordKeys.getParamID())->load() > 0.5f;
    const auto scaleNow = minor ? pc::theory::Scale::minor : pc::theory::Scale::major;
    const bool learning = proc.apvts.getRawParameterValue (pc::params::learn.getParamID())->load() > 0.5f;
    const auto hints = learning ? pc::presets::nextChordHints (scaleNow, hintSource()) : uint8_t (0);
    const bool majorFiveOn = proc.apvts.getRawParameterValue (pc::params::majorFive.getParamID())->load() > 0.5f;

    std::array<juce::String, pc::theory::numDegrees> numerals;
    for (int i = 0; i < pc::theory::numDegrees; ++i)
    {
        const auto chord = proc.chordFor (i);
        pads[(size_t) i]->setChord (chord, flats, held == i);
        pads[(size_t) i]->setHint (((hints >> i) & 1) != 0);
        pads[(size_t) i]->setMood (learning ? juce::String (pc::presets::moodWord (scaleNow, i, majorFiveOn)) : juce::String());
        numerals[(size_t) i] = fromLabel (pc::theory::romanNumeral (chord, i));
    }

    if (minor != minorKey)
    {
        minorKey = minor;
        majorFive.setVisible (minor);
    }

    // Lane
    const auto snap = proc.lane.snapshot();
    std::array<pc::gui::SlotView, pc::progression::numSlots> views;
    for (int i = 0; i < pc::progression::numSlots; ++i)
    {
        const auto slot = snap.slot (i);
        auto& v = views[(size_t) i];
        if (slot.empty()) continue;
        const auto chord = proc.chordFor (slot);
        v.empty = false;
        v.numeral = fromLabel (pc::theory::romanNumeral (chord, slot.degree));
        v.name = fromLabel (pc::theory::chordName (chord, flats));
        v.root = chord.root;
        v.size = chord.size;
        for (int k = 0; k < chord.size; ++k)
            v.pitchClasses[(size_t) k] = (chord.root + chord.intervals[(size_t) k]) % 12;
    }
    lane.update (views, snap.splits, proc.playingSlot(), selectedSlot);

    bool laneHasChords = false;
    for (auto& v : views) laneHasChords = laneHasChords || ! v.empty;
    dragLane.setEnabledLook (laneHasChords);

    // Display: the live chord, else the lane's, else a hint for what to do
    juce::String idle = keysOn ? "Chord keys on: the white keys C3 to B3 play I to vii. Or tap a pad."
                               : "Tap a pad to hear its chord.";
    if (selectedSlot >= 0)
        idle = "Bar " + juce::String (selectedSlot / 2 + 1) + (snap.isSplit (selectedSlot / 2) ? (selectedSlot % 2 == 0 ? " (first half)" : " (second half)") : "")
             + " selected: tap a pad to put a chord there. Pads marked NEXT fit well.";

    std::optional<pc::theory::Chord> shown;
    juce::String shownNumeral;
    if (held >= 0)
    {
        shown = proc.chordFor (held);
        shownNumeral = numerals[(size_t) held];
    }
    else if (const int playing = proc.playingSlot(); playing >= 0 && ! snap.chordIn (playing).empty())
    {
        const auto slot = snap.chordIn (playing);
        shown = proc.chordFor (slot);
        shownNumeral = fromLabel (pc::theory::romanNumeral (*shown, slot.degree));
    }
    const auto status = statusText();
    if (shown) display.update (fromLabel (pc::theory::chordName (*shown, flats)), shownNumeral, shown->root, proc.soundingNotes(), flats, idle, status);
    else       display.update ({}, {}, 0, {}, flats, idle, status);

    std::bitset<128> sounding;
    for (int note = 0; note < 128; ++note)
        sounding[(size_t) note] = proc.isNoteSounding (note);
    keyboard.update (sounding, flats, keysOn, PotatoChordsProcessor::chordKeysLow, numerals);

    // Capture
    proc.drainCapture ([this] (const pc::midiexport::CaptureEvent& e) { take.add (e); });
    const int notes = take.numNotes();
    dragTake.setEnabledLook (notes > 0, notes > 0 ? "(" + juce::String (notes) + " notes)" : juce::String());

    logoGlow = (held >= 0 || proc.playingSlot() >= 0) ? juce::jmin (1.0f, logoGlow + 0.25f) : logoGlow * 0.85f;
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
    drawCaption (g, "Spread", { 560, flavourY, 60, rowH });
    drawCaption (g, "Sound", { 790, flavourY, 56, rowH });
    drawCaption (g, "Progression", { margin, laneHeadY, 110, rowH });


    // Grooves between sections
    for (int y : { flavourY - 7, padsY - 4, laneHeadY - 6, displayY - 6 })
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
    scale.setBounds (top.removeFromLeft (160));
    preview.setBounds (top.removeFromRight (114));
    top.removeFromRight (8);
    chordKeys.setBounds (top.removeFromRight (142));
    top.removeFromRight (8);
    learn.setBounds (top.removeFromRight (100));

    flavour.setBounds (margin + 74, flavourY, 450, rowH);
    spread.setBounds (622, flavourY, 156, rowH);
    sound.setBounds (848, flavourY - 1, right - 848, rowH + 2);

    const int padW = (getWidth() - 2 * margin) / pc::theory::numDegrees;
    for (int i = 0; i < pc::theory::numDegrees; ++i)
        pads[(size_t) i]->setBounds (margin + i * padW, padsY, padW, padsH);
    majorFive.setBounds (margin + 4 * padW + 10, fiveY, padW - 20, 30);

    auto head = juce::Rectangle<int> (margin + 120, laneHeadY, right - margin - 120, rowH);
    presets.setBounds (head.removeFromLeft (220).reduced (0, 1));
    head.removeFromLeft (8);
    playLane.setBounds (head.removeFromLeft (148));
    head.removeFromLeft (8);
    clearLane.setBounds (head.removeFromLeft (70));
    head.removeFromLeft (8);
    rhythm.setBounds (head.removeFromLeft (168).reduced (0, 1));
    dragLane.setBounds (head.removeFromRight (110));
    head.removeFromRight (8);
    exportMidi.setBounds (head.removeFromRight (juce::jmin (110, head.getWidth())));

    lane.setBounds (margin, laneY, right - margin, laneH);

    auto row = juce::Rectangle<int> (margin, displayY, right - margin, displayH);
    dragTake.setBounds (row.removeFromRight (150).reduced (0, 9));
    row.removeFromRight (6);
    capture.setBounds (row.removeFromRight (116).reduced (0, 9));
    row.removeFromRight (10);
    display.setBounds (row);

    keyboard.setBounds (margin, keyboardY, right - margin, keyboardH);
}
