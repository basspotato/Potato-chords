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
    constexpr int width = 1000, height = 910, margin = 26, topY = 22, topH = 42, flavourY = 78, rowH = 34,
                  padsY = 122, padsH = 202, fiveY = padsY + padsH + 2, laneHeadY = 362, laneY = 398, laneH = 88,
                  melodyHeadY = 494, melodyY = 530, melodyH = 150, displayY = 690, displayH = 56, keyboardY = 754, keyboardH = 130;
}

PotatoChordsEditor::PotatoChordsEditor (PotatoChordsProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      keyAttachment (p.apvts, pc::params::key.getParamID(), key),
      soundAttachment (p.apvts, pc::params::sound.getParamID(), sound),
      rhythmAttachment (p.apvts, pc::params::rhythm.getParamID(), rhythm),
      melodySoundAttachment (p.apvts, pc::params::melodySound.getParamID(), melodySound),
      scale (param (p, pc::params::scale), pc::params::scaleNames),
      flavour (param (p, pc::params::flavour), pc::params::flavourNames),
      spread (param (p, pc::params::spread), pc::params::spreadNames),
      chordKeys (param (p, pc::params::chordKeys), "Chord keys"),
      preview (param (p, pc::params::preview), "Preview"),
      majorFive (param (p, pc::params::majorFive), "Major V"),
      playLane (param (p, pc::params::playLane), "Play with host"),
      learn (param (p, pc::params::learn), "Learn"),
      playSong (param (p, pc::params::songPlay), "Play song"),
      chorusFader (param (p, pc::params::chorus), "Chorus"),
      reverbFader (param (p, pc::params::reverb), "Reverb"),
      strumFader (param (p, pc::params::strum), "Strum"),
      octave (param (p, pc::params::octave), pc::params::octaveNames),
      velocityFader (param (p, pc::params::velocity), "Velocity", true),
      humaniseFader (param (p, pc::params::humanise), "Humanise"),
      swingFader (param (p, pc::params::swing), "Swing"),
      melodyOctave (param (p, pc::params::melodyOctave), pc::params::melodyOctaveNames)
{
    setLookAndFeel (&look);
    octave.setCaption ("Octave", 56);
    melodyOctave.setCaption ("Octave", 56);

    key.addItemList (pc::params::keyNames, 1);
    // Attachments were made before the items existed, so resync the shown selection
    key.setSelectedItemIndex ((int) p.apvts.getRawParameterValue (pc::params::key.getParamID())->load(), juce::dontSendNotification);

    // Sound presets grouped by type under headings
    for (auto* box : { &sound, &melodySound })
    {
        const char* group = "";
        for (int i = 0; i < pc::synth::numPresets; ++i)
        {
            const auto& preset = pc::synth::presets[(size_t) i];
            if (juce::String (preset.group) != group)
            {
                group = preset.group;
                box->addSectionHeading (group);
            }
            box->addItem (preset.name, i + 1);
        }
    }
    sound.setSelectedItemIndex ((int) p.apvts.getRawParameterValue (pc::params::sound.getParamID())->load(), juce::dontSendNotification);
    melodySound.setSelectedItemIndex ((int) p.apvts.getRawParameterValue (pc::params::melodySound.getParamID())->load(), juce::dontSendNotification);

    melodyLane.onAdd = [this] (int start, int length, int row) { proc.rememberEdit(); proc.melody.addNote (start, length, row); refresh(); };
    melodyLane.onRemove = [this] (int step) { proc.rememberEdit(); proc.melody.removeNote (step); refresh(); };
    clearMelody.onClick = [this] { proc.rememberEdit(); proc.melody.clear(); refresh(); };
    repeatMelody.onClick = [this] { proc.rememberEdit(); proc.melody.repeatFirstHalf(); refresh(); };
    dragMelody.makeFile = [this]
    {
        auto value = [this] (const juce::ParameterID& id) { return (int) proc.apvts.getRawParameterValue (id.getParamID())->load(); };
        return pc::midiexport::writeTemp (pc::midiexport::melodyToMidi (proc.melody.snapshot(), value (pc::params::key),
                                                                        (pc::theory::Scale) value (pc::params::scale),
                                                                        proc.hostBpm(), proc.barQuarters(), pc::midiexport::melodyChannelDefault,
                                                                        proc.apvts.getRawParameterValue (pc::params::swing.getParamID())->load(),
                                                                        value (pc::params::melodyOctave) - 1, value (pc::params::velocity)),
                                          "Potato-Chords melody");
    };

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

    clearLane.onClick = [this] { proc.rememberEdit(); proc.lane.clear(); selectedSlot = 0; refresh(); };

    lane.onSelect = [this] (int slot) { selectedSlot = slot; refresh(); };
    lane.onSplit = [this] (int bar, bool split)
    {
        proc.rememberEdit();
        proc.lane.setSplit (bar, split);
        if (! split && selectedSlot == bar * 2 + 1) selectedSlot = bar * 2;
        refresh();
    };
    lane.flavourNames = pc::params::flavourNames;
    lane.onFlavour = [this] (int slotIndex, int flavourIndex)
    {
        auto slot = proc.lane.slot (slotIndex);
        if (slot.empty()) return;
        proc.rememberEdit();
        slot.flavour = (pc::theory::Flavour) flavourIndex;
        proc.lane.setSlot (slotIndex, slot);
        refresh();
    };
    lane.onClear = [this] (int slot) { proc.rememberEdit(); proc.lane.setSlot (slot, {}); refresh(); };

    dragLane.makeFile = [this] { return laneFile(); };
    exportMidi.onClick = [this] { exportLane(); };
    panic.onClick = [this] { proc.panic(); };
    undo.onClick = [this] { proc.undo(); refresh(); };
    redo.onClick = [this] { proc.redo(); refresh(); };
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
                                                            &playLane, &learn, &rhythm, &exportMidi, &presets, &writeLane, &clearLane, &dragLane, &dragTake, &capture,
                                                            &logo, &display, &lane, &keyboard,
                                                            &melodyLane, &melodySound, &clearMelody, &repeatMelody, &writeMelody, &dragMelody, &viewTabs, &songTemplate, &songLength, &songBpm, &playSong, &dragSong, &songStrip, &panic, &undo, &redo, &chorusFader, &reverbFader, &strumFader, &octave, &melodyOctave, &velocityFader, &humaniseFader, &swingFader })
        addAndMakeVisible (c);
    addChildComponent (writer);  // added last so they open on top
    addChildComponent (melodyWriter);

    writer.setChoice (p.writerStyle, p.writerMood);
    writer.onChoice = [this] (int s, int m) { proc.writerStyle = s; proc.writerMood = m; };
    writer.onClose = [this] { writer.setVisible (false); };
    writeLane.onClick = [this] { melodyWriter.setVisible (false); writer.setVisible (! writer.isVisible()); writer.toFront (false); };
    writer.setTips ("Writes a new progression for this style and mood: bars 5-8 are the chorus loop, bars 1-4 the verse. "
                    "Press again for another take. Also sets the scale and rhythm to suit the style.",
                    "Keeps your chords and fills only the empty bars with a loop that fits them.");
    writer.onWrite = [this] (int style, int mood)
    {
        const auto r = pc::writer::write ((pc::writer::Style) style, (pc::writer::Mood) mood, (uint32_t) juce::Random::getSystemRandom().nextInt());
        setParam (proc, pc::params::scale, r.scale == pc::theory::Scale::major ? 0.0f : 1.0f);
        setParam (proc, pc::params::rhythm, (float) r.rhythm);
        proc.rememberEdit();
        proc.lane.set (r.lane);
        selectedSlot = -1;
        refresh();
    };
    writer.onComplete = [this] (int style, int mood)
    {
        const bool minor = proc.apvts.getRawParameterValue (pc::params::scale.getParamID())->load() > 0.5f;
        proc.rememberEdit();
        proc.lane.set (pc::writer::complete (proc.lane.snapshot(), minor ? pc::theory::Scale::minor : pc::theory::Scale::major,
                                             (pc::writer::Style) style, (pc::writer::Mood) mood,
                                             (uint32_t) juce::Random::getSystemRandom().nextInt()));
        selectedSlot = -1;
        refresh();
    };

    melodyWriter.setChoice (p.melodyShape, p.melodyBusy);
    melodyWriter.onChoice = [this] (int s, int b) { proc.melodyShape = s; proc.melodyBusy = b; };
    melodyWriter.onClose = [this] { melodyWriter.setVisible (false); };
    writeMelody.onClick = [this] { writer.setVisible (false); melodyWriter.setVisible (! melodyWriter.isVisible()); melodyWriter.toFront (false); };
    melodyWriter.setTips ("Writes a melody over the progression: chord notes on beats 1 and 3, a question in bars 1-2 answered "
                          "in bars 3-4, and the chorus (bars 5-8) a little higher. Press again for another take.", {});
    melodyWriter.onWrite = [this] (int shape, int busy)
    {
        const bool minor = proc.apvts.getRawParameterValue (pc::params::scale.getParamID())->load() > 0.5f;
        proc.rememberEdit();
        proc.melody.set (pc::melodywriter::write (proc.lane.snapshot(), minor ? pc::theory::Scale::minor : pc::theory::Scale::major,
                                                  (pc::melodywriter::Shape) shape, (pc::melodywriter::Busy) busy,
                                                  (uint32_t) juce::Random::getSystemRandom().nextInt()));
        refresh();
    };

    for (int i = 0; i < pc::theory::numDegrees; ++i)
    {
        pads[(size_t) i] = std::make_unique<pc::gui::ChordPad> (i);
        pads[(size_t) i]->onDown = [this] (int degree) { padPressed (degree); };
        pads[(size_t) i]->onUp = [this] (int degree) { proc.padUp (degree); };
        pads[(size_t) i]->setTooltip ("Hold to hear this chord and send it out as MIDI. With a progression slot selected, "
                                      "clicking also puts the chord there. Top-right tags show likely next chords.");
        addAndMakeVisible (*pads[(size_t) i]);
    }

    // Song view: pick a template and a length, then drag sections (or the whole song) into the DAW
    for (int i = 0; i < pc::song::numTemplates; ++i) songTemplate.addItem (pc::song::templates[(size_t) i].name, i + 1);
    songLength.addItem ("Template length", 1);
    songBpm.setEditableText (true);
    songBpm.addItem ("Host tempo", 1);
    for (int bpm : { 70, 80, 90, 100, 110, 120, 124, 126, 128, 130, 140, 150, 160, 174 })
        songBpm.addItem (juce::String (bpm) + " BPM", bpm);
    for (int half = 4; half <= 12; ++half)
        songLength.addItem (juce::String (half / 2) + (half % 2 == 1 ? ":30" : ":00") + " min", half);
    {
        const auto s = proc.getSong();
        songTemplate.setSelectedId (s.templateIndex + 1, juce::dontSendNotification);
        songLength.setSelectedId (s.length >= 4 ? s.length : 1, juce::dontSendNotification);
        if (s.bpm > 0) songBpm.setText (juce::String (s.bpm) + " BPM", juce::dontSendNotification);
        else songBpm.setSelectedId (1, juce::dontSendNotification);
    }
    songBpm.onChange = [this]
    {
        // Typed or picked: keep it tidy as "128 BPM"; anything that isn't a tempo goes back to the host's
        const int bpm = songBpm.getSelectedId() == 1 ? 0 : songBpm.getText().getIntValue();
        if (bpm >= pc::song::minBpm && bpm <= pc::song::maxBpm) songBpm.setText (juce::String (bpm) + " BPM", juce::dontSendNotification);
        else songBpm.setSelectedId (1, juce::dontSendNotification);
        applySongChoice();
    };
    songTemplate.onChange = [this] { applySongChoice(); };
    songLength.onChange = [this] { applySongChoice(); };
    songStrip.onEdit = [this] (const pc::song::Song& s)
    {
        auto state = proc.getSong();
        state.song = s;
        proc.setSong (state);
        refresh();
    };
    songStrip.makeSectionFile = [this] (int i)
    {
        const auto s = proc.getSong().song;
        if (i < 0 || i >= s.count) return juce::File();
        const auto name = juce::String::formatted ("Potato-Chords %02d ", i + 1) + pc::song::info (s.sections[(size_t) i].kind).name;
        return songFile (i, i + 1, name);
    };
    dragSong.makeFile = [this] { return songFile (0, pc::song::maxSections, "Potato-Chords song"); };
    viewTabs.onChange = [this] (int i) { showView (i == 1); };
    showView (false);

    // Plain-words help on hover
    auto tip = [] (juce::SettableTooltipClient& c, const juce::String& text) { c.setTooltip (text); };
    tip (key, "The key of your song. The 7 chord pads are the chords that belong to it.");
    tip (scale, "Major sounds bright and happy. Minor sounds darker and more emotional.");
    tip (flavour, "How each chord is built. Triad is the plain chord; 7th, sus2, sus4 and add9 add colour; Power is just root and fifth.");
    tip (spread, "Close keeps notes tight together; Wide spreads them out with a low bass note.");
    tip (octave, "Moves every chord up or down whole octaves.");
    tip (melodyOctave, "Moves the melody up or down an octave, for a low bass line or a high lead.");
    tip (sound, "The built-in sound you hear for chords. It never changes the MIDI sent to your instrument.");
    tip (chordKeys, "On: the white keys C to B in one octave play the 7 chords of the key, so one key plays a whole chord.");
    tip (preview, "Turns the built-in preview sound on or off. Turn it off when your own instrument plays the MIDI.");
    tip (learn, "Shows the teaching extras: mood words and the three likeliest next chords, best first.");
    tip (majorFive, "Uses a major V chord in a minor key. It pulls strongly back to the home chord.");
    tip (undo, "Undo the last change to the progression or melody.");
    tip (redo, "Redo the change you just undid.");
    tip (presets, "Fills the progression with a well-known chord pattern. You can still edit every slot afterwards.");
    tip (playLane, "On: the progression and melody play along with your DAW's transport.");
    tip (writeLane, "Opens Write: a progression written for a style and mood, or your empty bars filled in.");
    tip (writeMelody, "Opens Write: a melody written over your progression in the shape you pick.");
    tip (clearLane, "Empties the progression lane.");
    tip (rhythm, "The rhythm the progression plays: sustained, pulses, stabs or arpeggios.");
    tip (exportMidi, "Saves the progression as a .mid file.");
    tip (dragLane, "Drag this into your DAW to drop the progression on a MIDI track.");
    tip (lane, "8 bars. Click a slot, then click a chord pad to fill it. The small box on a bar splits it into two chords. Right-click for more.");
    tip (melodySound, "The built-in sound you hear for the melody.");
    tip (clearMelody, "Empties the melody lane.");
    tip (repeatMelody, "Copies bars 1 to 4 of the melody over bars 5 to 8.");
    tip (dragMelody, "Drag this into your DAW to drop the melody on a MIDI track.");
    tip (melodyLane, "Drag right to draw a note, click a note to delete it. Only notes of the key are shown; glowing rows are the chord notes.");
    tip (display, "Shows the chord that is playing: its name, its number in the key, and its notes.");
    tip (capture, "Records everything the plugin sends out, so you can drag it into your DAW as a take.");
    tip (dragTake, "Drag the recorded take into your DAW.");
    tip (velocityFader, "How hard the pads, progression and melody play.");
    tip (humaniseFader, "Adds small random changes to velocity and timing so it sounds less robotic.");
    tip (strumFader, "Rolls each chord in from the lowest note to the highest, like a strummed guitar.");
    tip (swingFader, "Pushes every second 1/16 note late for a bouncy, shuffled feel.");
    tip (chorusFader, "Thickens the preview sound. Does not change the MIDI.");
    tip (reverbFader, "Adds room to the preview sound. Does not change the MIDI.");
    tip (panic, "Stops every sounding note. Use it if a note ever gets stuck.");
    tip (keyboard, "Lights up the notes that are playing, in their note colours. Click a key to play it (drag to slide); "
                   "with Chord keys on, the marked octave plays chords.");
    keyboard.onNoteDown = [this] (int note) { proc.keyDown (note); };
    keyboard.onNoteUp = [this] (int note) { proc.keyUp (note); };
    tip (logo, "Potato-Chords");
    tip (viewTabs, "Switch between drawing the melody and laying out a whole song.");
    tip (songTemplate, "A song shape from Pop and EDM hits: which sections come in what order, and how long each is.");
    tip (songLength, "Fits the song to a length at the current tempo by stretching intros, choruses and drops, or adding sections.");
    tip (playSong, "On: Play with host plays the whole song, section by section, instead of looping the 8 bars.");
    tip (songBpm, "The song's tempo: used to fit the length and written into the dragged MIDI. Type a number or pick one; "
                  "Host tempo follows your DAW. Playback always follows the DAW's tempo.");
    tip (dragSong, "Drag the whole song into your DAW: chords on one track, melody on another.");
    tip (songStrip, "Verse-type sections loop bars 1-4 of the progression, chorus-type sections loop bars 5-8. "
                    "Drag a block into your DAW for that section, right-click to change it.");

    // The layout is drawn at 1000 x 910 and scaled as a whole, keeping its shape
    // (read the remembered width first: setting the limits resizes the window, which overwrites it)
    const int w = juce::jlimit (width * 7 / 10, width * 8 / 5, proc.editorWidth > 0 ? proc.editorWidth : width);
    setResizable (true, true);
    setResizeLimits (width * 7 / 10, height * 7 / 10, width * 8 / 5, height * 8 / 5);
    getConstrainer()->setFixedAspectRatio ((double) width / height);
    setSize (w, (int) std::lround ((double) w * height / width));
    refresh();
    startTimerHz (30);
}

