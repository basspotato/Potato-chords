# Potato-Chords

A chord and melody sequencer for EDM and pop, as a VST3 plugin (JUCE 8, C++17). Pick a key, play or write a progression, add a melody, and send the MIDI to any instrument. A built-in preview sound lets you hear it without one.

## What it does

- **Chord pads** for the 7 chords of the key, with six chord types (triad, 7th, sus2, sus4, add9, power), close or wide voicing, and a Learn mode that shows each chord's mood and the three likeliest next chords.
- **Chord keys:** play one key on your MIDI keyboard (or click the on-screen keyboard) and get the whole chord.
- **Progression lane:** 8 bars, with a rhythm pattern, strum, swing, humanise and velocity. Plays with your DAW's transport.
- **Write a progression:** pick a style and mood, and it writes verse and chorus loops from a catalogue of common pop and EDM progressions. Complete fills only your empty bars.
- **Melody lane:** draw notes on a scale grid, or Write a melody in four shapes (Sing-along, Riff, Arp, Whoop). It plays on MIDI channel 2.
- **Song mode:** Pop, Pop-EDM and EDM club templates, fitted to a length and tempo. Drag any section, or the whole song, into your DAW as MIDI, or let Play song follow the transport section by section.
- **Song ideas:** 12 one-click starting points (key, chords, rhythm, melody, song shape and tempo).
- **Drag and drop MIDI** for the lane, the melody, a captured take and every song section.

## Using it in Cubase

Load it on an instrument track, then add a MIDI track whose input is Potato-Chords and route that track to your synth. Press play, and the chords and melody come out as MIDI.

## Building

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target PotatoChords_VST3
```

Every push is built on GitHub Actions; download the Windows VST3 from the run's Artifacts tab. See `CLAUDE.md` for the Linux build, tests and source layout.
