// =============================================================================
//  Purrgression - eingebaute Instrumente (reines C++, kein JUCE)
//  Kleiner polyphoner Synth mit zehn Klaengen: FM, gefilterte Saegezaehne,
//  Orgel-Additiv und gezupfte Saite (Karplus-Strong).
// =============================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace purr
{
constexpr int kNumSounds = 11;   // inklusive "Aus"

inline const char* soundName (int id)
{
    static const char* n[kNumSounds] = { "Aus", "E-Piano", "Piano", "Pad", "Dark Pad", "Strings",
                                         "Pluck", "Bell", "Orgel", "Gitarre", "Fl\xc3\xb6te" };
    return n[std::clamp (id, 0, kNumSounds - 1)];
}

class PreviewSynth
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        for (auto& v : voices) v = Voice();
        setSound (sound);
    }

    void setSound (int idx)
    {
        sound = std::clamp (idx, 0, kNumSounds - 1);
        patch = patchFor (sound);
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
        const bool fresh = v->stage == 0;

        v->note     = note;
        v->inc      = twoPi * freq / sr;
        v->vel      = 0.35f + 0.65f * std::clamp (velocity, 0.0f, 1.0f);
        v->stage    = 1;
        v->menv     = 1.0f;
        v->fenv     = 1.0f;
        v->age      = ++counter;
        v->pan      = std::clamp ((float) (note - 60) / 60.0f, -0.35f, 0.35f);
        v->keyScale = (float) std::pow (2.0, (note - 60) / 24.0);

        if (fresh)
        {
            v->env = 0.0f;
            v->lp1 = v->lp2 = 0.0f;
            v->lfo = 0.0;
            // leicht versetzte Startphasen, damit Unisono nicht "phasig" beginnt
            v->ph[0] = 0.0; v->ph[1] = 2.1; v->ph[2] = 4.2; v->mph = 0.0;
        }

        if (patch.wave == kString)
        {
            // Saite anzupfen: Verzoegerungsleitung mit geglaettetem Rauschen fuellen
            const double len = std::clamp (sr / freq - 0.5, 4.0, (double) (ksSize - 8));
            const int n = (int) std::ceil (len) + 2;
            float last = 0.0f;
            for (int i = 0; i < ksSize; ++i)
            {
                float x = 0.0f;
                if (i < n)
                {
                    last = 0.55f * last + 0.45f * white();
                    x = last * 1.6f;
                }
                v->ks[i] = x;
            }
            v->ksLen = len;
            v->ksW = n;
            v->ksPrev = 0.0f;
            v->env = 1.0f;
            v->stage = 2;
        }
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

        const Patch& p = patch;
        const float aStep = (float) (1.0 / std::max (1.0, (double) p.attack * sr));
        const float dCoef = (float) std::exp (-1.0 / (std::max (0.01, (double) p.decay) * sr * 0.35));
        const float rCoef = (float) std::exp (-1.0 / (std::max (0.01, (double) p.release) * sr * 0.25));
        const float mCoef = (float) std::exp (-1.0 / (std::max (0.01, (double) p.fmDecay) * sr));
        const float fCoef = (float) std::exp (-1.0 / (std::max (0.01, (double) p.filterDecay) * sr));
        const double lfoInc = twoPi * (double) p.vibRate / sr;
        const float  fcScale = (float) (twoPi / sr);
        const float  uniNorm = 1.0f / std::sqrt ((float) std::max (1, p.unison));
        const double det[3] = { 1.0, 1.0 + (double) p.detune, 1.0 - (double) p.detune };

        for (auto& v : voices)
        {
            if (v.stage == 0) continue;

            const float gl = 0.16f * p.gain * v.vel * (1.0f - v.pan);
            const float gr = 0.16f * p.gain * v.vel * (1.0f + v.pan);

            for (int i = 0; i < numSamples; ++i)
            {
                switch (v.stage)
                {
                    case 1: v.env += aStep; if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 2; } break;
                    case 2: v.env = p.sustain + (v.env - p.sustain) * dCoef; break;
                    case 4: v.env *= rCoef; if (v.env < 0.0004f) { v.env = 0.0f; v.stage = 0; } break;
                    default: break;
                }
                if (v.stage == 0) break;

                v.menv *= mCoef;
                v.fenv *= fCoef;

                double inc = v.inc;
                if (p.vibDepth > 0.0f)
                {
                    inc *= 1.0 + (double) p.vibDepth * std::sin (v.lfo);
                    v.lfo += lfoInc;
                    if (v.lfo > twoPi) v.lfo -= twoPi;
                }

                float smp = 0.0f;

                switch (p.wave)
                {
                    case kFm:
                    {
                        const double mod = (double) (p.fm * v.menv) * std::sin (v.mph);
                        smp = (float) std::sin (v.ph[0] + mod);
                        if (p.h2 > 0.0f)
                            smp += p.h2 * (float) std::sin (2.0 * v.ph[0]);
                        if (p.unison > 1)
                        {
                            smp = 0.62f * smp + 0.38f * (float) std::sin (v.ph[1] + mod);
                            v.ph[1] = wrap (v.ph[1] + inc * det[1]);
                        }
                        v.ph[0] = wrap (v.ph[0] + inc);
                        v.mph   = wrap (v.mph + inc * (double) p.fmRatio);
                        break;
                    }

                    case kSaw:
                    {
                        for (int u = 0; u < p.unison; ++u)
                        {
                            const double dt = inc * det[u] / twoPi;
                            const double t = v.ph[u] / twoPi;
                            smp += (float) (2.0 * t - 1.0) - polyBlep (t, dt);
                            v.ph[u] = wrap (v.ph[u] + inc * det[u]);
                        }
                        smp *= uniNorm;
                        break;
                    }

                    case kOrgan:
                    {
                        const double ph = v.ph[0];
                        smp = 0.5f * (float) (std::sin (ph) + 0.55 * std::sin (2.0 * ph) + 0.3 * std::sin (3.0 * ph)
                                              + 0.22 * std::sin (4.0 * ph) + 0.1 * std::sin (6.0 * ph));
                        v.ph[0] = wrap (v.ph[0] + inc);
                        break;
                    }

                    case kString:
                    {
                        const double rp = (double) v.ksW - v.ksLen;
                        const int i0 = (int) std::floor (rp);
                        const float fr = (float) (rp - i0);
                        const float x0 = v.ks[i0 & (ksSize - 1)], x1 = v.ks[(i0 + 1) & (ksSize - 1)];
                        const float out = x0 + fr * (x1 - x0);
                        v.ks[v.ksW & (ksSize - 1)] = 0.5f * (out + v.ksPrev) * 0.9965f;
                        v.ksPrev = out;
                        ++v.ksW;
                        smp = out;
                        break;
                    }

                    default: break;
                }

                if (p.noise > 0.0f)
                    smp += p.noise * white();

                if (p.cutoff > 0.0f)
                {
                    float a = std::min (0.45f * (float) sr, (p.cutoff + p.cutoffEnv * v.fenv) * v.keyScale) * fcScale;
                    a = a / (1.0f + a);
                    v.lp1 += a * (smp - v.lp1);
                    v.lp2 += a * (v.lp1 - v.lp2);
                    smp = v.lp2;
                }

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
    enum Wave { kFm, kSaw, kOrgan, kString };

    struct Patch
    {
        int   wave;
        float attack, decay, sustain, release;
        float fm, fmRatio, fmDecay, h2;
        int   unison;
        float detune;
        float cutoff, cutoffEnv, filterDecay;
        float noise, vibDepth, vibRate;
        float gain;
    };

    static Patch patchFor (int idx)
    {
        //                         wave     att     dec   sus    rel    fm    ratio fmDec  h2    uni det      cutoff  cutEnv  fDec   noise  vibD     vibR  gain
        static const Patch t[kNumSounds] = {
            /* Aus      */ { kFm,     0.004f, 1.0f, 0.0f,  0.1f,  0.0f, 1.0f, 0.3f,  0.0f, 1, 0.0f,    0.0f,   0.0f,   1.0f,  0.0f,  0.0f,    5.0f, 0.0f },
            /* E-Piano  */ { kFm,     0.004f, 1.1f, 0.32f, 0.30f, 1.3f, 1.0f, 0.30f, 0.10f, 1, 0.0f,   0.0f,   0.0f,   1.0f,  0.0f,  0.0f,    5.0f, 1.0f },
            /* Piano    */ { kFm,     0.002f, 2.0f, 0.0f,  0.35f, 2.6f, 1.0f, 0.14f, 0.22f, 2, 0.0012f, 5200.0f, 0.0f,  1.0f,  0.0f,  0.0f,    5.0f, 1.15f },
            /* Pad      */ { kSaw,    0.25f,  1.5f, 0.80f, 0.90f, 0.0f, 1.0f, 1.0f,  0.0f, 3, 0.006f,  900.0f, 700.0f, 1.5f,  0.0f,  0.0f,    5.0f, 1.25f },
            /* Dark Pad */ { kSaw,    0.45f,  2.0f, 0.85f, 1.40f, 0.0f, 1.0f, 1.0f,  0.0f, 3, 0.009f,  360.0f, 260.0f, 2.5f,  0.0f,  0.0f,    5.0f, 2.2f },
            /* Strings  */ { kSaw,    0.12f,  1.0f, 0.85f, 0.50f, 0.0f, 1.0f, 1.0f,  0.0f, 3, 0.005f,  2600.0f, 0.0f,  1.0f,  0.0f,  0.0035f, 5.2f, 0.8f },
            /* Pluck    */ { kSaw,    0.002f, 0.35f, 0.0f, 0.20f, 0.0f, 1.0f, 1.0f,  0.0f, 2, 0.004f,  450.0f, 5200.0f, 0.11f, 0.0f, 0.0f,    5.0f, 1.5f },
            /* Bell     */ { kFm,     0.002f, 2.4f, 0.0f,  1.20f, 1.7f, 3.5f, 1.3f,  0.0f, 1, 0.0f,    0.0f,   0.0f,   1.0f,  0.0f,  0.0f,    5.0f, 0.85f },
            /* Orgel    */ { kOrgan,  0.010f, 0.1f, 1.0f,  0.08f, 0.0f, 1.0f, 1.0f,  0.0f, 1, 0.0f,    0.0f,   0.0f,   1.0f,  0.0f,  0.002f,  6.0f, 0.8f },
            /* Gitarre  */ { kString, 0.001f, 1.0f, 1.0f,  0.25f, 0.0f, 1.0f, 1.0f,  0.0f, 1, 0.0f,    0.0f,   0.0f,   1.0f,  0.0f,  0.0f,    5.0f, 1.3f },
            /* Floete   */ { kFm,     0.07f,  0.3f, 0.85f, 0.18f, 0.3f, 2.0f, 5.0f,  0.12f, 1, 0.0f,   3200.0f, 0.0f,  1.0f,  0.05f, 0.006f,  5.0f, 1.0f },
        };
        return t[std::clamp (idx, 0, kNumSounds - 1)];
    }

    static constexpr double twoPi = 6.283185307179586;
    static constexpr int ksSize = 4096;

    static double wrap (double ph)        { return ph >= twoPi ? ph - twoPi : ph; }

    static float polyBlep (double t, double dt)
    {
        if (dt <= 0.0) return 0.0f;
        if (t < dt)        { t /= dt;               return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt)  { t = (t - 1.0) / dt;    return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    float white()
    {
        noiseSeed = noiseSeed * 1664525u + 1013904223u;
        return (float) (int32_t) noiseSeed * (1.0f / 2147483648.0f);
    }

    struct Voice
    {
        int      stage = 0;      // 0 aus, 1 attack, 2 decay/sustain, 4 release
        int      note  = -1;
        double   ph[3] = { 0.0, 0.0, 0.0 }, mph = 0.0, inc = 0.0, lfo = 0.0;
        float    env = 0.0f, menv = 0.0f, fenv = 0.0f, vel = 1.0f, pan = 0.0f, keyScale = 1.0f;
        float    lp1 = 0.0f, lp2 = 0.0f;
        uint64_t age = 0;

        // gezupfte Saite
        double   ksLen = 0.0;
        int      ksW = 0;
        float    ksPrev = 0.0f;
        float    ks[ksSize] = {};
    };

    Voice    voices[24];
    Patch    patch = patchFor (1);
    double   sr = 44100.0;
    int      sound = 1;
    uint64_t counter = 0;
    uint32_t noiseSeed = 0x2545f491u;
};

} // namespace purr
