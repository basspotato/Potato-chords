#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::Identifier versionProperty { "version" };
    const juce::Identifier progressionProperty { "progression" };
    const juce::Identifier melodyProperty { "melody" };
    const juce::Identifier songProperty { "song" };
    const juce::Identifier songTemplateProperty { "songTemplate" };
    const juce::Identifier songLengthProperty { "songLength" };
}

PotatoChordsProcessor::PotatoChordsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PotatoChords", pc::params::createLayout())
{
    apvts.state.setProperty (versionProperty, stateVersion, nullptr);

    using namespace pc::params;
    keyParam       = apvts.getRawParameterValue (key.getParamID());
    scaleParam     = apvts.getRawParameterValue (scale.getParamID());
    flavourParam   = apvts.getRawParameterValue (flavour.getParamID());
    spreadParam    = apvts.getRawParameterValue (spread.getParamID());
    majorFiveParam = apvts.getRawParameterValue (majorFive.getParamID());
    chordKeysParam = apvts.getRawParameterValue (chordKeys.getParamID());
    soundParam     = apvts.getRawParameterValue (sound.getParamID());
    previewParam   = apvts.getRawParameterValue (preview.getParamID());
    playLaneParam  = apvts.getRawParameterValue (playLane.getParamID());
    rhythmParam    = apvts.getRawParameterValue (rhythm.getParamID());
    melodySoundParam = apvts.getRawParameterValue (melodySound.getParamID());
    chorusParam = apvts.getRawParameterValue (chorus.getParamID());
    reverbParam = apvts.getRawParameterValue (reverb.getParamID());
    strumParam = apvts.getRawParameterValue (strum.getParamID());
    octaveParam = apvts.getRawParameterValue (octave.getParamID());
    velocityParam = apvts.getRawParameterValue (velocity.getParamID());
    humaniseParam = apvts.getRawParameterValue (humanise.getParamID());
    swingParam = apvts.getRawParameterValue (swing.getParamID());
    melodyOctaveParam = apvts.getRawParameterValue (melodyOctave.getParamID());

    out.ensureSize (8192);
}

void PotatoChordsProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    synth.prepare (newSampleRate);
    fx.prepare (newSampleRate);
    out.ensureSize (8192);
    // Anything still held from before goes out as note-offs at the start of the next block
    flushRequested = true;
}

bool PotatoChordsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

PotatoChordsProcessor::Settings PotatoChordsProcessor::readSettings() const
{
    Settings s;
    s.key = (int) keyParam->load();
    s.scale = (int) scaleParam->load();
    s.flavour = (int) flavourParam->load();
    s.spread = (int) spreadParam->load();
    s.majorFive = majorFiveParam->load() > 0.5f;
    s.octave = (int) octaveParam->load() - 2;
    return s;
}

pc::theory::Chord PotatoChordsProcessor::chordFor (int degree) const
{
    const auto s = readSettings();
    return pc::theory::diatonicChord (s.key, (pc::theory::Scale) s.scale, degree, (pc::theory::Flavour) s.flavour, s.majorFive);
}

pc::theory::Chord PotatoChordsProcessor::chordFor (const pc::progression::Slot& slot) const
{
    const auto s = readSettings();
    return pc::theory::diatonicChord (s.key, (pc::theory::Scale) s.scale, std::max (0, slot.degree), slot.flavour, slot.majorFive);
}

bool PotatoChordsProcessor::keyUsesFlats() const
{
    const auto s = readSettings();
    return pc::theory::usesFlats (s.key, (pc::theory::Scale) s.scale);
}

pc::voicing::Voiced PotatoChordsProcessor::soundingNotes() const
{
    pc::voicing::Voiced v;
    const auto packed = activeNotes.load (std::memory_order_relaxed);
    for (int i = 0; i < pc::voicing::maxNotes; ++i)
    {
        const int note = (int) ((packed >> (8 * i)) & 0xff);
        if (note < 128) v.notes[(size_t) v.size++] = note;
    }
    return v;
}

void PotatoChordsProcessor::pushPadEvent (int event)
{
    const auto scope = padFifo.write (1);
    if (scope.blockSize1 > 0) padEvents[(size_t) scope.startIndex1] = event;
}