float PotatoChordsEditor::zoom() const { return (float) getWidth() / (float) width; }

PotatoChordsEditor::~PotatoChordsEditor()
{
    proc.setCapturing (false);
    keyboard.release();
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
    proc.rememberEdit();
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
                                    value (pc::params::rhythm), PotatoChordsProcessor::chordChannel,
                                    proc.apvts.getRawParameterValue (pc::params::strum.getParamID())->load() * pc::params::strumMaxSeconds,
                                    (int) proc.apvts.getRawParameterValue (pc::params::octave.getParamID())->load() - 2,
                                    proc.apvts.getRawParameterValue (pc::params::swing.getParamID())->load(), value (pc::params::velocity)),
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
    proc.rememberEdit();
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
    if (songView) songStrip.update (proc.getSong().song, proc.playingSection());
    undo.setEnabled (proc.canUndo());
    redo.setEnabled (proc.canRedo());
    const int held = proc.heldDegree();
    const bool flats = proc.keyUsesFlats();
    const bool minor = (int) proc.apvts.getRawParameterValue (pc::params::scale.getParamID())->load() == 1;
    const bool keysOn = proc.apvts.getRawParameterValue (pc::params::chordKeys.getParamID())->load() > 0.5f;
    const auto scaleNow = minor ? pc::theory::Scale::minor : pc::theory::Scale::major;
    const bool learning = proc.apvts.getRawParameterValue (pc::params::learn.getParamID())->load() > 0.5f;
    const int hintFrom = hintSource();
    const bool majorFiveOn = proc.apvts.getRawParameterValue (pc::params::majorFive.getParamID())->load() > 0.5f;

    std::array<juce::String, pc::theory::numDegrees> numerals;
    for (int i = 0; i < pc::theory::numDegrees; ++i)
    {
        const auto chord = proc.chordFor (i);
        pads[(size_t) i]->setChord (chord, flats, held == i);
        pads[(size_t) i]->setHint (learning ? pc::presets::nextChordRank (scaleNow, hintFrom, i) : 0);
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

    // Melody: rows in the key, and the chord under every step for the chord-tone tint
    {
        pc::gui::MelodyLane::View mv;
        mv.tune = proc.melody.snapshot();
        const int keyNow = (int) proc.apvts.getRawParameterValue (pc::params::key.getParamID())->load();
        for (int row = 0; row < pc::melody::numRows; ++row)
        {
            const int note = pc::melody::rowToNote (row, keyNow, scaleNow,
                                                    (int) proc.apvts.getRawParameterValue (pc::params::melodyOctave.getParamID())->load() - 1);
            mv.rowPitchClass[(size_t) row] = note % 12;
            mv.rowName[(size_t) row] = juce::String (pc::theory::noteName (note, flats)) + juce::String (note / 12 - 2);
        }
        const double barQ = proc.barQuarters();
        mv.stepsInBar = juce::jlimit (1, pc::melody::stepsPerBar, (int) std::llround (barQ / pc::sequencer::stepQuarters));
        for (int step = 0; step < pc::melody::numSteps; ++step)
        {
            const int bar = step / pc::melody::stepsPerBar, inBar = step % pc::melody::stepsPerBar;
            const int slotIndex = bar * 2 + (snap.isSplit (bar) && inBar * pc::sequencer::stepQuarters >= barQ * 0.5 - 1.0e-9 ? 1 : 0);
            const auto slot = snap.chordIn (slotIndex);
            if (slot.empty()) continue;
            const auto chord = proc.chordFor (slot);
            for (int k = 0; k < chord.size; ++k)
                mv.chordMask[(size_t) step] |= (uint16_t) (1 << ((chord.root + chord.intervals[(size_t) k]) % 12));
        }
        mv.playingStep = proc.playingMelodyStep();
        dragMelody.setEnabledLook (mv.tune.cells != pc::melody::Snapshot {}.cells);
        melodyLane.update (mv);
    }

    bool laneHasChords = false;
    for (auto& v : views) laneHasChords = laneHasChords || ! v.empty;
    dragLane.setEnabledLook (laneHasChords);
    dragSong.setEnabledLook (laneHasChords);

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

    g.addTransform (juce::AffineTransform::scale (zoom()));

    // The device body
    const auto body = juce::Rectangle<float> ((float) width, (float) height).reduced (10.0f, 10.0f);
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
    for (int y : { flavourY - 7, padsY - 4, laneHeadY - 6, melodyHeadY - 6, displayY - 6 })
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawHorizontalLine (y, (float) margin, (float) (width - margin));
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine (y + 1, (float) margin, (float) (width - margin));
    }
}

