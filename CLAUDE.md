# Potato-Chords

VST3 chord sequencer plugin for EDM production: pick a key/scale, build a chord progression from a pattern of steps, and send the resulting MIDI to an instrument (or play it through an optional preview sound). JUCE 8, C++17, CMake. Plan: `potato-chords-BRIEF.md` (to be written after research). Research: `research/reports/`.

Targets: **Windows 11 x64 VST3 is the priority**. The developer tests in Cubase 15 on Windows 11 (Ryzen 9 9950X). macOS VST3/AU is later and must never block a build or PR.

## Model choice (save usage)

- Use Opus for research, architecture and hard debugging (the current phase).
- For routine tasks (small fixes, UI tweaks, preset edits, docs, tests) switch to a lower or different model such as Sonnet or Haiku with `/model` to save usage. Switch back to Opus when a task gets architectural or keeps failing.

## Working style (token efficiency)

- Do the task. Don't narrate steps or recap finished work; end with 1-2 sentences on the outcome.
- Simple change = minimal action. Don't re-read files you just edited or add verification nobody asked for.
- Stay in scope: no refactors, renames, reformatting, extra features, options, dependencies or files that weren't asked for.
- Comments only where the logic is non-obvious.
- When the target file is known, go straight to it. Use the source layout below instead of searching the repo.
- Ask only when blocked on a decision that changes the outcome; otherwise take the obvious default and say which.
- One feature per session. Keep MIDI/sequencer logic and GUI changes in separate sessions.
- Show a short plan before any big change.
- When debugging a build error, quote only the relevant error lines.

## Git

- Never commit to `main` directly. Work on a branch and open a PR; CI must be green before merge.
- No force-push, history rewrites or branch deletion without asking.

## Build

```bash
# Windows (Visual Studio 2022) — the target that matters
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target PotatoChords_VST3

# Linux (cloud sessions: compile check + headless tests; deps installed by .claude/hooks/session-start.sh)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPOTATO_DEV_TOOLS=ON
cmake --build build --target PotatoChords_VST3 PotatoChordsSnapshot PotatoChordsTests
```

- Local JUCE checkout: `-DJUCE_DIR=path/to/JUCE` (otherwise FetchContent downloads the pinned JUCE 8 tag).
- `COPY_PLUGIN_AFTER_BUILD` is off; output is in `build/PotatoChords_artefacts/Release/`.
- Releases come from GitHub Actions (`.github/workflows/build.yml`): download from the run's Artifacts tab.

## Test

- The gate is the Windows job: VST3 build plus pluginval at strictness 10, on every push and PR. A Linux job runs headless sequencer/theory tests. A macOS job, if added, is non-blocking.
- Linux: `xvfb-run -a /tmp/pluginval/pluginval --strictness-level 10 --validate-in-process --validate build/PotatoChords_artefacts/Release/VST3/Potato-Chords.vst3`.
- GUI check without a host: `xvfb-run -a build/PotatoChordsSnapshot shot.png [width] [pad]` renders the editor to a PNG (width 0 = default; pad 1-7 holds a chord pad). Look at it after any GUI change.
- Tests: `xvfb-run -a build/PotatoChordsTests` (chord/scale theory, voicing, step timing against host transport, note-off on stop/loop/tempo change, state). Add a test with every sequencer change; never weaken one to pass.

## Source layout (planned; update as it's built)

Built so far: Params, Theory, Voicing, MidiOut, PreviewSynth, gui/Colours, gui/Look, gui/Controls, gui/ChordPad, gui/Display, gui/Keyboard, gui/PotatoLogo.

```
Source/
  Params.h            — every APVTS parameter ID, range and default (single source of truth)
  Theory.h            — scales, modes, chord construction, diatonic degrees
  Voicing.h           — inversions, spread, voice leading, range limits
  Progression.h       — chord slots / progression model
  Sequencer.h         — step/pattern playback synced to host PPQ (rhythm, length, gate, swing, strum)
  MidiOut.h           — note scheduling with sample-accurate offsets, hanging-note protection
  Presets.h           — factory progressions/patterns
  PreviewSynth.h      — built-in 16-voice preview synth (Piano, Soft Piano, E-Piano, Pad, Pluck, Lead)
  gui/Colours.h       — the note-colour table and interface colours (the only place colours live)
  gui/Controls.h      — own-look parameter controls (ChoiceStrip, Switch)
  gui/Look.h          — 3D drawing helpers (textured background, raised/inset surfaces, glow, LEDs) and LookAndFeel
  gui/ChordPad.h      — one chord pad
  gui/Display.h       — glass now-playing display
  gui/Keyboard.h      — coloured keyboard that lights the sounding notes
  gui/PotatoLogo.h    — smiling potato emblem (from Potato-Kicker)
  gui/                — own look; replaceable later
  PluginProcessor.*   — audio/MIDI thread, parameter handling, state save/load
  PluginEditor.*      — GUI layout
tools/Snapshot.cpp    — headless editor-to-PNG renderer (POTATO_DEV_TOOLS)
tests/Tests.cpp       — headless tests (POTATO_DEV_TOOLS)
```

## Architecture rules

- **VST3 MIDI output:** VST3 has no pure "MIDI effect" type (JUCE's `IS_MIDI_EFFECT` is AU-only). Build as a VST3 instrument with MIDI in + MIDI out (and a stereo out bus for the optional preview). In Cubase, other tracks pick up its MIDI output as their input. Confirm this routing in research before locking the design.
- Timing comes from the host playhead (PPQ position, tempo, time signature, loop points). Every note-on/note-off is placed with a sample offset inside the block; never quantise to block boundaries.
- Never leave a hanging note: send note-offs on transport stop, loop wrap, tempo jump, bypass, pattern/chord change, and state load.
- Music theory lives in pure, allocation-free functions with no JUCE GUI dependency, so it's easy to test.
- **Parameters:** fixed count after load; IDs are permanent once released. Progression and pattern data live in state (ValueTree), not as hundreds of host parameters.
- **State** carries a version number. Loading older state must still work; add a migration rather than breaking it.
- Cubase is the reference host: keep a stable bus layout, report zero latency, never change latency on the fly.
- Real-time safety: no allocation, locks, logging or file I/O on the audio thread. GUI edits reach the audio thread lock-free (atomic swap or FIFO).
- Read parameters through cached `std::atomic<float>*` pointers, never by string lookup on the audio thread.
- Other plugins (Cthulhu, Scaler, etc.) are references for ideas only: re-implement, never copy code or assets.
- Presets are not exposed as host programs; MIDI program changes must never replace the user's progression.

## GUI conventions

- Own look, kept in its own files so it's easy to replace later.
- Bright, vibrant colours on a dark indigo background. Every note has one fixed colour (C = yellow, E = blue; full table in `potato-chords-BRIEF.md`), used on keys, pads, lanes and labels. Keep the note-colour table in one place in code and never hard-code a note colour elsewhere. Note names always stay visible.
- Main page = key/scale + chord slots + step pattern grid + transport/sync status. Detail controls (voicing, strum, humanise) on secondary panels.
- Repaint at a modest rate (about 30 Hz) and keep CPU low.

## Plugin identity

Manufacturer `Basspotato` / code `Bpot`, plugin code `PtCh`, product `Potato-Chords`, bundle ID `com.basspotato.potatochords`. Never change these after the first release.
