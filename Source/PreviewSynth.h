#pragma once
#include <array>
#include <cmath>
#include <cstdint>

// Small built-in synth, only for hearing chords without routing MIDI to another instrument.
// 16 voices, 4 fixed sounds, no allocation after construction.
namespace pc::synth
{
    enum class Sound { piano, softPiano, electricPiano, pad, pluck, lead };

    constexpr int numVoices = 16;
    constexpr float twoPi = 6.28318530718f;

    struct SoundSettings
    {
        float attack, decay, sustain, release;  // seconds, level 0-1
        float cutoff, cutoffEnv, filterDecay;   // Hz (0 = no filter), Hz added at the envelope peak, seconds
                                            // (Electric Piano has no filter and uses the envelope as its FM depth)
        float detuneCents;
        int partials;                           // piano sounds: number of sine partials (0 = not additive)
        float brightness;                       // piano sounds: how fast upper partials fall away (0-1)
    };

    constexpr SoundSettings settingsFor (Sound s)
    {
        switch (s)
        {
            case Sound::piano:         return { 0.001f, 4.0f,  0.0f,  0.30f,  0.0f,    0.0f,    0.0f,  0.8f, 7, 0.60f };
            case Sound::softPiano:     return { 0.006f, 3.0f,  0.0f,  0.50f,  0.0f,    0.0f,    0.0f,  0.5f, 4, 0.30f };
            case Sound::electricPiano: return { 0.002f, 1.6f,  0.25f, 0.35f,  0.0f,    0.0f,    0.6f,  0.0f, 0, 0.0f };
            case Sound::pad:           return { 0.35f,  1.5f,  0.80f, 1.20f,  1400.0f, 900.0f,  1.2f,  7.0f, 0, 0.0f };
            case Sound::pluck:         return { 0.001f, 0.45f, 0.0f,  0.25f,  300.0f,  5000.0f, 0.18f, 0.0f, 0, 0.0f };
            case Sound::lead:          return { 0.005f, 0.30f, 0.85f, 0.18f,  3500.0f, 1500.0f, 0.3f,  5.0f, 0, 0.0f };
        }
        return settingsFor (Sound::piano);
    }

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

        void start (int ch, int midiNote, float velocity, Sound s, float sampleRate, uint32_t order)
        {
            channel = ch;
            note = midiNote;
            sound = s;
            set = settingsFor (s);
            sr = sampleRate;
            startOrder = order;
            gain = 0.16f * (0.3f + 0.7f * velocity);
            const float hz = 440.0f * std::pow (2.0f, (float) (midiNote - 69) / 12.0f);
            const float detune = std::pow (2.0f, set.detuneCents / 1200.0f);
            inc1 = hz / sr * detune;
            inc2 = hz / sr / detune;
            phase1 = 0.0f;
            phase2 = 0.37f;
            level = 0.0f;
            filterEnv = 1.0f;
            filterCoef = set.filterDecay > 0.0f ? std::exp (-1.0f / (set.filterDecay * 0.25f * sr)) : 0.0f;
            attackStep = 1.0f / std::fmax (1.0f, set.attack * sr);
            decayCoef = std::exp (-1.0f / (set.decay * 0.25f * sr));
            releaseCoef = std::exp (-1.0f / (set.release * 0.25f * sr));
            // Piano: sine partials with slight string stiffness (stretched overtones). Harder notes are
            // brighter, and upper partials die away faster, like a real string.
            const float bright = set.brightness * (0.6f + 0.6f * velocity);
            for (int k = 0; k < maxPartials; ++k)
            {
                const float n = (float) (k + 1);
                partialInc[(size_t) k] = k < set.partials ? hz * n * std::sqrt (1.0f + 0.0004f * n * n) / sr : 0.0f;
                partialAmp[(size_t) k] = k < set.partials && partialInc[(size_t) k] < 0.45f ? std::pow (bright, n - 1.0f) / n : 0.0f;
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
            for (int i = 0; i < num && stage != Stage::idle; ++i)
            {
                if ((counter++ & 15) == 0 && set.cutoff > 0.0f)
                {
                    const float fc = set.cutoff + set.cutoffEnv * filterEnv;
                    filterL.setCutoff (fc, sr);
                    filterR.setCutoff (fc, sr);
                }

                float l = 0.0f, r = 0.0f;
                switch (sound)
                {
                    case Sound::piano:
                    case Sound::softPiano:
                    {
                        float sum = 0.0f;
                        for (int k = 0; k < set.partials; ++k)
                        {
                            sum += partialAmp[(size_t) k] * detail::sine (partialPhase[(size_t) k]);
                            partialPhase[(size_t) k] += partialInc[(size_t) k];
                            if (partialPhase[(size_t) k] >= 1.0f) partialPhase[(size_t) k] -= 1.0f;
                            partialAmp[(size_t) k] *= partialCoef[(size_t) k];
                        }
                        // A second, slightly detuned string on the fundamental gives the stereo shimmer
                        const float second = 0.35f * partialAmp[0] * detail::sine (phase2);
                        l = sum + second;
                        r = sum - second;
                        break;
                    }
                    case Sound::electricPiano:
                    {
                        // Two-operator FM with a decaying index: a soft electric-piano tone
                        const float index = 1.3f * filterEnv;
                        l = r = detail::sine (phase1 + index * detail::sine (phase1));
                        break;
                    }
                    case Sound::pad:
                    {
                        const float a = detail::saw (phase1, inc1), b = detail::saw (phase2, inc2);
                        l = filterL.process (a + 0.5f * b) * 0.7f;
                        r = filterR.process (b + 0.5f * a) * 0.7f;
                        break;
                    }
                    case Sound::pluck:
                        l = r = filterL.process (detail::saw (phase1, inc1));
                        break;
                    case Sound::lead:
                        l = r = filterL.process (0.6f * detail::square (phase1, inc1) + 0.5f * detail::saw (phase2, inc2)) * 0.8f;
                        break;
                }

                phase1 += inc1; if (phase1 >= 1.0f) phase1 -= 1.0f;
                phase2 += inc2; if (phase2 >= 1.0f) phase2 -= 1.0f;
                filterEnv *= filterCoef;

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
                    level = set.sustain + (level - set.sustain) * decayCoef;
                    if (level - set.sustain < 1.0e-4f)
                        stage = set.sustain > 0.0f ? Stage::sustain : Stage::idle;
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
        Sound sound = Sound::piano;
        SoundSettings set = settingsFor (Sound::piano);
        float sr = 48000.0f, gain = 0.0f, level = 0.0f;
        float phase1 = 0.0f, phase2 = 0.0f, inc1 = 0.0f, inc2 = 0.0f;
        float filterEnv = 0.0f, filterCoef = 0.0f;
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
        void setSound (Sound s) { sound = s; }

        void noteOn (int channel, int note, float velocity)
        {
            for (auto& v : voices)
                if (v.plays (channel, note)) v.release();
            voiceFor().start (channel, note, velocity, sound, sr, ++order);
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
        Sound sound = Sound::piano;
        float sr = 48000.0f;
        uint32_t order = 0;
    };
}