void PotatoChordsEditor::resized()
{
    background = look::makeBackground (getWidth(), getHeight());
    proc.editorWidth = getWidth();
    for (auto* c : getChildren())
        if (c != resizableCorner.get() && c != &tooltips)
            c->setTransform (juce::AffineTransform::scale (zoom()));

    const int right = width - margin;
    logo.setBounds (margin - 4, topY - 6, 66, topH + 12);
    auto top = juce::Rectangle<int> (318, topY, right - 318, topH);
    key.setBounds (top.removeFromLeft (76));
    top.removeFromLeft (8);
    scale.setBounds (top.removeFromLeft (160));
    top.removeFromLeft (10);
    undo.setBounds (top.removeFromLeft (38));
    redo.setBounds (top.removeFromLeft (38));
    preview.setBounds (top.removeFromRight (100));
    top.removeFromRight (8);
    chordKeys.setBounds (top.removeFromRight (124));
    top.removeFromRight (8);
    learn.setBounds (top.removeFromRight (92));

    flavour.setBounds (margin + 74, flavourY, 450, rowH);
    spread.setBounds (622, flavourY, 156, rowH);
    sound.setBounds (848, flavourY - 1, right - 848, rowH + 2);

    const int padW = (width - 2 * margin) / pc::theory::numDegrees;
    for (int i = 0; i < pc::theory::numDegrees; ++i)
        pads[(size_t) i]->setBounds (margin + i * padW, padsY, padW, padsH);
    majorFive.setBounds (margin + 4 * padW + 10, fiveY, padW - 20, 30);

    auto head = juce::Rectangle<int> (margin + 120, laneHeadY, right - margin - 120, rowH);
    presets.setBounds (head.removeFromLeft (150).reduced (0, 1));
    head.removeFromLeft (8);
    writeLane.setBounds (head.removeFromLeft (66));
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
    writer.setBounds (margin + 120, laneY + laneH + 4, 560, 166);  // below the lane, so the new chords stay in view
    melodyWriter.setBounds (margin + 120, padsY + 20, 560, 166);    // over the pads, so the melody stays in view

    auto melodyHead = juce::Rectangle<int> (margin + 120, melodyHeadY, right - margin - 120, rowH);
    melodySound.setBounds (melodyHead.removeFromLeft (180).reduced (0, 1));
    melodyHead.removeFromLeft (8);
    clearMelody.setBounds (melodyHead.removeFromLeft (70));
    repeatMelody.setBounds (melodyHead.removeFromLeft (108));
    melodyHead.removeFromLeft (14);
    melodyOctave.setBounds (melodyHead.removeFromLeft (170));
    melodyHead.removeFromLeft (8);
    writeMelody.setBounds (melodyHead.removeFromLeft (66));
    dragMelody.setBounds (melodyHead.removeFromRight (130));
    melodyLane.setBounds (margin, melodyY, right - margin, melodyH);

    viewTabs.setBounds (margin, melodyHeadY, 112, rowH);
    auto songHead = juce::Rectangle<int> (margin + 120, melodyHeadY, right - margin - 120, rowH);
    songTemplate.setBounds (songHead.removeFromLeft (180).reduced (0, 1));
    songHead.removeFromLeft (8);
    songLength.setBounds (songHead.removeFromLeft (150).reduced (0, 1));
    songHead.removeFromLeft (8);
    songBpm.setBounds (songHead.removeFromLeft (120).reduced (0, 1));
    dragSong.setBounds (songHead.removeFromRight (130));
    songHead.removeFromRight (8);
    playSong.setBounds (songHead.removeFromRight (120));
    songStrip.setBounds (margin, melodyY, right - margin, melodyH);

    auto row = juce::Rectangle<int> (margin, displayY, right - margin, displayH);
    dragTake.setBounds (row.removeFromRight (150).reduced (0, 9));
    row.removeFromRight (6);
    capture.setBounds (row.removeFromRight (116).reduced (0, 9));
    row.removeFromRight (10);
    auto feel = row.removeFromRight (150);
    velocityFader.setBounds (feel.removeFromTop (displayH / 2));
    humaniseFader.setBounds (feel);
    row.removeFromRight (10);
    display.setBounds (row);

    keyboard.setBounds (margin, keyboardY, right - margin, keyboardH);
    panic.setBounds (right - 128, keyboardY + 3, 120, 26);
    reverbFader.setBounds (right - 128 - 10 - 112, keyboardY + 3, 112, 26);
    chorusFader.setBounds (reverbFader.getX() - 8 - 112, keyboardY + 3, 112, 26);
    swingFader.setBounds (chorusFader.getX() - 20 - 112, keyboardY + 3, 112, 26);
    strumFader.setBounds (swingFader.getX() - 8 - 112, keyboardY + 3, 112, 26);
    octave.setBounds (strumFader.getX() - 8 - 186, keyboardY + 3, 186, 26);
}

