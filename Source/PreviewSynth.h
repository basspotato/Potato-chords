#pragma once
#include <array>
#include <cmath>
#include <cstdint>

// Small built-in synth, only for hearing chords without routing MIDI to another instrument.
// 16 voices, 25 fixed presets in 5 groups, no allocation after construction.
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
    };

    using E = Engine;
    //                       name                group      engine          A       D      S      R      cutoff   env     envDec  det    width  saw   sq    sine  ratio  index  parts bright level
    inline constexpr std::array<Preset, 25> presets {{
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
    }};

    constexpr int numPresets = (int) presets.size();
    constexpr int presetsPerGroup = 5;

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

        inline float square (float t, float dt)
        {
            float t2 = t + 0.5f;
            if (t2 >= 1.0f) t2 -= 1.0f;
            return (t < 0.5f ? 1.0f : -1.0f) + polyBlep (t, dt) - polyBlep (t2, dt);
        }

        // Topology-preserving state-variable low-pass
        struct LowPass
        {
            float ic1 = 0.0f, ic2 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

            void setCutoff (float hz, float sampleRate)
            {
                const float g = std::tan (3.14159265f * std::fmin (hz, sampleRate * 0.45f) / sampleRate);
                const float k = 1.2f;
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
                if ((counter++ & 15) == 0 && p.cutoff > 0.0f)
                {
                    const float fc = p.cutoff + p.cutoffEnv * env;
                    filterL.setCutoff (fc, sr);
                    filterR.setCutoff (fc, sr);
                }

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
                        const float a = p.saw * detail::saw (phase1, inc1) + p.square * detail::square (phase1, inc1) + p.sine * detail::sine (phase1);
                        const float b = p.saw * detail::saw (phase2, inc2) + p.square * detail::square (phase2, inc2) + p.sine * detail::sine (phase2);
                        l = (a + cross * b) * 0.6f;
                        r = (b + cross * a) * 0.6f;
                        break;
                    }
                }

                if (p.cutoff > 0.0f)
                {
                    l = filterL.process (l);
                    r = filterR.process (r);
                }

                phase1 += inc1; if (phase1 >= 1.0f) phase1 -= 1.0f;
                phase2 += inc2; if (phase2 >= 1.0f) phase2 -= 1.0f;
                phaseMod += inc1 * p.ratio; phaseMod -= std::floor (phaseMod);
                env *= envCoef;

                stepEnvelope();
                const float g = gain * level;
                left[i] += l * g;
                right[i] += r * g;
            }
        }

        int channel = 1, note = -1;

    private:
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
        uint32_t startOrder = 0, counter = 0;
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
