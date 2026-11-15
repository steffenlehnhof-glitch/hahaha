// =============================================================================
//  Purple 808 - der 808-Synth (reines C++, kein JUCE)
//  Einstimmig mit Glide: Sinus/Dreieck/Rechteck/Saege mit Pitch-Huellkurve
//  (der "Punch"), Klick, Verzerrung in vier Arten, Tiefpass und sauberem Sub.
//  16 Sounds, jeder ueber die Regler weiter formbar.
// =============================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace p808
{
constexpr int kNumKits = 16;

inline const char* kitName (int id)
{
    static const char* n[kNumKits] = { "Clean Sub", "Classic", "Punchy", "Long Boy", "Dark Drill", "Rage", "Phonk Clip", "Detroit Knock",
                                       "Boom-Bap Sub", "Growl", "Fuzz", "Soft Round", "Short Pluck", "Tape Warm", "Square Sub", "Glide King" };
    return n[std::clamp (id, 0, kNumKits - 1)];
}

struct SoundParams
{
    int   kit        = 2;
    float distortion = 0.3f;
    float tone       = 0.6f;
    float punch      = 0.5f;
    float decay      = 0.6f;
    float glide      = 0.4f;
    float sub        = 0.4f;
    float level      = 0.8f;
};

class BassSynth
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        for (auto& v : voices) v = Voice();
        current = 0;
    }

    void setParams (const SoundParams& p)   { params = p; params.kit = std::clamp (p.kit, 0, kNumKits - 1); }

    // Neue Note anschlagen. serial identifiziert die Note fuer noteOff.
    void noteOn (int note, float velocity, int serial)
    {
        voices[current].releasing = true;         // alte Note blendet kurz aus
        voices[current].fastRelease = true;
        current ^= 1;

        const Kit& k = kitFor (params.kit);
        Voice& v = voices[current];
        v = Voice();
        v.active   = true;
        v.serial   = serial;
        v.pitch    = v.target = (double) note;
        v.vel      = 0.45f + 0.55f * std::clamp (velocity, 0.0f, 1.0f);
        v.punchEnv = 1.0f;
        v.clickEnv = 1.0f;
        v.kit      = k;
    }

    // Legato: der klingende Ton gleitet zur neuen Tonhoehe (808-Slide)
    void glideTo (int note, int serial)
    {
        Voice& v = voices[current];
        if (! v.active || v.releasing)
        {
            noteOn (note, 0.8f, serial);
            return;
        }
        v.target = (double) note;
        v.serial = serial;
        v.env = std::max (v.env, 0.55f);          // Slide bekommt wieder etwas Druck
    }

    void noteOff (int serial)
    {
        Voice& v = voices[current];
        if (v.active && v.serial == serial)
            v.releasing = true;
    }

    void allOff()
    {
        for (auto& v : voices) { v.releasing = true; }
    }

    // schreibt mono in "out" (addiert)
    void render (float* out, int numSamples)
    {
        if (numSamples <= 0 || out == nullptr) return;

        const float dist  = std::clamp (params.distortion, 0.0f, 1.0f);
        const float toneK = std::clamp (params.tone, 0.0f, 1.0f);
        const float punch = std::clamp (params.punch, 0.0f, 1.0f);
        const float decK  = std::clamp (params.decay, 0.0f, 1.0f);
        const float subK  = std::clamp (params.sub, 0.0f, 1.0f);
        const float level = std::clamp (params.level, 0.0f, 1.0f);

        // Glide: 15 ms .. 450 ms, als Halbtoene pro Sample exponentiell angenaehert
        const double glideTime = 0.015 * std::pow (30.0, (double) std::clamp (params.glide, 0.0f, 1.0f));
        const double glideCoef = 1.0 - std::exp (-1.0 / (glideTime * 0.35 * sr));

        for (auto& v : voices)
        {
            if (! v.active) continue;
            const Kit& k = v.kit;

            const double decayTime = (double) k.decay * (0.25 + 1.75 * (double) decK);
            const float  decayCoef = (float) std::exp (-1.0 / (decayTime * sr));
            const float  relCoef   = (float) std::exp (-1.0 / ((v.fastRelease ? 0.004 : 0.045) * sr));
            const float  punchCoef = (float) std::exp (-1.0 / ((double) k.punchTime * sr));
            const float  clickCoef = (float) std::exp (-1.0 / (0.0035 * sr));
            const float  attStep   = (float) (1.0 / (0.0015 * sr));
            const float  punchSemis = k.punch * (0.2f + 1.6f * punch);
            const float  drive = k.drive * (1.0f + 10.0f * dist * dist);
            const float  makeup = 1.0f / (0.6f + 0.4f * std::min (drive, 6.0f) * 0.4f);
            const float  cutoff = std::min (0.45f * (float) sr, k.tone * (float) std::pow (2.0, ((double) toneK - 0.6) * 5.0));
            float lpA = cutoff * (float) (6.283185307179586 / sr);
            lpA = lpA / (1.0f + lpA);

            for (int i = 0; i < numSamples; ++i)
            {
                // Tonhoehe: Glide + Pitch-Huellkurve
                v.pitch += (v.target - v.pitch) * glideCoef;
                v.punchEnv *= punchCoef;
                const double freq = 440.0 * std::pow (2.0, (v.pitch + (double) (punchSemis * v.punchEnv) - 69.0) / 12.0);
                const double inc = freq / sr;
                v.phase += inc;
                if (v.phase >= 1.0) v.phase -= 1.0;
                v.phase2 += inc * 1.007;
                if (v.phase2 >= 1.0) v.phase2 -= 1.0;

                const float sine = (float) std::sin (6.283185307179586 * v.phase);
                float osc = sine;
                switch (k.wave)
                {
                    case 1:  osc = (float) (4.0 * std::abs (v.phase - 0.5) - 1.0) * -1.0f; break;                     // Dreieck
                    case 2:  osc = std::tanh (sine * 4.0f); break;                                                     // rundes Rechteck
                    case 3:  osc = (float) ((2.0 * v.phase - 1.0) * 0.5 + (2.0 * v.phase2 - 1.0) * 0.5); break;        // zwei verstimmte Saegen
                    default: break;
                }
                if (k.h2 > 0.0f) osc += k.h2 * (float) std::sin (12.566370614359172 * v.phase);
                if (k.h3 > 0.0f) osc += k.h3 * (float) std::sin (18.849555921538759 * v.phase);

                // Klick am Anfang
                v.clickEnv *= clickCoef;
                noiseSeed = noiseSeed * 1664525u + 1013904223u;
                osc += k.click * (0.3f + 0.7f * punch) * v.clickEnv * (float) (int32_t) noiseSeed * (1.0f / 2147483648.0f);

                // Verzerrung
                float x = osc * drive;
                switch (k.shape)
                {
                    case 1:  x = std::clamp (x, -1.0f, 1.0f); break;                                   // harter Clip
                    case 2:  x = std::sin (x * 1.2f); break;                                           // Foldback
                    case 3:  x = x >= 0.0f ? std::tanh (x) : std::tanh (x * 0.6f) * 1.25f; break;      // asymmetrisch (Roehre)
                    default: x = std::tanh (x); break;
                }
                x *= makeup;

                // Tiefpass (zweipolig)
                v.lp1 += lpA * (x - v.lp1);
                v.lp2 += lpA * (v.lp1 - v.lp2);
                x = v.lp2;

                // sauberer Sub darunter, damit das Fundament trotz Verzerrung bleibt
                x = x * (1.0f - 0.35f * subK) + sine * 0.9f * subK;

                // Lautstaerke
                if (v.att < 1.0f) v.att = std::min (1.0f, v.att + attStep);
                v.env *= v.releasing ? relCoef : decayCoef;
                if (v.env < 0.0004f) { v.active = false; break; }

                out[i] += x * v.env * v.att * v.vel * k.gain * level * 0.8f;
            }
        }

        for (int i = 0; i < numSamples; ++i)
            out[i] = std::tanh (out[i]);
    }

