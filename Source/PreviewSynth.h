#pragma once
#include <array>
#include <cmath>
#include <cstdint>

// Small built-in synth, only for hearing chords without routing MIDI to another instrument.
// 16 voices, 75 fixed presets in 6 groups, no allocation after construction.
namespace pc::synth
{
    constexpr int numVoices = 16;
    constexpr float twoPi = 6.28318530718f;

    enum class Engine { additive, fm, subtractive };

    struct Preset
    {
        const char* name;
        const char* group;
        Engine engine;
        float attack, decay, sustain, release;  // seconds, level 0-1
        float cutoff, cutoffEnv, envDecay;      // Hz (0 = no filter), Hz added at the envelope peak, seconds.
                                                // The same envelope drives the FM depth.
        float detuneCents, width;               // second oscillator / string detune, stereo width 0-1
        float saw, square, sine;                // subtractive oscillator mix
        float ratio, index;                     // FM modulator ratio and depth
        int partials;                           // additive: number of sine partials
        float brightness;                       // additive: how fast upper partials fall away (0-1)
        float level;

        // Extras, used by the Song Writer sounds; the first 25 presets keep these defaults.
        // Vibrato also works on FM; everything else is subtractive only.
        const char* section = nullptr;          // heading inside the group's menu
        float resonance = 0.0f;                 // filter resonance 0-1
        float vibratoRate = 5.5f, vibratoDepth = 0.0f, vibratoDelay = 0.0f;  // Hz, cents, seconds to fade in
        float pulseWidth = 0.5f, pwmDepth = 0.0f;  // square becomes a pulse; the slow LFO swings its width
        float syncRatio = 0.0f, syncEnv = 0.0f; // hard-synced saw at this ratio to the note (0 = off), plus syncEnv at the envelope peak
        int unison = 0;                         // extra saws, 0-4
        float unisonDetune = 0.0f;              // cents
        float sub = 0.0f, noise = 0.0f, drive = 0.0f;
        float modRate = 0.3f, filterLfo = 0.0f; // slow LFO in Hz (pulse width and filter), cutoff swing in Hz

        constexpr Preset res (float r) const                                { auto p = *this; p.resonance = r; return p; }
        constexpr Preset vib (float rate, float cents, float delay = 0.0f) const { auto p = *this; p.vibratoRate = rate; p.vibratoDepth = cents; p.vibratoDelay = delay; return p; }
        constexpr Preset pw (float w, float depth = 0.0f, float rate = 0.3f) const { auto p = *this; p.pulseWidth = w; p.pwmDepth = depth; p.modRate = rate; return p; }
        constexpr Preset sync (float r, float env = 0.0f) const             { auto p = *this; p.syncRatio = r; p.syncEnv = env; return p; }
        constexpr Preset uni (int n, float cents) const                     { auto p = *this; p.unison = n; p.unisonDetune = cents; return p; }
        constexpr Preset subOsc (float amount) const                        { auto p = *this; p.sub = amount; return p; }
        constexpr Preset noisy (float amount) const                         { auto p = *this; p.noise = amount; return p; }
        constexpr Preset driven (float amount) const                        { auto p = *this; p.drive = amount; return p; }
        constexpr Preset wobble (float rate, float hz) const                { auto p = *this; p.modRate = rate; p.filterLfo = hz; return p; }
    };

    namespace build
    {
        constexpr const char* songWriter = "Song Writer";
        constexpr const char* leads = "Leads";
        constexpr const char* basses = "Basses";
        constexpr const char* pads = "Pads";
        constexpr const char* keys = "Keys & stabs";
        constexpr const char* plucks = "Plucks";
        constexpr const char* fx = "FX";

        constexpr Preset va (const char* name, const char* section, float a, float d, float s, float r, float cutoff, float cutoffEnv, float envDecay,
                             float detune, float width, float saw, float square, float sine, float level)
        {
            Preset p { name, songWriter, Engine::subtractive, a, d, s, r, cutoff, cutoffEnv, envDecay, detune, width, saw, square, sine, 0, 0, 0, 0, level };
            p.section = section;
            return p;
        }

        constexpr Preset fm (const char* name, const char* section, float a, float d, float s, float r, float envDecay,
                             float detune, float width, float ratio, float index, float level)
        {
            Preset p { name, songWriter, Engine::fm, a, d, s, r, 0, 0, envDecay, detune, width, 0, 0, 0, ratio, index, 0, 0, level };
            p.section = section;
            return p;
        }
    }

