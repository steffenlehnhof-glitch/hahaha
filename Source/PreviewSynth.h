// =============================================================================
//  Purrgression - kleiner Vorschau-Synth (reines C++, kein JUCE)
//  Weicher FM-Klang, damit man die Akkorde sofort hoert, ohne etwas zu routen.
// =============================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace purr
{
class PreviewSynth
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        for (auto& v : voices) v = Voice();
        setSound (sound);
    }

    // 0 = aus, 1 = Keys, 2 = Pad, 3 = Pluck
    void setSound (int idx)
    {
        sound = std::clamp (idx, 0, 3);
        switch (sound)
        {
            default:
            case 1: attack = 0.004; decay = 1.1;  sustain = 0.32f; release = 0.30; fmAmount = 1.3f; fmDecay = 0.30; h2 = 0.10f; detune = 0.0f;   break;
            case 2: attack = 0.22;  decay = 1.5;  sustain = 0.80f; release = 0.90; fmAmount = 0.35f; fmDecay = 2.0;  h2 = 0.22f; detune = 0.004f; break;
            case 3: attack = 0.002; decay = 0.28; sustain = 0.0f;  release = 0.16; fmAmount = 2.2f; fmDecay = 0.09; h2 = 0.05f; detune = 0.0f;   break;
        }
        if (sound == 0)
            allNotesOff();
    }

    void noteOn (int note, float velocity)
    {
        if (sound == 0) return;

        Voice* v = nullptr;
        for (auto& c : voices) if (c.stage != 0 && c.note == note) { v = &c; break; }
        if (v == nullptr) for (auto& c : voices) if (c.stage == 0) { v = &c; break; }
        if (v == nullptr)
        {
            v = &voices[0];
            for (auto& c : voices) if (c.age < v->age) v = &c;
        }

        const double freq = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        v->note  = note;
        v->inc   = 2.0 * 3.14159265358979323846 * freq / sr;
        v->vel   = 0.35f + 0.65f * std::clamp (velocity, 0.0f, 1.0f);
        v->stage = 1;
        v->menv  = 1.0f;
        v->age   = ++counter;
        v->pan   = std::clamp ((float) (note - 60) / 60.0f, -0.35f, 0.35f);
        // Phase und Huellkurve bei Retrigger weiterlaufen lassen (kein Knacksen)
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.stage != 0 && v.stage != 4 && v.note == note)
                v.stage = 4;
    }

    void allNotesOff()
    {
        for (auto& v : voices)
            if (v.stage != 0)
                v.stage = 4;
    }

    // addiert in die Puffer (right darf nullptr sein)
    void render (float* left, float* right, int numSamples)
    {
        if (numSamples <= 0 || left == nullptr) return;

        const float aStep = (float) (1.0 / std::max (1.0, attack * sr));
        const float dCoef = (float) std::exp (-1.0 / (decay * sr * 0.35));
        const float rCoef = (float) std::exp (-1.0 / (release * sr * 0.25));
        const float mCoef = (float) std::exp (-1.0 / (fmDecay * sr));

        for (auto& v : voices)
        {
            if (v.stage == 0) continue;

            const float gl = 0.16f * v.vel * (1.0f - v.pan);
            const float gr = 0.16f * v.vel * (1.0f + v.pan);

            for (int i = 0; i < numSamples; ++i)
            {
                switch (v.stage)
                {
                    case 1: v.env += aStep; if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 2; } break;
                    case 2: v.env = sustain + (v.env - sustain) * dCoef; break;
                    case 4: v.env *= rCoef; if (v.env < 0.0005f) { v.env = 0.0f; v.stage = 0; } break;
                    default: break;
                }
                if (v.stage == 0) break;

                v.menv *= mCoef;
                const double ph = v.phase;
                float smp = (float) std::sin (ph + (double) (fmAmount * v.menv) * std::sin (ph));
                smp += h2 * (float) std::sin (2.0 * ph);
                if (detune > 0.0f)
                    smp = 0.6f * smp + 0.4f * (float) std::sin (v.phase2);

                v.phase  += v.inc;
                v.phase2 += v.inc * (1.0 + (double) detune);
                if (v.phase  > 6.283185307179586) v.phase  -= 6.283185307179586;
                if (v.phase2 > 6.283185307179586) v.phase2 -= 6.283185307179586;

                smp *= v.env;
                left[i] += smp * gl;
                if (right != nullptr) right[i] += smp * gr;
            }
        }

        // sanfte Begrenzung
        for (int i = 0; i < numSamples; ++i)
        {
            left[i] = std::tanh (left[i]);
            if (right != nullptr) right[i] = std::tanh (right[i]);
        }
    }

private:
    struct Voice
    {
        int      stage = 0;      // 0 aus, 1 attack, 2 decay/sustain, 4 release
        int      note  = -1;
        double   phase = 0.0, phase2 = 0.0, inc = 0.0;
        float    env = 0.0f, menv = 0.0f, vel = 1.0f, pan = 0.0f;
        uint64_t age = 0;
    };

    Voice    voices[24];
    double   sr = 44100.0;
    int      sound = 1;
    uint64_t counter = 0;
    double   attack = 0.004, decay = 1.1, release = 0.3, fmDecay = 0.3;
    float    sustain = 0.32f, fmAmount = 1.3f, h2 = 0.1f, detune = 0.0f;
};

} // namespace purr
