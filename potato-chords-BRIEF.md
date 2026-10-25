# Potato-Chords brief

A colourful VST3 for people who aren't professional musicians. Pick a key, tap chords, sketch a progression and a melody, hear them on a built-in preview synth, and drag the MIDI into the DAW. It's like Scaler 3, but with a fraction of the options and a display that teaches as you play.

Research and reasoning: [Potato-Chords research and plan](https://claude.ai/artifact/WeA5XRjuWHaWbtWpahzPBH). Decisions locked 2026-10-25 (all recommendations accepted; Basspotato will change things as we go).

## Scope v1

- **Scales:** Major and Natural minor only.
- **Chords:** the 7 chords in the key (one per scale degree). The only chord from outside the key is the major V in minor (e.g. E in A minor).
- **Flavours per chord:** Triad, 7th, sus2, sus4, add9, Power (5).
- **Voicing:** automatic voice leading (nearest inversion to the previous chord), plus one Spread control (close / wide).
- **One-key chords:** a "Chord keys" switch makes the white keys C–B in one octave play I–vii° in the chosen key.
- **Progression:** an 8-bar lane, up to 2 chords per bar, synced to the host transport, with starter presets: I–V–vi–IV, vi–IV–I–V, I–vi–IV–V, IV–V–vi–I, i–VI–III–VII, i–VI–iv–VII, i–VII–VI–VII, i–iv–VI–V.
- **Melody:** a lane that shows only the notes in the key. Notes in the current chord glow in that chord's colours.
- **Export:** drag either lane into the DAW (temporary .mid via `performExternalDragDropOfFiles`), Export .mid, and MIDI out. Test drag and drop in Cubase first. There is no clipboard paste.
- **Preview synth:** 16 voices, 4 fixed sounds (Keys, Pad, Pluck, Lead), separate sounds for chords and melody, a mute switch for MIDI-out use, and well under 5% of one core.

## Layout (one screen, no tabs)

From top to bottom:
1. **Top bar:** key, scale, preset, sound, sync/BPM, Export .mid.
2. **Chord pads:** the 7 chords, showing name, Roman numeral, mood word and colour stripes.
3. **Progression lane:** with a drag handle.
4. **Melody lane:** with a drag handle.
5. **Coloured keyboard:** chord notes light up.

## Teaching display

Every chord is shown as:
- its **name** (C, Am)
- its **Roman numeral** (uppercase = major, lowercase = minor)
- a **colour pattern** (one colour per note)
- its **keyboard shape**

It also has:
- mood words (home, lift, pull, sad)
- next-chord hints
- the progression shown as numbers, which stay the same across keys
- a Learn mode toggle that hides the extras

## Colours

Bright and vibrant, following Basspotato's palette (yellow → orange → pink → purple → blue → cyan). C = yellow and E = blue are the anchors, and the colours run in order going up the octave. Each note keeps its colour in every octave and every view. Note names always stay visible, so colour is never the only cue.

| Note | Colour | Hex |
| --- | --- | --- |
| C | yellow | `#FED031` |
| C# | orange | `#FDAF5B` |
| D | pink | `#FB5EA8` |
| D# | purple | `#BD5EDA` |
| E | blue | `#68A5DE` |
| F | sky | `#46C0DF` |
| F# | aqua | `#2FD8C8` |
| G | mint | `#3FE08F` |
| G# | green | `#6EE65A` |
| A | lime | `#B2EA3C` |
| A# | gold-lime | `#DCE036` |
| B | amber | `#FFC23A` |

C to F come straight from the palette. F# to B are bright greens in the same style, so the octave loops back to yellow (decision pending: the alternative is palette-only, with blue on A). The background stays dark (deep indigo) so the colours pop.

## Build phases

0. **Scaffold** (done)
1. Preview synth, chord pads, flavours, voicing, one-key chords
2. Colours, keyboard, teaching labels
3. Progression lane, host sync, presets, next-chord hints
4. Melody lane
5. Drag to DAW, Export .mid, MIDI out
6. Save/load, presets, beginner testing, first release

Use Opus for phases 1 and 3, and Sonnet for 2, 4 and 6.