    using E = Engine;
    using namespace build;
    //                       name                group      engine          A       D      S      R      cutoff   env     envDec  det    width  saw   sq    sine  ratio  index  parts bright level
    inline constexpr std::array<Preset, 75> presets {{
        { "Grand Piano",      "Piano",   E::additive,    0.001f, 4.0f,  0.0f,  0.30f, 0.0f,    0.0f,   0.0f,  0.8f,  0.6f,  0, 0, 0, 0, 0,     7, 0.60f, 1.0f },
        { "Soft Felt Piano",  "Piano",   E::additive,    0.006f, 3.0f,  0.0f,  0.50f, 0.0f,    0.0f,   0.0f,  0.5f,  0.5f,  0, 0, 0, 0, 0,     4, 0.30f, 1.1f },
        { "Bright Upright",   "Piano",   E::additive,    0.001f, 3.0f,  0.0f,  0.25f, 0.0f,    0.0f,   0.0f,  2.5f,  0.7f,  0, 0, 0, 0, 0,     8, 0.75f, 0.9f },
        { "Dark Concert",     "Piano",   E::additive,    0.002f, 5.0f,  0.0f,  0.60f, 0.0f,    0.0f,   0.0f,  0.6f,  0.6f,  0, 0, 0, 0, 0,     6, 0.42f, 1.0f },
        { "Lo-fi Piano",      "Piano",   E::additive,    0.003f, 2.2f,  0.0f,  0.25f, 2200.0f, 0.0f,   0.0f,  6.0f,  0.4f,  0, 0, 0, 0, 0,     5, 0.50f, 1.1f },

        { "Classic EP",       "E-Piano", E::fm,          0.002f, 1.6f,  0.25f, 0.35f, 0.0f,    0.0f,   0.6f,  0.0f,  0.0f,  0, 0, 0, 1.0f,  1.3f,  0, 0, 1.0f },
        { "Soft Rhodes",      "E-Piano", E::fm,          0.004f, 2.0f,  0.30f, 0.45f, 0.0f,    0.0f,   0.8f,  3.0f,  0.5f,  0, 0, 0, 1.0f,  0.7f,  0, 0, 1.1f },
        { "Bell EP",          "E-Piano", E::fm,          0.001f, 1.8f,  0.15f, 0.40f, 0.0f,    0.0f,   0.35f, 2.0f,  0.4f,  0, 0, 0, 7.0f,  0.8f,  0, 0, 0.9f },
        { "Tine Keys",        "E-Piano", E::fm,          0.001f, 1.4f,  0.20f, 0.30f, 0.0f,    0.0f,   0.15f, 0.0f,  0.0f,  0, 0, 0, 14.0f, 1.0f,  0, 0, 0.9f },
        { "Wurly",            "E-Piano", E::fm,          0.002f, 1.2f,  0.45f, 0.25f, 3500.0f, 0.0f,   0.4f,  0.0f,  0.0f,  0, 0, 0, 1.0f,  2.2f,  0, 0, 0.8f },

        { "Warm Pad",         "Pad",     E::subtractive, 0.35f,  1.5f,  0.80f, 1.20f, 1400.0f, 900.0f, 1.2f,  7.0f,  0.7f,  1.0f, 0, 0, 0, 0, 0, 0, 0.7f },
        { "Supersaw Pad",     "Pad",     E::subtractive, 0.25f,  2.0f,  0.85f, 1.00f, 3800.0f, 800.0f, 1.0f,  18.0f, 1.0f,  1.0f, 0, 0, 0, 0, 0, 0, 0.55f },
        { "Dark Pad",         "Pad",     E::subtractive, 0.60f,  2.0f,  0.85f, 1.50f, 650.0f,  300.0f, 1.5f,  9.0f,  0.8f,  1.0f, 0.4f, 0, 0, 0, 0, 0, 0.9f },
        { "Glass Pad",        "Pad",     E::subtractive, 0.80f,  2.5f,  0.70f, 1.80f, 2600.0f, 0.0f,   0.0f,  6.0f,  0.9f,  0.3f, 0.6f, 0.4f, 0, 0, 0, 0, 0.7f },
        { "String Pad",       "Pad",     E::subtractive, 0.22f,  1.0f,  0.90f, 0.80f, 2100.0f, 500.0f, 0.6f,  11.0f, 0.8f,  1.0f, 0, 0, 0, 0, 0, 0, 0.65f },

        { "Classic Pluck",    "Pluck",   E::subtractive, 0.001f, 0.45f, 0.0f,  0.25f, 300.0f,  5000.0f, 0.18f, 0.0f, 0.0f,  1.0f, 0, 0, 0, 0, 0, 0, 1.0f },
        { "Bright Pluck",     "Pluck",   E::subtractive, 0.001f, 0.35f, 0.0f,  0.20f, 600.0f,  8000.0f, 0.12f, 5.0f, 0.5f,  1.0f, 0, 0, 0, 0, 0, 0, 0.85f },
        { "Future Pluck",     "Pluck",   E::subtractive, 0.001f, 0.50f, 0.0f,  0.30f, 400.0f,  6000.0f, 0.22f, 16.0f, 1.0f, 1.0f, 0, 0, 0, 0, 0, 0, 0.7f },
        { "Soft Pluck",       "Pluck",   E::subtractive, 0.002f, 0.60f, 0.0f,  0.35f, 250.0f,  2200.0f, 0.25f, 0.0f, 0.0f,  0.0f, 0.7f, 0.5f, 0, 0, 0, 0, 1.0f },
        { "Kalimba",          "Pluck",   E::fm,          0.001f, 0.70f, 0.0f,  0.30f, 0.0f,    0.0f,   0.08f, 0.0f,  0.0f,  0, 0, 0, 4.0f,  1.5f,  0, 0, 1.0f },

        { "Saw Lead",         "Lead",    E::subtractive, 0.005f, 0.30f, 0.85f, 0.18f, 3500.0f, 1500.0f, 0.3f, 5.0f,  0.3f,  1.0f, 0, 0, 0, 0, 0, 0, 0.75f },
        { "Square Lead",      "Lead",    E::subtractive, 0.005f, 0.30f, 0.85f, 0.18f, 2800.0f, 1200.0f, 0.3f, 3.0f,  0.2f,  0.0f, 1.0f, 0, 0, 0, 0, 0, 0.7f },
        { "Bright Lead",      "Lead",    E::subtractive, 0.003f, 0.25f, 0.80f, 0.15f, 6000.0f, 2500.0f, 0.2f, 7.0f,  0.4f,  0.8f, 0.6f, 0, 0, 0, 0, 0, 0.6f },
        { "Soft Sine Lead",   "Lead",    E::subtractive, 0.010f, 0.40f, 0.90f, 0.25f, 1800.0f, 400.0f, 0.4f,  2.0f,  0.2f,  0.2f, 0, 1.0f, 0, 0, 0, 0, 1.1f },
        { "Supersaw Lead",    "Lead",    E::subtractive, 0.004f, 0.30f, 0.85f, 0.20f, 5000.0f, 2000.0f, 0.3f, 20.0f, 1.0f, 1.0f, 0, 0, 0, 0, 0, 0, 0.5f },

        // Song Writer: own takes on classic virtual-analogue sounds
        //   name                    section  A       D      S      R      cutoff   env      envDec det    width  saw   sq    sine  level
        va ("Sync Scream",           leads,   0.003f, 0.40f, 0.85f, 0.20f, 5000.0f, 2000.0f, 0.40f, 4.0f,  0.3f,  1.0f, 0,    0,    0.6f).sync (2.0f, 3.0f).vib (5.5f, 15.0f, 0.4f).driven (0.3f),
        va ("Hollow Square",         leads,   0.004f, 0.30f, 0.85f, 0.20f, 3000.0f, 1500.0f, 0.30f, 3.0f,  0.2f,  0,    1.0f, 0,    0.42f).pw (0.35f, 0.1f, 0.5f).vib (5.0f, 12.0f, 0.3f),
        va ("Euro Lead",             leads,   0.003f, 0.30f, 0.85f, 0.20f, 4500.0f, 2500.0f, 0.25f, 6.0f,  0.5f,  1.0f, 0,    0,    0.72f).uni (3, 18.0f).vib (6.0f, 10.0f, 0.5f),
        va ("Mono Saw Solo",         leads,   0.003f, 0.35f, 0.80f, 0.18f, 2200.0f, 3000.0f, 0.35f, 3.0f,  0.2f,  1.0f, 0,    0,    0.6f).subOsc (0.3f).res (0.4f).driven (0.4f).vib (5.5f, 14.0f, 0.35f),
        va ("Whistle Lead",          leads,   0.020f, 0.30f, 0.90f, 0.20f, 4000.0f, 0.0f,    0.0f,  2.0f,  0.2f,  0.1f, 0,    1.0f, 0.55f).vib (6.0f, 18.0f, 0.25f),
        va ("Thin Pulse",            leads,   0.003f, 0.30f, 0.85f, 0.18f, 5000.0f, 1000.0f, 0.30f, 3.0f,  0.3f,  0,    1.0f, 0,    0.6f).pw (0.15f, 0.05f, 0.4f).vib (5.5f, 10.0f, 0.3f),
        va ("Chip Lead",             leads,   0.002f, 0.20f, 0.80f, 0.10f, 0.0f,    0.0f,    0.0f,  0.0f,  0.0f,  0,    1.0f, 0,    0.55f).pw (0.25f).vib (7.0f, 20.0f, 0.15f),
        va ("Sync Sweep",            leads,   0.004f, 0.50f, 0.85f, 0.25f, 6000.0f, 0.0f,    1.20f, 4.0f,  0.4f,  1.0f, 0,    0,    0.55f).sync (1.5f, 5.0f),
        va ("Fat Detune Lead",       leads,   0.004f, 0.35f, 0.85f, 0.20f, 3000.0f, 2000.0f, 0.30f, 12.0f, 0.6f,  0.8f, 0.4f, 0,    1.2f).res (0.3f).driven (0.2f),
        va ("Soft Flute",            leads,   0.060f, 0.40f, 0.90f, 0.25f, 2500.0f, 0.0f,    0.0f,  2.0f,  0.2f,  0,    0,    0.8f, 0.63f).noisy (0.15f).vib (4.8f, 14.0f, 0.5f),
        va ("Acid Lead",             leads,   0.003f, 0.30f, 0.70f, 0.15f, 500.0f,  4000.0f, 0.25f, 0.0f,  0.0f,  1.0f, 0,    0,    0.6f).res (0.7f).driven (0.5f),
        va ("Brass Lead",            leads,   0.030f, 0.50f, 0.80f, 0.25f, 1200.0f, 3000.0f, 0.35f, 5.0f,  0.3f,  1.0f, 0,    0,    0.7f).res (0.2f).vib (5.0f, 8.0f, 0.6f),
        va ("Screamer",              leads,   0.003f, 0.30f, 0.85f, 0.18f, 3500.0f, 2500.0f, 0.30f, 5.0f,  0.3f,  0.7f, 0.5f, 0,    0.58f).sync (2.5f, 1.5f).driven (0.8f).res (0.35f),
        va ("Pure Tone Lead",        leads,   0.010f, 0.40f, 0.90f, 0.25f, 1800.0f, 0.0f,    0.0f,  2.0f,  0.2f,  0,    0.2f, 1.0f, 0.42f).vib (5.5f, 10.0f, 0.4f),
        va ("Wide Supersaw",         leads,   0.004f, 0.30f, 0.85f, 0.25f, 6000.0f, 2000.0f, 0.30f, 10.0f, 1.0f,  1.0f, 0,    0,    0.77f).uni (4, 25.0f),

        va ("Rubber Bass",           basses,  0.002f, 0.30f, 0.50f, 0.10f, 300.0f,  1500.0f, 0.20f, 0.0f,  0.0f,  0,    1.0f, 0,    0.9f).pw (0.4f).subOsc (0.6f).res (0.4f),
        va ("Saw Bass",              basses,  0.002f, 0.30f, 0.60f, 0.10f, 600.0f,  2500.0f, 0.15f, 3.0f,  0.1f,  1.0f, 0,    0,    0.8f).subOsc (0.5f).res (0.3f).driven (0.3f),
        va ("Deep Sub",              basses,  0.003f, 0.50f, 0.90f, 0.10f, 400.0f,  0.0f,    0.0f,  0.0f,  0.0f,  0.1f, 0,    0.8f, 1.4f),
        va ("Acid Bass",             basses,  0.002f, 0.25f, 0.40f, 0.10f, 250.0f,  3500.0f, 0.18f, 0.0f,  0.0f,  1.0f, 0,    0,    0.75f).res (0.75f).driven (0.4f),
        va ("Pluck Bass",            basses,  0.001f, 0.35f, 0.0f,  0.12f, 400.0f,  3500.0f, 0.15f, 2.0f,  0.1f,  0.6f, 0.4f, 0,    3.5f),
        va ("Reese Bass",            basses,  0.005f, 0.50f, 0.90f, 0.15f, 700.0f,  0.0f,    0.0f,  25.0f, 0.5f,  1.0f, 0,    0,    0.65f).uni (2, 15.0f).driven (0.3f).wobble (0.25f, 300.0f),
        va ("Sync Bass",             basses,  0.002f, 0.30f, 0.60f, 0.10f, 1200.0f, 0.0f,    0.20f, 2.0f,  0.1f,  1.0f, 0,    0,    0.85f).sync (1.8f, 2.0f).subOsc (0.4f),
        va ("Wobble Bass",           basses,  0.003f, 0.40f, 0.90f, 0.12f, 500.0f,  0.0f,    0.0f,  4.0f,  0.2f,  1.0f, 0.5f, 0,    0.7f).res (0.5f).wobble (2.0f, 450.0f).subOsc (0.5f).driven (0.4f),

        va ("PWM Strings",           pads,    0.40f,  1.50f, 0.85f, 1.20f, 2400.0f, 300.0f,  1.00f, 6.0f,  0.8f,  0,    1.0f, 0,    0.5f).pw (0.5f, 0.35f, 0.6f),
        va ("Analog Choir",          pads,    0.60f,  2.00f, 0.85f, 1.60f, 1600.0f, 0.0f,    0.0f,  7.0f,  0.8f,  0.4f, 0.4f, 0,    2.0f).pw (0.3f, 0.2f, 0.4f).noisy (0.05f).vib (4.5f, 6.0f),
        va ("Sweep Pad",             pads,    0.80f,  2.00f, 0.85f, 2.00f, 500.0f,  0.0f,    0.0f,  8.0f,  0.9f,  1.0f, 0,    0,    1.1f).uni (2, 12.0f).res (0.45f).wobble (0.12f, 900.0f),
        va ("Dream Pad",             pads,    1.20f,  2.50f, 0.80f, 2.50f, 1800.0f, 600.0f,  2.00f, 9.0f,  1.0f,  0.5f, 0,    0.5f, 2.3f).vib (4.0f, 5.0f),
        va ("Trance Pad",            pads,    0.30f,  2.00f, 0.90f, 1.50f, 3000.0f, 800.0f,  1.50f, 10.0f, 1.0f,  1.0f, 0,    0,    0.72f).uni (4, 22.0f),
        va ("Soft Brass",            pads,    0.25f,  1.50f, 0.80f, 0.90f, 900.0f,  1500.0f, 0.80f, 5.0f,  0.6f,  1.0f, 0,    0,    0.7f).res (0.15f),
        va ("Air Pad",               pads,    1.00f,  2.00f, 0.85f, 2.00f, 3000.0f, 0.0f,    0.0f,  6.0f,  0.9f,  0,    0,    0.5f, 1.1f).noisy (0.25f).wobble (0.2f, 1500.0f),
        va ("Dark Drone",            pads,    1.00f,  2.00f, 0.90f, 2.00f, 400.0f,  0.0f,    0.0f,  8.0f,  0.7f,  1.0f, 0.5f, 0,    1.2f).subOsc (0.4f).res (0.3f).wobble (0.1f, 200.0f),

        va ("Rave Stab",             keys,    0.001f, 0.40f, 0.0f,  0.15f, 800.0f,  5000.0f, 0.15f, 8.0f,  0.8f,  1.0f, 0,    0,    1.1f).uni (3, 20.0f).driven (0.2f),
        fm ("House Organ",           keys,    0.003f, 0.20f, 0.90f, 0.08f, 0.0f,                     0.0f,  0.0f,  2.0f,  0.5f,  0.45f),
        va ("Clav Keys",             keys,    0.001f, 0.50f, 0.20f, 0.10f, 1500.0f, 3000.0f, 0.12f, 2.0f,  0.2f,  0,    1.0f, 0,    0.7f).pw (0.2f).res (0.25f),
        va ("Chord Stab",            keys,    0.001f, 0.30f, 0.0f,  0.20f, 1000.0f, 4000.0f, 0.12f, 8.0f,  0.7f,  1.0f, 0.5f, 0,    2.3f),
        va ("Poly Brass Stab",       keys,    0.015f, 0.50f, 0.60f, 0.25f, 600.0f,  3500.0f, 0.25f, 6.0f,  0.5f,  1.0f, 0,    0,    0.7f),
        fm ("FM Bell Keys",          keys,    0.001f, 2.00f, 0.10f, 0.60f, 0.60f,                    2.0f,  0.4f,  3.5f,  1.2f,  0.8f),
        fm ("Digital Keys",          keys,    0.002f, 1.20f, 0.30f, 0.40f, 0.30f,                    3.0f,  0.5f,  1.0f,  2.0f,  0.8f),

        va ("Pulse Pluck",           plucks,  0.001f, 0.40f, 0.0f,  0.25f, 300.0f,  6000.0f, 0.15f, 3.0f,  0.4f,  0,    1.0f, 0,    0.9f).pw (0.3f),
        va ("Sync Pluck",            plucks,  0.001f, 0.35f, 0.0f,  0.25f, 4000.0f, 2000.0f, 0.15f, 4.0f,  0.4f,  1.0f, 0,    0,    1.05f).sync (2.0f, 3.0f),
        va ("Wood Pluck",            plucks,  0.001f, 0.30f, 0.0f,  0.20f, 400.0f,  4000.0f, 0.05f, 0.0f,  0.0f,  0.2f, 0,    1.0f, 1.8f).noisy (0.15f),
        va ("Supersaw Pluck",        plucks,  0.001f, 0.50f, 0.0f,  0.30f, 500.0f,  7000.0f, 0.20f, 10.0f, 1.0f,  1.0f, 0,    0,    1.2f).uni (4, 20.0f),
        va ("Resonant Pluck",        plucks,  0.001f, 0.45f, 0.0f,  0.25f, 300.0f,  5000.0f, 0.20f, 3.0f,  0.3f,  1.0f, 0,    0,    0.7f).res (0.6f),
        fm ("Harp Pluck",            plucks,  0.001f, 1.20f, 0.0f,  0.50f, 0.20f,                    3.0f,  0.5f,  2.0f,  0.9f,  0.9f),
        va ("Square Pluck",          plucks,  0.001f, 0.30f, 0.0f,  0.20f, 400.0f,  3500.0f, 0.12f, 2.0f,  0.2f,  0,    1.0f, 0,    0.65f).subOsc (0.3f),

        va ("Laser Zap",             fx,      0.001f, 0.30f, 0.0f,  0.10f, 0.0f,    0.0f,    0.08f, 0.0f,  0.0f,  1.0f, 0,    0,    0.65f).sync (1.0f, 8.0f),
        va ("Noise Riser",           fx,      2.00f,  1.00f, 1.0f,  1.00f, 8000.0f, -7600.0f, 3.0f, 6.0f,  0.6f,  0.2f, 0,    0,    1.25f).noisy (0.8f),
        va ("Siren",                 fx,      0.010f, 0.50f, 0.90f, 0.30f, 3000.0f, 0.0f,    0.0f,  0.0f,  0.0f,  0,    0.5f, 0.5f, 0.6f).vib (0.8f, 500.0f),
        va ("Wind",                  fx,      0.50f,  1.00f, 0.90f, 1.50f, 800.0f,  0.0f,    0.0f,  0.0f,  0.0f,  0,    0,    0,    2.7f).noisy (1.0f).res (0.6f).wobble (0.15f, 600.0f),
        fm ("Metal Bleep",           fx,      0.001f, 0.25f, 0.0f,  0.10f, 0.10f,                    0.0f,  0.0f,  3.7f,  3.0f,  0.6f),
    }};

