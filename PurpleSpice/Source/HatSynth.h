// =============================================================================
//  Purple Spice - eingebaute Hi-Hats (reines C++, kein JUCE)
//  Rauschen + sechs metallische Rechteck-Oszillatoren (wie bei der 808) durch
//  einen Hochpass. Vier Kits, Tonhoehe in Halbtoenen, Open Hat mit Choke.
// =============================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spice
{
constexpr int kNumKits = 5;   // inklusive "Aus"

inline const char* kitName (int id)
{
    static const char* n[kNumKits] = { "Aus", "Crisp", "808", "Dusty", "Sharp" };
    return n[std::clamp (id, 0, kNumKits - 1)];
}

class HatSynth
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        for (auto& v : voices) v = Voice();
    }

    void setKit (int idx)
    {
        kit = std::clamp (idx, 0, kNumKits - 1);
        if (kit == 0)
            for (auto& v : voices) v.release = true;
    }

    // velocity 0..1, pitch in Halbtoenen, pan -1..1
    void trigger (bool open, float velocity, int pitch, float pan)
    {
        if (kit == 0) return;
        const Kit& k = kitFor (kit);

        // jeder neue Schlag wuergt eine klingende Open Hat ab
        for (auto& v : voices)
            if (v.active && v.open)
                v.release = true;

        Voice* v = &voices[0];
        for (auto& c : voices) { if (! c.active) { v = &c; break; } if (c.age < v->age) v = &c; }

        const double ratio = std::pow (2.0, pitch / 12.0);
        static const double partials[6] = { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };

        *v = Voice();
        v->active = true;
        v->open   = open;
        v->age    = ++counter;
        for (int i = 0; i < 6; ++i)
            v->inc[i] = std::min (0.49, partials[i] * (double) k.metalTune * ratio / sr);

        const float vel = std::clamp (velocity, 0.0f, 1.0f);
        const float amp = k.gain * (0.12f + 0.88f * vel * vel);
        const float p = std::clamp (pan, -1.0f, 1.0f);
        v->gainL = amp * (1.0f - 0.6f * std::max (0.0f, p));
        v->gainR = amp * (1.0f + 0.6f * std::min (0.0f, p));

        // Filter folgt der Tonhoehe zur Haelfte, leise Schlaege sind etwas dumpfer
        const double fc = std::min (0.42 * sr, (double) k.hpFreq * std::pow (2.0, pitch / 24.0) * (0.85 + 0.15 * vel));
        const double g = std::tan (3.14159265358979323846 * fc / sr);
        const double kq = 1.0 / (double) k.q;
        v->a1 = (float) (1.0 / (1.0 + g * (g + kq)));
        v->a2 = (float) (g * v->a1);
        v->a3 = (float) (g * v->a2);
        v->kq = (float) kq;

        const double lp = k.lpFreq > 0.0f ? std::min (0.45 * sr, (double) k.lpFreq * std::pow (2.0, pitch / 24.0)) : 0.0;
        v->lpA = lp > 0.0 ? (float) (1.0 - std::exp (-6.283185307179586 * lp / sr)) : 1.0f;

        const double tau = (double) (open ? k.openDecay : k.closedDecay) * (0.75 + 0.25 * vel);
        v->decay = (float) std::exp (-1.0 / (tau * sr));
    }

    // Open Hat ausklingen lassen (Notenende)
    void releaseOpen()
    {
        for (auto& v : voices)
            if (v.active && v.open)
                v.release = true;
    }

    // addiert in die Puffer (right darf nullptr sein)
    void render (float* left, float* right, int numSamples, float level)
    {
        if (numSamples <= 0 || left == nullptr) return;

        const Kit& k = kitFor (kit);
        const float attStep = (float) (1.0 / (0.0004 * sr));
        const float relCoef = (float) std::exp (-1.0 / (0.012 * sr));

        for (auto& v : voices)
        {
            if (! v.active) continue;

            for (int i = 0; i < numSamples; ++i)
            {
                // Quelle: Rauschen + Metall
                noiseSeed = noiseSeed * 1664525u + 1013904223u;
                const float noise = (float) (int32_t) noiseSeed * (1.0f / 2147483648.0f);

                float metal = 0.0f;
                for (int o = 0; o < 6; ++o)
                {
                    metal += v.ph[o] < 0.5 ? 1.0f : -1.0f;
                    v.ph[o] += v.inc[o];
                    if (v.ph[o] >= 1.0) v.ph[o] -= 1.0;
                }

                const float in = k.noiseMix * noise + k.metalMix * metal * 0.4f;

                // Hochpass (State-Variable-Filter), danach optional ein weicher Tiefpass
                const float v3 = in - v.ic2;
                const float v1 = v.a1 * v.ic1 + v.a2 * v3;
                const float v2 = v.ic2 + v.a2 * v.ic1 + v.a3 * v3;
                v.ic1 = 2.0f * v1 - v.ic1;
                v.ic2 = 2.0f * v2 - v.ic2;
                float smp = in - v.kq * v1 - v2;

                v.lp += v.lpA * (smp - v.lp);
                smp = v.lp;

                // Huellkurve
                if (v.att < 1.0f) v.att = std::min (1.0f, v.att + attStep);
                v.env *= v.release ? relCoef : v.decay;
                if (v.env < 0.0005f) { v.active = false; break; }

                smp *= v.env * v.att * level;
                left[i] += smp * v.gainL;
                if (right != nullptr) right[i] += smp * v.gainR;
            }
        }

        for (int i = 0; i < numSamples; ++i)
        {
            left[i] = std::tanh (left[i]);
            if (right != nullptr) right[i] = std::tanh (right[i]);
        }
    }

private:
    struct Kit
    {
        float noiseMix, metalMix;
        float hpFreq, q, lpFreq;      // lpFreq 0 = kein Tiefpass
        float closedDecay, openDecay; // Sekunden
        float metalTune;
        float gain;
    };

    static const Kit& kitFor (int idx)
    {
        static const Kit k[kNumKits] = {
            /* Aus   */ { 0.0f, 0.0f, 8000.0f, 0.7f, 0.0f,     0.02f,  0.1f,  1.0f, 0.0f },
            /* Crisp */ { 1.0f, 0.35f, 7200.0f, 0.9f, 0.0f,    0.022f, 0.17f, 1.6f, 0.55f },
            /* 808   */ { 0.25f, 1.0f, 8200.0f, 1.6f, 0.0f,    0.030f, 0.24f, 1.0f, 0.75f },
            /* Dusty */ { 1.0f, 0.2f, 4300.0f, 0.8f, 8500.0f,  0.034f, 0.15f, 1.3f, 0.70f },
            /* Sharp */ { 1.0f, 0.15f, 9800.0f, 1.1f, 0.0f,    0.012f, 0.12f, 2.0f, 0.62f },
        };
        return k[std::clamp (idx, 0, kNumKits - 1)];
    }

    struct Voice
    {
        bool     active = false, open = false, release = false;
        double   ph[6] = { 0.0, 0.17, 0.31, 0.53, 0.71, 0.89 }, inc[6] = {};
        float    env = 1.0f, att = 0.0f, decay = 0.99f;
        float    a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, kq = 1.0f, ic1 = 0.0f, ic2 = 0.0f;
        float    lp = 0.0f, lpA = 1.0f;
        float    gainL = 0.5f, gainR = 0.5f;
        uint64_t age = 0;
    };

    Voice    voices[10];
    double   sr = 44100.0;
    int      kit = 1;
    uint64_t counter = 0;
    uint32_t noiseSeed = 0x1f123bb5u;
};

} // namespace spice