// The settings generated MIDI plays with, as the plugin plays it live
pc::midiexport::Feel PotatoChordsEditor::feel (bool forMelody) const
{
    auto value = [this] (const juce::ParameterID& id) { return proc.apvts.getRawParameterValue (id.getParamID())->load(); };
    pc::midiexport::Feel f;
    f.key = (int) value (pc::params::key);
    f.scale = (pc::theory::Scale) (int) value (pc::params::scale);
    f.spread = (pc::voicing::Spread) (int) value (pc::params::spread);
    f.bpm = proc.hostBpm();
    f.barQuarters = proc.barQuarters();
    f.velocity = (int) value (pc::params::velocity);
    f.swing = value (pc::params::swing);
    if (forMelody)
    {
        f.channel = PotatoChordsProcessor::melodyChannel;
        f.octaves = (int) value (pc::params::melodyOctave) - 1;
    }
    else
    {
        f.channel = PotatoChordsProcessor::chordChannel;
        f.octaves = (int) value (pc::params::octave) - 2;
        f.strumSeconds = value (pc::params::strum) * pc::params::strumMaxSeconds;
    }
    return f;
}

juce::File PotatoChordsEditor::songFile (int first, int last, const juce::String& name)
{
    const auto s = proc.getSong();
    auto chords = feel (false), tune = feel (true);
    if (s.bpm > 0) chords.bpm = tune.bpm = s.bpm;
    return pc::midiexport::writeTemp (pc::midiexport::songToMidi (s.song, first, last, proc.lane.snapshot(), proc.melody.snapshot(), chords, tune),
                                      name);
}