    constexpr int numPresets = (int) presets.size();

    namespace detail
    {
        constexpr int sineSize = 2048;

        inline const std::array<float, sineSize + 1>& sineTable()
        {
            static const auto table = []
            {
                std::array<float, sineSize + 1> t {};
                for (int i = 0; i <= sineSize; ++i)
                    t[(size_t) i] = std::sin (twoPi * (float) i / (float) sineSize);
                return t;
            }();
            return table;
        }

        inline float sine (float phase)  // phase in cycles, any value
        {
            phase -= std::floor (phase);
            const float pos = phase * (float) sineSize;
            const int i = (int) pos;
            const auto& t = sineTable();
            return t[(size_t) i] + (t[(size_t) i + 1] - t[(size_t) i]) * (pos - (float) i);
        }

        inline float polyBlep (float t, float dt)
        {
            if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
            if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
            return 0.0f;
        }

        inline float saw (float t, float dt) { return 2.0f * t - 1.0f - polyBlep (t, dt); }

        // Pulse with width w (0.5 = square), DC removed
        inline float pulse (float t, float dt, float w)
        {
            float t2 = t + 1.0f - w;
            if (t2 >= 1.0f) t2 -= 1.0f;
            return (t < w ? 1.0f : -1.0f) + polyBlep (t, dt) - polyBlep (t2, dt) - (2.0f * w - 1.0f);
        }