void PotatoChordsProcessor::playChord (int degree, juce::uint8 velocity, int samplePos)
{
    stopChord (samplePos);
    chordSettings = readSettings();
    const auto chord = pc::theory::diatonicChord (chordSettings.key, (pc::theory::Scale) chordSettings.scale, degree,
                                                  (pc::theory::Flavour) chordSettings.flavour, chordSettings.majorFive);
    previousClose = pc::voicing::closeVoicing (chord, previousClose);
    playing = pc::voicing::shiftOctaves (pc::voicing::applySpread (chord, previousClose, (pc::voicing::Spread) chordSettings.spread),
                                         chordSettings.octave);
    startNotes (liveOwner, playing, velocity, samplePos);
    currentDegree = degree;
    chordVelocity = velocity;
}

void PotatoChordsProcessor::stopChord (int samplePos)
{
    stopNotes (liveOwner, playing, samplePos);
    playing.size = 0;
}

// A chord's notes, low to high and `strum` apart; the lowest goes out on samplePos
void PotatoChordsProcessor::startNotes (int owner, const pc::voicing::Voiced& notes, juce::uint8 velocity, int samplePos, int channel)
{
    const double gap = strumParam->load() * pc::params::strumMaxSeconds * sampleRate;
    const float human = humaniseParam->load();
    for (int i = 0; i < notes.size; ++i)
    {
        const int note = notes.notes[(size_t) i];
        int rank = 0;
        for (int j = 0; j < notes.size; ++j)
            rank += notes.notes[(size_t) j] < note || (notes.notes[(size_t) j] == note && j < i) ? 1 : 0;
        auto delay = (int64_t) std::llround (gap * rank) + (owner == liveOwner ? 0 : swingDelay);
        auto vel = velocity;
        if (human > 0.0f)
        {
            // Humanise: a little late, never early, so a note is never moved before its chord change
            delay += (int64_t) std::llround (random.nextFloat() * human * pc::params::humaniseMaxSeconds * sampleRate);
            const int change = juce::roundToInt ((random.nextFloat() * 2.0f - 1.0f) * human * (float) pc::params::humaniseMaxVelocity);
            vel = (juce::uint8) juce::jlimit (1, 127, (int) velocity + change);
        }
        if (delay == 0) tracker.noteOn (out, channel, note, vel, samplePos);
        else            strummer.add (owner, channel, note, vel, blockStart + samplePos + delay);
    }
}

// Notes due before the stop still go out first; later ones never start
void PotatoChordsProcessor::stopNotes (int owner, const pc::voicing::Voiced& notes, int samplePos, int channel)
{
    strummer.fireUntil (tracker, out, blockStart, blockStart + samplePos, owner);
    for (int i = 0; i < notes.size; ++i)
        if (! strummer.cancel (owner, channel, notes.notes[(size_t) i]))
            tracker.noteOff (out, channel, notes.notes[(size_t) i], samplePos);
}

void PotatoChordsProcessor::flushAll (int samplePos)
{
    strummer.clear();
    tracker.allOff (out, samplePos);
    playing.size = 0;
    lanePlaying.size = 0;
    laneVoicing.size = 0;
    laneRunning = false;  // the lane restarts its current chord on the next block if the host is playing
    laneCurrent = -1;
    melodyNote = -1;
    source = Source::none;
    currentDegree = -1;
    for (auto& ch : passHeld) ch.fill (0);
}