private:
    struct Kit
    {
        int   wave;                 // 0 Sinus, 1 Dreieck, 2 Rechteck, 3 Saegen
        float punch, punchTime;     // Halbtoene, Sekunden
        float click;
        float decay;                // Sekunden
        float drive;
        int   shape;                // 0 weich, 1 hart, 2 Foldback, 3 asymmetrisch
        float h2, h3;
        float tone;                 // Hz
        float gain;
    };

    static const Kit& kitFor (int idx)
    {
        static const Kit k[kNumKits] = {
            /* Clean Sub     */ { 0,  6.0f, 0.030f, 0.05f, 1.6f,  1.0f, 0, 0.0f,  0.0f,  2500.0f, 1.00f },
            /* Classic       */ { 0, 12.0f, 0.045f, 0.25f, 1.1f,  1.6f, 0, 0.08f, 0.0f,  3500.0f, 0.95f },
            /* Punchy        */ { 0, 24.0f, 0.030f, 0.50f, 0.8f,  2.2f, 0, 0.10f, 0.05f, 5000.0f, 0.90f },
            /* Long Boy      */ { 0, 10.0f, 0.050f, 0.15f, 2.6f,  1.8f, 0, 0.06f, 0.0f,  3000.0f, 0.92f },
            /* Dark Drill    */ { 1, 14.0f, 0.040f, 0.30f, 1.8f,  3.0f, 3, 0.12f, 0.06f, 2200.0f, 0.95f },
            /* Rage          */ { 2, 12.0f, 0.030f, 0.35f, 1.4f,  5.0f, 1, 0.20f, 0.15f, 6000.0f, 0.62f },
            /* Phonk Clip    */ { 0, 18.0f, 0.035f, 0.40f, 1.2f,  9.0f, 1, 0.10f, 0.10f, 7000.0f, 0.62f },
            /* Detroit Knock */ { 0, 30.0f, 0.022f, 0.70f, 0.45f, 2.5f, 0, 0.10f, 0.0f,  6000.0f, 0.95f },
            /* Boom-Bap Sub  */ { 0,  8.0f, 0.060f, 0.10f, 0.7f,  1.2f, 0, 0.0f,  0.0f,  1800.0f, 1.05f },
            /* Growl         */ { 3,  7.0f, 0.040f, 0.20f, 1.5f,  2.5f, 0, 0.0f,  0.0f,   900.0f, 1.10f },
            /* Fuzz          */ { 1, 12.0f, 0.040f, 0.30f, 1.3f,  7.0f, 2, 0.15f, 0.10f, 4500.0f, 0.70f },
            /* Soft Round    */ { 1,  5.0f, 0.050f, 0.05f, 1.2f,  1.0f, 0, 0.0f,  0.0f,  1500.0f, 1.10f },
            /* Short Pluck   */ { 0, 20.0f, 0.025f, 0.50f, 0.30f, 1.8f, 0, 0.10f, 0.0f,  5000.0f, 1.00f },
            /* Tape Warm     */ { 0, 10.0f, 0.050f, 0.20f, 1.4f,  2.4f, 3, 0.10f, 0.03f, 2800.0f, 0.90f },
            /* Square Sub    */ { 2,  8.0f, 0.040f, 0.15f, 1.2f,  1.5f, 0, 0.0f,  0.0f,  1400.0f, 0.85f },
            /* Glide King    */ { 0,  9.0f, 0.050f, 0.20f, 2.2f,  2.0f, 0, 0.08f, 0.0f,  3200.0f, 0.92f },
        };
        return k[std::clamp (idx, 0, kNumKits - 1)];
    }

    struct Voice
    {
        bool   active = false, releasing = false, fastRelease = false;
        int    serial = -1;
        double pitch = 36.0, target = 36.0, phase = 0.0, phase2 = 0.33;
        float  env = 1.0f, att = 0.0f, punchEnv = 0.0f, clickEnv = 0.0f, vel = 1.0f;
        float  lp1 = 0.0f, lp2 = 0.0f;
        Kit    kit = kitFor (1);
    };

    Voice       voices[2];
    int         current = 0;
    double      sr = 44100.0;
    SoundParams params;
    uint32_t    noiseSeed = 0x9b17c3d5u;
};

} // namespace p808