        // Soft clip that reaches 1 at x = 3
        inline float softClip (float x)
        {
            x = std::fmax (-3.0f, std::fmin (3.0f, x));
            return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
        }

        // Topology-preserving state-variable low-pass
        struct LowPass
        {
            float ic1 = 0.0f, ic2 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

            void setCutoff (float hz, float sampleRate, float resonance)
            {
                const float g = std::tan (3.14159265f * std::fmin (std::fmax (hz, 20.0f), sampleRate * 0.45f) / sampleRate);
                const float k = 1.2f - 1.1f * resonance;
                a1 = 1.0f / (1.0f + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
            }

            float process (float x)
            {
                const float v3 = x - ic2;
                const float v1 = a1 * ic1 + a2 * v3;
                const float v2 = ic2 + a2 * ic1 + a3 * v3;
                ic1 = 2.0f * v1 - ic1;
                ic2 = 2.0f * v2 - ic2;
                return v2;
            }

            void reset() { ic1 = ic2 = 0.0f; }
        };
    }

    class Voice
    {
    public:
        static constexpr int maxPartials = 8;
        static constexpr int maxUnison = 4;

        enum class Stage { idle, attack, decay, sustain, release };

        void start (int ch, int midiNote, float velocity, const Preset& p, float sampleRate, uint32_t order)
        {
            channel = ch;
            note = midiNote;
            set = &p;
            sr = sampleRate;
            startOrder = order;
            gain = 0.16f * p.level * (0.3f + 0.7f * velocity);
            const float hz = 440.0f * std::pow (2.0f, (float) (midiNote - 69) / 12.0f);
            const float detune = std::pow (2.0f, p.detuneCents / 1200.0f);
            inc1 = hz / sr * detune;
            inc2 = hz / sr / detune;
            phase1 = 0.0f;
            phase2 = 0.37f;
            phaseMod = 0.0f;
            level = 0.0f;
            env = 1.0f;
            envCoef = p.envDecay > 0.0f ? std::exp (-1.0f / (p.envDecay * 0.25f * sr)) : 1.0f;
            attackStep = 1.0f / std::fmax (1.0f, p.attack * sr);
            decayCoef = std::exp (-1.0f / (p.decay * 0.25f * sr));
            releaseCoef = std::exp (-1.0f / (p.release * 0.25f * sr));
            cross = 1.0f - 0.5f * p.width;
            pitch = 1.0f;
            vibPhase = 0.0f;
            modPhase = 0.25f * (float) (order & 3);  // voices of a chord swing out of step
            vibRampStep = p.vibratoDelay > 0.0f ? 16.0f / (p.vibratoDelay * sr) : 1.0f;
            vibRamp = p.vibratoDelay > 0.0f ? 0.0f : 1.0f;
            slave1 = slave2 = subPhase = 0.0f;
            noiseState = 0x9E3779B9u ^ (order * 2654435761u);
            for (int j = 0; j < maxUnison; ++j)
            {
                const float spread = j < p.unison ? p.unisonDetune * unisonSpread[(size_t) j] : 0.0f;
                uniInc[(size_t) j] = hz / sr * std::pow (2.0f, spread / 1200.0f);
                uniPhase[(size_t) j] = 0.13f + 0.29f * (float) j;
            }
            mixNorm = 1.0f / std::sqrt (1.0f + 0.5f * (float) p.unison);

            // Additive piano: sine partials with slight string stiffness (stretched overtones). Harder notes
            // are brighter, and upper partials die away faster, like a real string.
            const float bright = p.brightness * (0.6f + 0.6f * velocity);
            for (int k = 0; k < maxPartials; ++k)
            {
                const float n = (float) (k + 1);
                const bool used = p.engine == Engine::additive && k < p.partials;
                partialInc[(size_t) k] = used ? hz * n * std::sqrt (1.0f + 0.0004f * n * n) / sr : 0.0f;
                partialAmp[(size_t) k] = used && partialInc[(size_t) k] < 0.45f ? std::pow (bright, n - 1.0f) / n : 0.0f;
                partialCoef[(size_t) k] = std::exp (-(0.4f + 0.8f * n) / sr);
                partialPhase[(size_t) k] = 0.0f;
            }
            filterL.reset();
            filterR.reset();
            stage = Stage::attack;
            counter = 0;
        }