void PotatoChordsProcessor::handleMidi (const juce::MidiBuffer& in, bool chordKeysOn, bool padsOn)
{
    // Pad events have no timing of their own, so they land at the start of the block
    const auto scope = padFifo.read (padFifo.getNumReady());
    auto handlePad = [&] (int event)
    {
        if (! padsOn) return;
        const int degree = std::abs (event) - 1;
        if (degree < 0 || degree >= pc::theory::numDegrees) return;
        if (event > 0)
        {
            playChord (degree, noteVelocity(), 0);
            source = Source::pad;
        }
        else if (source == Source::pad && currentDegree == degree)
        {
            stopChord (0);
            source = Source::none;
            currentDegree = -1;
        }
    };
    for (int i = 0; i < scope.blockSize1; ++i) handlePad (padEvents[(size_t) (scope.startIndex1 + i)]);
    for (int i = 0; i < scope.blockSize2; ++i) handlePad (padEvents[(size_t) (scope.startIndex2 + i)]);

    for (const auto meta : in)
    {
        const auto msg = meta.getMessage();
        const int pos = meta.samplePosition;

        if (msg.isNoteOn())
        {
            const int ch = msg.getChannel(), note = msg.getNoteNumber();
            if (chordKeysOn && note >= chordKeysLow && note < chordKeysLow + 12)
            {
                const int degree = pc::theory::whiteKeyDegree (note);
                if (degree >= 0)
                {
                    playChord (degree, msg.getVelocity(), pos);
                    source = Source::key;
                    triggerChannel = ch;
                    triggerNote = note;
                }
                continue;  // black keys in the chord octave do nothing
            }
            tracker.noteOn (out, ch, note, msg.getVelocity(), pos);
            auto& count = passHeld[(size_t) ch - 1][(size_t) note];
            if (count < 255) ++count;
        }
        else if (msg.isNoteOff())
        {
            const int ch = msg.getChannel(), note = msg.getNoteNumber();
            // Matched by what the note-on did, not the current mode, so toggling Chord keys never strands a note
            if (source == Source::key && ch == triggerChannel && note == triggerNote)
            {
                stopChord (pos);
                source = Source::none;
                currentDegree = -1;
            }
            else if (auto& count = passHeld[(size_t) ch - 1][(size_t) note]; count > 0)
            {
                tracker.noteOff (out, ch, note, pos);
                --count;
            }
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            flushAll (pos);
            out.addEvent (msg, pos);
        }
        else
        {
            out.addEvent (msg, pos);
        }
    }
}

void PotatoChordsProcessor::enterLaneSlot (int slotIndex, const pc::progression::Snapshot& snap)
{
    laneCurrent = slotIndex;
    const auto slot = snap.chordIn (slotIndex);
    laneCode = slot.encode();
    laneVoicing.size = 0;
    if (slot.empty()) return;
    const auto chord = pc::theory::diatonicChord (laneSettings.key, (pc::theory::Scale) laneSettings.scale, slot.degree,
                                                  slot.flavour, slot.majorFive);
    lanePreviousClose = pc::voicing::closeVoicing (chord, lanePreviousClose);
    laneVoicing = pc::voicing::shiftOctaves (pc::voicing::applySpread (chord, lanePreviousClose, (pc::voicing::Spread) laneSettings.spread),
                                             laneSettings.octave);
}

void PotatoChordsProcessor::startLaneHit (const pc::sequencer::Hit& hit, int samplePos)
{
    stopLaneChord (samplePos);
    if (laneVoicing.size == 0) return;
    if (hit.arp < 0)
    {
        lanePlaying = laneVoicing;
    }
    else
    {
        // Arp: the n-th chord note, going up an octave each time round
        lanePlaying.size = 1;
        lanePlaying.notes[0] = juce::jmin (127, laneVoicing.notes[(size_t) (hit.arp % laneVoicing.size)] + 12 * (hit.arp / laneVoicing.size));
    }
    startNotes (laneOwner, lanePlaying, noteVelocity(), samplePos);
}

void PotatoChordsProcessor::stopLaneChord (int samplePos)
{
    stopNotes (laneOwner, lanePlaying, samplePos);
    lanePlaying.size = 0;
}

