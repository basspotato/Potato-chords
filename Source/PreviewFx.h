#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// Chorus and reverb on the preview sound only (MIDI out is untouched).
// prepare() allocates; process() is real-time safe.
namespace pc::fx
{
    // Stereo chorus: two delay lines swept by slow sine LFOs a quarter-cycle apart
    class Chorus
    {
    public:
        void prepare (double sampleRate)
        {
            rate = sampleRate;
            const auto size = (size_t) std::ceil (sampleRate * 0.04) + 4;
            for (auto& line : lines) line.assign (size, 0.0f);
            reset();
        }

        void reset()
        {
            for (auto& line : lines) std::fill (line.begin(), line.end(), 0.0f);
            write = 0;
            phase = 0.0;
        }

        // amount 0-1 sets both the depth and the wet mix
        void process (float* left, float* right, int numSamples, float amount)
        {
            if (lines[0].empty() || amount <= 0.0f) return;
            const int size = (int) lines[0].size();
            const double inc = 0.55 / rate;
            const float wet = 0.5f * amount, dry = 1.0f - 0.25f * amount;
            const double base = 0.012 * rate, depth = (0.002 + 0.004 * amount) * rate;
            float* io[2] { left, right };
            for (int i = 0; i < numSamples; ++i)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto& line = lines[(size_t) ch];
                    const float in = io[ch][i];
                    line[(size_t) write] = in;
                    const double lfo = std::sin (juce::MathConstants<double>::twoPi * (phase + 0.25 * ch));
                    double readPos = write - (base + depth * (0.5 + 0.5 * lfo));
                    if (readPos < 0.0) readPos += size;
                    const int a = (int) readPos;
                    const float frac = (float) (readPos - a);
                    const float delayed = line[(size_t) a] + frac * (line[(size_t) ((a + 1) % size)] - line[(size_t) a]);
                    io[ch][i] = dry * in + wet * delayed;
                }
                if (++write >= size) write = 0;
                phase += inc;
                if (phase >= 1.0) phase -= 1.0;
            }
        }

    private:
        std::array<std::vector<float>, 2> lines;
        double rate = 44100.0, phase = 0.0;
        int write = 0;
    };

    class PreviewFx
    {
    public:
        void prepare (double sampleRate)
        {
            chorus.prepare (sampleRate);
            reverb.setSampleRate (sampleRate);
            reset();
        }

        void reset()
        {
            chorus.reset();
            reverb.reset();
        }

        void process (float* left, float* right, int numSamples, float chorusAmount, float reverbAmount)
        {
            chorus.process (left, right, numSamples, chorusAmount);
            if (reverbAmount <= 0.0f)
            {
                if (reverbWasOn) reverb.reset();
                reverbWasOn = false;
                return;
            }
            if (! reverbWasOn || std::abs (reverbAmount - lastReverb) > 1.0e-4f)
            {
                juce::Reverb::Parameters p;
                p.roomSize = 0.55f + 0.3f * reverbAmount;
                p.damping = 0.45f;
                p.width = 1.0f;
                p.wetLevel = 0.4f * reverbAmount;  // juce::Reverb scales wet by 3 and dry by 2
                p.dryLevel = 0.5f * (1.0f - 0.3f * reverbAmount);
                reverb.setParameters (p);
                lastReverb = reverbAmount;
            }
            reverbWasOn = true;
            reverb.processStereo (left, right, numSamples);
        }

    private:
        Chorus chorus;
        juce::Reverb reverb;
        float lastReverb = -1.0f;
        bool reverbWasOn = false;
    };
}