        void release() { if (stage != Stage::idle) stage = Stage::release; }
        void kill()    { stage = Stage::idle; }

        bool isActive() const    { return stage != Stage::idle; }
        bool isReleased() const  { return stage == Stage::release; }
        bool plays (int ch, int n) const { return isActive() && ! isReleased() && channel == ch && note == n; }
        float currentLevel() const { return level; }
        uint32_t order() const   { return startOrder; }

        void render (float* left, float* right, int num)
        {
            const auto& p = *set;
            for (int i = 0; i < num && stage != Stage::idle; ++i)
            {
                if ((counter++ & 15) == 0) control (p);

                float l = 0.0f, r = 0.0f;
                switch (p.engine)
                {
                    case Engine::additive:
                    {
                        float sum = 0.0f;
                        for (int k = 0; k < p.partials; ++k)
                        {
                            sum += partialAmp[(size_t) k] * detail::sine (partialPhase[(size_t) k]);
                            partialPhase[(size_t) k] += partialInc[(size_t) k];
                            if (partialPhase[(size_t) k] >= 1.0f) partialPhase[(size_t) k] -= 1.0f;
                            partialAmp[(size_t) k] *= partialCoef[(size_t) k];
                        }
                        // A second, slightly detuned string on the fundamental gives the stereo shimmer
                        const float second = 0.35f * p.width * partialAmp[0] * detail::sine (phase2);
                        l = sum + second;
                        r = sum - second;
                        break;
                    }
                    case Engine::fm:
                    {
                        // Two operators with a decaying index; the detuned copy spreads it in stereo
                        const float index = p.index * env;
                        const float a = detail::sine (phase1 + index * detail::sine (phaseMod));
                        const float b = p.width > 0.0f ? detail::sine (phase2 + index * detail::sine (phaseMod)) : a;
                        l = a + (b - a) * 0.5f * p.width;
                        r = b + (a - b) * 0.5f * p.width;
                        break;
                    }
                    case Engine::subtractive:
                    {
                        const float i1 = inc1 * pitch, i2 = inc2 * pitch;
                        const bool synced = p.syncRatio > 0.0f;
                        const float saw1 = synced ? detail::saw (slave1, i1 * slaveRatio) : detail::saw (phase1, i1);
                        const float saw2 = synced ? detail::saw (slave2, i2 * slaveRatio) : detail::saw (phase2, i2);
                        const float a = p.saw * saw1 + p.square * detail::pulse (phase1, i1, width) + p.sine * detail::sine (phase1);
                        const float b = p.saw * saw2 + p.square * detail::pulse (phase2, i2, width) + p.sine * detail::sine (phase2);
                        l = (a + cross * b) * 0.6f;
                        r = (b + cross * a) * 0.6f;

                        // Unison saws: the up-tuned ones lean left, the down-tuned ones right
                        for (int j = 0; j < p.unison; ++j)
                        {
                            const float u = 0.7f * p.saw * detail::saw (uniPhase[(size_t) j], uniInc[(size_t) j] * pitch);
                            const bool leansLeft = (j & 1) == 0;
                            l += u * (leansLeft ? 0.8f : 0.3f);
                            r += u * (leansLeft ? 0.3f : 0.8f);
                            uniPhase[(size_t) j] += uniInc[(size_t) j] * pitch;
                            if (uniPhase[(size_t) j] >= 1.0f) uniPhase[(size_t) j] -= 1.0f;
                        }
                        l *= mixNorm;
                        r *= mixNorm;

                        if (p.sub > 0.0f)
                        {
                            const float s = p.sub * 0.6f * detail::pulse (subPhase, i1 * 0.5f, 0.5f);
                            l += s;
                            r += s;
                            subPhase += i1 * 0.5f;
                            if (subPhase >= 1.0f) subPhase -= 1.0f;
                        }
                        if (p.noise > 0.0f)
                        {
                            noiseState = noiseState * 1664525u + 1013904223u;
                            const float n = p.noise * 0.6f * ((float) (int32_t) noiseState * 4.656612873e-10f);
                            l += n;
                            r += n;
                        }
                        if (p.drive > 0.0f)
                        {
                            const float in = 1.0f + 4.0f * p.drive, out = 1.0f / (1.0f + p.drive);
                            l = detail::softClip (l * in) * out;
                            r = detail::softClip (r * in) * out;
                        }

                        if (synced)
                        {
                            slave1 += i1 * slaveRatio; slave1 -= std::floor (slave1);
                            slave2 += i2 * slaveRatio; slave2 -= std::floor (slave2);
                            // The slave restarts each time the note's own cycle does
                            if (phase1 + i1 >= 1.0f) slave1 = (phase1 + i1 - 1.0f) * slaveRatio;
                            if (phase2 + i2 >= 1.0f) slave2 = (phase2 + i2 - 1.0f) * slaveRatio;
                        }
                        break;
                    }
                }

                if (p.cutoff > 0.0f)
                {
                    l = filterL.process (l);
                    r = filterR.process (r);
                }

                phase1 += inc1 * pitch; if (phase1 >= 1.0f) phase1 -= 1.0f;
                phase2 += inc2 * pitch; if (phase2 >= 1.0f) phase2 -= 1.0f;
                phaseMod += inc1 * pitch * p.ratio; phaseMod -= std::floor (phaseMod);
                env *= envCoef;

                stepEnvelope();
                const float g = gain * level;
                left[i] += l * g;
                right[i] += r * g;
            }
        }