// Plays the lane in time with the host. The lane loops every 8 bars from the song start; within each chord slot
// the rhythm pattern fires on a 1/16 grid. Every note lands on its exact sample, and a stop, jump, loop wrap
// or tempo change never leaves a note hanging.
void PotatoChordsProcessor::runLane (int numSamples)
{
    bool playingNow = false;
    double ppq = 0.0, bpm = 120.0;
    int numerator = 4, denominator = 4;
    if (auto* head = getPlayHead())
        if (const auto pos = head->getPosition())
        {
            playingNow = pos->getIsPlaying();
            if (const auto p = pos->getPpqPosition()) ppq = *p; else playingNow = false;
            if (const auto b = pos->getBpm()) bpm = *b;
            if (const auto ts = pos->getTimeSignature()) { numerator = ts->numerator; denominator = ts->denominator; }
        }
    hostPlaying.store (playingNow, std::memory_order_relaxed);
    blockPlaying = playingNow;
    blockPpq = ppq;
    blockBpm = bpm > 0.0 ? bpm : 120.0;
    lastBpm.store (blockBpm, std::memory_order_relaxed);
    lastBarQuarters.store (numerator > 0 && denominator > 0 ? numerator * 4.0 / denominator : 4.0, std::memory_order_relaxed);

    if (! playingNow || playLaneParam->load() < 0.5f || bpm <= 0.0 || numSamples <= 0)
    {
        stopLaneChord (0);
        stopMelodyNote (0);
        laneRunning = false;
        laneCurrent = -1;
        melodyStepShown.store (-1, std::memory_order_relaxed);
        return;
    }

    using pc::sequencer::stepQuarters;
    const double barQ = numerator > 0 && denominator > 0 ? numerator * 4.0 / denominator : 4.0;
    const double loopQ = pc::progression::Snapshot::lengthQuarters (barQ);
    const double ppqPerSample = bpm / 60.0 / sampleRate;
    const double endPpq = ppq + numSamples * ppqPerSample;
    const int swingSamples = (int) std::llround (swingParam->load() * pc::params::swingMaxStep * stepQuarters / ppqPerSample);
    const auto snap = lane.snapshot();
    const auto settings = readSettings();
    const auto& pattern = pc::sequencer::patterns[(size_t) juce::jlimit (0, pc::sequencer::numPatterns - 1, (int) rhythmParam->load())];
    auto inLoop = [loopQ] (double p) { const double m = std::fmod (p, loopQ); return m < 0.0 ? m + loopQ : m; };
    auto stepInSlot = [&] (double posInLoop, int slotIndex)
    {
        const int bar = slotIndex / 2;
        const double slotStart = bar * barQ + (slotIndex % 2 == 1 ? barQ * 0.5 : 0.0);
        return (int) std::floor ((posInLoop - slotStart) / stepQuarters + 1.0e-6);
    };

    // A jump (start, locate, host loop, tempo change that moves the position), an edit to the current slot or a
    // key change: re-enter the slot and sound whatever the pattern has at this moment
    const bool jumped = ! laneRunning || std::abs (ppq - expectedPpq) > ppqPerSample + 1.0e-6;
    const bool settingsChanged = settings != laneSettings;
    laneSettings = settings;
    const double startInLoop = inLoop (ppq + 1.0e-9);
    const int now = snap.slotAt (startInLoop, barQ);
    const bool restarted = jumped || now != laneCurrent || snap.chordIn (now).encode() != laneCode || settingsChanged;
    const auto tune = melody.snapshot();
    auto melodyIndex = [barQ] (double posInLoop)
    {
        const int bar = juce::jlimit (0, pc::progression::numBars - 1, (int) (posInLoop / barQ));
        const int step = (int) std::floor ((posInLoop - bar * barQ) / pc::sequencer::stepQuarters + 1.0e-6);
        return step < pc::melody::stepsPerBar ? bar * pc::melody::stepsPerBar + step : -1;
    };
    if (jumped || settingsChanged)
        melodyStep (tune, melodyIndex (startInLoop), true, 0);
    if (restarted)
    {
        stopLaneChord (0);
        enterLaneSlot (now, snap);
        if (const auto* hit = pc::sequencer::hitSoundingAt (pattern, stepInSlot (startInLoop, now)))
            startLaneHit (*hit, 0);
    }

    // Grid steps inside this block, each on its own sample
    double g = std::ceil (ppq / stepQuarters - 1.0e-6) * stepQuarters;
    if (restarted && g < ppq + 1.0e-6)
    {
        // Already handled above for the chord; the melody still needs this step unless it restarted too
        if (! (jumped || settingsChanged)) melodyStep (tune, melodyIndex (startInLoop), false, 0);
        g += stepQuarters;
    }
    for (int guard = 0; guard < 512 && g < endPpq - 1.0e-9; ++guard, g += stepQuarters)
    {
        const double posInLoop = inLoop (g + 1.0e-9);
        const int slotIndex = snap.slotAt (posInLoop, barQ);
        const int samplePos = juce::jlimit (0, numSamples - 1, (int) std::llround ((g - ppq) / ppqPerSample));
        if (slotIndex != laneCurrent)
        {
            stopLaneChord (samplePos);
            enterLaneSlot (slotIndex, snap);
        }
        const int tuneIndex = melodyIndex (posInLoop);
        swingDelay = tuneIndex >= 0 && tuneIndex % 2 == 1 ? swingSamples : 0;
        melodyStep (tune, tuneIndex, false, samplePos);
        const int step = stepInSlot (posInLoop, slotIndex);
        swingDelay = step % 2 == 1 ? swingSamples : 0;
        if (const auto* hit = pc::sequencer::hitStartingAt (pattern, step))
            startLaneHit (*hit, samplePos);
        else if (pc::sequencer::hitSoundingAt (pattern, step) == nullptr)
            stopLaneChord (samplePos);
    }
    swingDelay = 0;

    melodyStepShown.store (melodyIndex (inLoop (endPpq - 1.0e-9)), std::memory_order_relaxed);
    expectedPpq = endPpq;
    laneRunning = true;
}

