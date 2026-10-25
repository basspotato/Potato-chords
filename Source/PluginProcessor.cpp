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

    using namespace pc::params;
    keyParam       = apvts.getRawParameterValue (key.getParamID());
    scaleParam     = apvts.getRawParameterValue (scale.getParamID());
    flavourParam   = apvts.getRawParameterValue (flavour.getParamID());
    spreadParam    = apvts.getRawParameterValue (spread.getParamID());
    majorFiveParam = apvts.getRawParameterValue (majorFive.getParamID());
    chordKeysParam = apvts.getRawParameterValue (chordKeys.getParamID());
    soundParam     = apvts.getRawParameterValue (sound.getParamID());
    previewParam   = apvts.getRawParameterValue (preview.getParamID());

    out.ensureSize (8192);
}

void PotatoChordsProcessor::prepareToPlay (double sampleRate, int)
{
    synth.prepare (sampleRate);
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
    return s;
}

pc::theory::Chord PotatoChordsProcessor::chordFor (int degree) const
{
    const auto s = readSettings();
    return pc::theory::diatonicChord (s.key, (pc::theory::Scale) s.scale, degree, (pc::theory::Flavour) s.flavour, s.majorFive);
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
    playing = pc::voicing::applySpread (chord, previousClose, (pc::voicing::Spread) chordSettings.spread);
    for (int i = 0; i < playing.size; ++i)
        tracker.noteOn (out, chordChannel, playing.notes[(size_t) i], velocity, samplePos);
    currentDegree = degree;
    chordVelocity = velocity;
}

void PotatoChordsProcessor::stopChord (int samplePos)
{
    for (int i = 0; i < playing.size; ++i)
        tracker.noteOff (out, chordChannel, playing.notes[(size_t) i], samplePos);
    playing.size = 0;
}

void PotatoChordsProcessor::flushAll (int samplePos)
{
    tracker.allOff (out, samplePos);
    playing.size = 0;
    source = Source::none;
    currentDegree = -1;
    for (auto& ch : passHeld) ch.fill (false);
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
            playChord (degree, 100, 0);
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
            passHeld[(size_t) ch - 1][(size_t) note] = true;
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
            else if (passHeld[(size_t) ch - 1][(size_t) note])
            {
                tracker.noteOff (out, ch, note, pos);
                passHeld[(size_t) ch - 1][(size_t) note] = false;
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

void PotatoChordsProcessor::renderPreview (juce::AudioBuffer<float>& buffer)
{
    buffer.clear();
    const bool previewOn = previewParam->load() > 0.5f && buffer.getNumChannels() >= 2;
    if (! previewOn)
    {
        if (previewWasOn) synth.killAll();
        previewWasOn = false;
        return;
    }
    previewWasOn = true;
    synth.setSound ((pc::synth::Sound) (int) soundParam->load());

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
    uint64_t packed = ~uint64_t (0);
    for (int i = 0; i < playing.size && i < pc::voicing::maxNotes; ++i)
    {
        packed &= ~(uint64_t (0xff) << (8 * i));
        packed |= uint64_t (playing.notes[(size_t) i]) << (8 * i);
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
    handleMidi (midi, false, false);
    midi.swapWith (out);
    buffer.clear();
    synth.killAll();
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
            flushRequested = true;
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PotatoChordsProcessor();
}