        int channel = 1, note = -1;

    private:
        static constexpr std::array<float, maxUnison> unisonSpread { 1.0f, -1.0f, 0.5f, -0.5f };

        // Every 16 samples: vibrato, the slow LFO, sync amount and the filter
        void control (const Preset& p)
        {
            if (p.vibratoDepth > 0.0f)
            {
                vibPhase += 16.0f * p.vibratoRate / sr;
                vibPhase -= std::floor (vibPhase);
                vibRamp = std::fmin (1.0f, vibRamp + vibRampStep);
                pitch = std::exp2 (p.vibratoDepth * vibRamp * detail::sine (vibPhase) / 1200.0f);
            }
            modPhase += 16.0f * p.modRate / sr;
            modPhase -= std::floor (modPhase);
            const float lfo = detail::sine (modPhase);
            width = std::fmax (0.05f, std::fmin (0.95f, p.pulseWidth + p.pwmDepth * lfo));
            slaveRatio = p.syncRatio + p.syncEnv * env;
            if (p.cutoff > 0.0f)
            {
                const float fc = p.cutoff + p.cutoffEnv * env + p.filterLfo * lfo;
                filterL.setCutoff (fc, sr, p.resonance);
                filterR.setCutoff (fc, sr, p.resonance);
            }
        }