// One 1/16 step of the melody: a note start retriggers, a hold carries on, anything else ends the note.
// On a restart a note is picked up even half way through.
void PotatoChordsProcessor::melodyStep (const pc::melody::Snapshot& tune, int step, bool restart, int samplePos)
{
    const auto cell = step >= 0 ? tune.cell (step) : pc::melody::Cell {};
    if (cell.empty())
    {
        stopMelodyNote (samplePos);
        return;
    }
    const int note = pc::melody::rowToNote (cell.row, laneSettings.key, (pc::theory::Scale) laneSettings.scale,
                                          (int) melodyOctaveParam->load() - 1);
    if (cell.hold && ! restart && note == melodyNote) return;
    stopMelodyNote (samplePos);
    melodyNote = note;
    pc::voicing::Voiced one;
    one.notes[0] = note;
    one.size = 1;
    startNotes (melodyOwner, one, noteVelocity(), samplePos, melodyChannel);
}

void PotatoChordsProcessor::stopMelodyNote (int samplePos)
{
    if (melodyNote >= 0)
    {
        pc::voicing::Voiced one;
        one.notes[0] = melodyNote;
        one.size = 1;
        stopNotes (melodyOwner, one, samplePos, melodyChannel);
    }
    melodyNote = -1;
}

void PotatoChordsProcessor::captureOutput (int numSamples)
{
    // With the transport running, events keep their song position; when stopped, a free-running clock at the
    // last known tempo keeps the timing as played
    const double quartersPerSample = blockBpm / 60.0 / sampleRate;
    const double start = blockPlaying ? blockPpq : freeQuarters;
    freeQuarters = start + numSamples * quartersPerSample;
    if (! capturing.load (std::memory_order_relaxed)) return;

    for (const auto meta : out)
    {
        const auto m = meta.getMessage();
        if (! (m.isNoteOnOrOff())) continue;
        const auto scope = captureFifo.write (1);
        if (scope.blockSize1 == 0) return;  // full: the message thread is behind, drop rather than block
        auto& e = captureEvents[(size_t) scope.startIndex1];
        const auto* raw = m.getRawData();
        e.quarters = start + meta.samplePosition * quartersPerSample;
        e.status = raw[0];
        e.data1 = raw[1];
        e.data2 = raw[2];
    }
}

void PotatoChordsProcessor::renderPreview (juce::AudioBuffer<float>& buffer)
{
    buffer.clear();
    const bool previewOn = previewParam->load() > 0.5f && buffer.getNumChannels() >= 2;
    if (! previewOn)
    {
        if (previewWasOn) { synth.killAll(); fx.reset(); }
        previewWasOn = false;
        return;
    }
    previewWasOn = true;
    synth.setPreset ((int) soundParam->load());
    synth.setChannelPreset (melodyChannel, (int) melodySoundParam->load());

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    int done = 0;
    for (const auto meta : out)
    {
        const int pos = juce::jlimit (0, buffer.getNumSamples(), meta.samplePosition);
        if (pos > done)
        {
            synth.render (left + done, right + done, pos - done);
            done = pos;
        }
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())                                   synth.noteOn (msg.getChannel(), msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())                             synth.noteOff (msg.getChannel(), msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())  synth.releaseAll();
    }
    if (done < buffer.getNumSamples())
        synth.render (left + done, right + done, buffer.getNumSamples() - done);
    fx.process (left, right, buffer.getNumSamples(), chorusParam->load(), reverbParam->load());

    // Gentle soft clip so stacked chords never overload
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            d[i] = std::tanh (d[i]);
    }
}