// A new template or length rebuilds the section list
void PotatoChordsEditor::applySongChoice()
{
    PotatoChordsProcessor::SongState s;
    s.templateIndex = juce::jmax (0, songTemplate.getSelectedId() - 1);
    s.length = songLength.getSelectedId() >= 4 ? songLength.getSelectedId() : 0;
    s.bpm = songBpm.getSelectedId() == 1 ? 0 : juce::jlimit (0, pc::song::maxBpm, songBpm.getText().getIntValue());
    const double bpm = s.bpm > 0 ? s.bpm : proc.hostBpm();
    const int bars = s.length > 0 ? pc::song::barsForMinutes (s.length / 2.0, bpm, proc.barQuarters()) : 0;
    s.song = bars > 0 ? pc::song::fitToBars (s.templateIndex, bars) : pc::song::fromTemplate (s.templateIndex);
    proc.setSong (s);
    refresh();
}

void PotatoChordsEditor::showView (bool song)
{
    songView = song;
    for (auto* c : std::initializer_list<juce::Component*> { &melodySound, &clearMelody, &repeatMelody, &writeMelody, &melodyOctave, &dragMelody, &melodyLane })
        c->setVisible (! song);
    for (auto* c : std::initializer_list<juce::Component*> { &songTemplate, &songLength, &songBpm, &playSong, &dragSong, &songStrip })
        c->setVisible (song);
    refresh();
}