        void stepEnvelope()
        {
            switch (stage)
            {
                case Stage::attack:
                    level += attackStep;
                    if (level >= 1.0f) { level = 1.0f; stage = Stage::decay; }
                    break;
                case Stage::decay:
                    level = set->sustain + (level - set->sustain) * decayCoef;
                    if (level - set->sustain < 1.0e-4f)
                        stage = set->sustain > 0.0f ? Stage::sustain : Stage::idle;
                    break;
                case Stage::sustain:
                    break;
                case Stage::release:
                    level *= releaseCoef;
                    if (level < 1.0e-4f) stage = Stage::idle;
                    break;
                case Stage::idle:
                    break;
            }
        }

        Stage stage = Stage::idle;
        const Preset* set = &presets[0];
        float sr = 48000.0f, gain = 0.0f, level = 0.0f, cross = 1.0f;
        float phase1 = 0.0f, phase2 = 0.0f, phaseMod = 0.0f, inc1 = 0.0f, inc2 = 0.0f;
        float env = 0.0f, envCoef = 0.0f;
        float attackStep = 0.0f, decayCoef = 0.0f, releaseCoef = 0.0f;
        float pitch = 1.0f, vibPhase = 0.0f, vibRamp = 1.0f, vibRampStep = 1.0f, modPhase = 0.0f;
        float width = 0.5f, slaveRatio = 0.0f, slave1 = 0.0f, slave2 = 0.0f, subPhase = 0.0f, mixNorm = 1.0f;
        uint32_t noiseState = 1, startOrder = 0, counter = 0;
        std::array<float, maxUnison> uniPhase {}, uniInc {};
        std::array<float, maxPartials> partialPhase {}, partialInc {}, partialAmp {}, partialCoef {};
        detail::LowPass filterL, filterR;
    };