void PotatoChordsProcessor::publishState()
{
    activeDegree.store (source == Source::none ? -1 : currentDegree, std::memory_order_relaxed);
    laneSlot.store (laneRunning ? laneCurrent : -1, std::memory_order_relaxed);
    // The display shows the live chord if there is one, else the lane's chord
    const auto& shown = playing.size > 0 ? playing : (laneRunning ? laneVoicing : lanePlaying);
    uint64_t packed = ~uint64_t (0);
    for (int i = 0; i < shown.size && i < pc::voicing::maxNotes; ++i)
    {
        packed &= ~(uint64_t (0xff) << (8 * i));
        packed |= uint64_t (shown.notes[(size_t) i]) << (8 * i);
    }
    activeNotes.store (packed, std::memory_order_relaxed);
    const auto bits = tracker.noteBits();
    outputNotes[0].store (bits[0], std::memory_order_relaxed);
    outputNotes[1].store (bits[1], std::memory_order_relaxed);
}

void PotatoChordsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    out.clear();

    if (flushRequested.exchange (false))
        flushAll (0);

    // Changing key, scale, flavour or spread while a chord is held re-voices it straight away
    if (source != Source::none && readSettings() != chordSettings)
        playChord (currentDegree, chordVelocity, 0);

    handleMidi (midi, chordKeysParam->load() > 0.5f, true);
    runLane (buffer.getNumSamples());
    strummer.fireUntil (tracker, out, blockStart, blockStart + buffer.getNumSamples());
    blockStart += buffer.getNumSamples();
    captureOutput (buffer.getNumSamples());
    renderPreview (buffer);
    midi.swapWith (out);
    publishState();
}

void PotatoChordsProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // Bypassed: release the chord and play MIDI straight through (still tracked, so nothing hangs on return)
    out.clear();
    if (flushRequested.exchange (false))
        flushAll (0);
    if (source != Source::none)
    {
        stopChord (0);
        source = Source::none;
        currentDegree = -1;
    }
    stopLaneChord (0);
    stopMelodyNote (0);
    laneRunning = false;
    laneCurrent = -1;
    handleMidi (midi, false, false);
    strummer.fireUntil (tracker, out, blockStart, blockStart + buffer.getNumSamples());
    blockStart += buffer.getNumSamples();
    midi.swapWith (out);
    buffer.clear();
    synth.killAll();
    fx.reset();
    publishState();
}

juce::AudioProcessorEditor* PotatoChordsProcessor::createEditor()
{
    return new PotatoChordsEditor (*this);
}

void PotatoChordsProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (versionProperty, stateVersion, nullptr);
    state.setProperty (progressionProperty, juce::String (lane.snapshot().toString()), nullptr);
    state.setProperty (melodyProperty, juce::String (melody.snapshot().toString()), nullptr);
    {
        const juce::SpinLock::ScopedLockType lock (songLock);
        state.setProperty (songProperty, juce::String (song.toString()), nullptr);
        state.setProperty (songTemplateProperty, songTemplate, nullptr);
        state.setProperty (songLengthProperty, songLength, nullptr);
    }
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
            lane.set (pc::progression::Snapshot::fromString (state[progressionProperty].toString().toStdString()));
            melody.set (pc::melody::Snapshot::fromString (state[melodyProperty].toString().toStdString()));
            {
                // State from before Song mode has no song: start from the Pop template
                const juce::SpinLock::ScopedLockType lock (songLock);
                songTemplate = juce::jlimit (0, pc::song::numTemplates - 1, (int) state.getProperty (songTemplateProperty, 0));
                songLength = juce::jmax (0, (int) state.getProperty (songLengthProperty, 0));
                song = state.hasProperty (songProperty) ? pc::song::Song::fromString (state[songProperty].toString().toStdString())
                                                        : pc::song::fromTemplate (songTemplate);
            }
            apvts.replaceState (state);
            flushRequested = true;
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PotatoChordsProcessor();
}