    class PreviewSynth
    {
    public:
        PreviewSynth() { detail::sineTable(); }  // build the table off the audio thread

        void prepare (double sampleRate) { sr = (float) sampleRate; killAll(); }
        // The sound for new notes on every channel except those given their own with setChannelPreset
        void setPreset (int index) { preset = index >= 0 && index < numPresets ? index : 0; }
        void setChannelPreset (int channel, int index)
        {
            if (channel >= 1 && channel <= 16) channelPreset[(size_t) channel - 1] = index >= 0 && index < numPresets ? index : -1;
        }

        void noteOn (int channel, int note, float velocity)
        {
            for (auto& v : voices)
                if (v.plays (channel, note)) v.release();
            const int own = channel >= 1 && channel <= 16 ? channelPreset[(size_t) channel - 1] : -1;
            voiceFor().start (channel, note, velocity, presets[(size_t) (own >= 0 ? own : preset)], sr, ++order);
        }

        void noteOff (int channel, int note)
        {
            for (auto& v : voices)
                if (v.plays (channel, note)) v.release();
        }

        void releaseAll() { for (auto& v : voices) v.release(); }
        void killAll()    { for (auto& v : voices) v.kill(); }

        int numActive() const
        {
            int n = 0;
            for (auto& v : voices) n += v.isActive() ? 1 : 0;
            return n;
        }

        // Adds into the buffers (callers clear them first)
        void render (float* left, float* right, int num)
        {
            for (auto& v : voices)
                if (v.isActive()) v.render (left, right, num);
        }

    private:
        // A free voice, else the quietest released one, else the oldest
        Voice& voiceFor()
        {
            Voice* best = nullptr;
            for (auto& v : voices)
                if (! v.isActive()) return v;
            for (auto& v : voices)
                if (v.isReleased() && (best == nullptr || v.currentLevel() < best->currentLevel())) best = &v;
            if (best != nullptr) return *best;
            best = &voices[0];
            for (auto& v : voices)
                if (v.order() < best->order()) best = &v;
            return *best;
        }

        std::array<Voice, numVoices> voices;
        int preset = 0;
        std::array<int, 16> channelPreset { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        float sr = 48000.0f;
        uint32_t order = 0;
    };
}
