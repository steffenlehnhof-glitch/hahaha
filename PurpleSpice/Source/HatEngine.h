// =============================================================================
//  Purple Spice - Hi-Hat-Engine
//  Reines C++17 ohne JUCE: Stile, Grundraster, Rolls, Pitch-Fahrten, Open Hats,
//  Pausen, Flams, Ghost Notes, Swing und MIDI-Export.
//
//  Die Erzeugung ist deterministisch: jeder Takt hat zwei Zufalls-Seeds. Aus
//  Seeds + Reglern + Tempo entsteht immer dasselbe Pattern. Dadurch wirken die
//  Regler live (mehr "Rolls" = es kommen Rolls dazu, der Rest bleibt stehen).
// =============================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace spice
{
constexpr int kMaxBars   = 8;
constexpr int kMaxEvents = 1536;

enum Style { kTrap, kDrill, kRage, kDetroit, kWestCoast, kBoomBap, kLoFi, kPhonk, kNumStyles };
enum Grid  { kGridAuto, kGrid8, kGrid16, kGrid8T, kGrid16T, kNumGrids };
enum Kind  { kHit, kRoll, kOpen, kGhost, kFlam };

inline const char* styleName (int id)
{
    static const char* n[kNumStyles] = { "Trap", "Drill", "Rage / Plugg", "Detroit", "West Coast", "Boom-Bap", "Lo-Fi", "Phonk" };
    return n[std::clamp (id, 0, (int) kNumStyles - 1)];
}

inline const char* gridName (int id)
{
    static const char* n[kNumGrids] = { "Auto", "1/8", "1/16", "1/8 Triolen", "1/16 Triolen" };
    return n[std::clamp (id, 0, (int) kNumGrids - 1)];
}

// ----------------------------------------------------------------------------
struct Profile
{
    float accent[4];       // Lautstaerke je 16tel-Position im Beat (1, e, +, a)
    float velScale;
    float swing;           // Vorschlag fuer den Swing-Regler
    float rollBias, openBias, gapBias, ghostBias, tripletBias, pitchBias;
    int   mask16;          // Bitmaske der gespielten 16tel (0 = jedes Rasterfeld)
};

inline const Profile& profile (int style)
{
    static const Profile p[kNumStyles] = {
        /* Trap       */ { { 1.0f, 0.62f, 0.80f, 0.62f }, 1.0f,  0.0f,  1.0f, 0.6f, 0.7f, 0.3f, 1.0f, 1.0f, 0 },
        /* Drill      */ { { 1.0f, 0.70f, 0.85f, 0.70f }, 1.0f,  0.0f,  0.8f, 1.0f, 0.3f, 0.7f, 1.5f, 0.6f, 0x4949 },   // x..x..x.x..x..x.
        /* Rage       */ { { 1.0f, 0.60f, 0.90f, 0.60f }, 1.0f,  0.0f,  0.7f, 1.3f, 0.5f, 0.2f, 0.6f, 0.8f, 0 },
        /* Detroit    */ { { 1.0f, 0.86f, 0.93f, 0.86f }, 1.0f,  0.0f,  0.5f, 0.5f, 1.0f, 0.0f, 0.3f, 0.3f, 0 },
        /* West Coast */ { { 0.8f, 0.60f, 1.00f, 0.60f }, 0.96f, 0.15f, 0.5f, 1.2f, 0.4f, 0.5f, 0.5f, 0.4f, 0 },
        /* Boom-Bap   */ { { 1.0f, 0.50f, 0.76f, 0.50f }, 0.94f, 0.5f,  0.25f, 0.9f, 0.3f, 1.0f, 0.5f, 0.1f, 0 },
        /* Lo-Fi      */ { { 0.9f, 0.45f, 0.66f, 0.45f }, 0.8f,  0.55f, 0.2f, 0.5f, 0.5f, 1.2f, 0.4f, 0.15f, 0 },
        /* Phonk      */ { { 1.0f, 0.70f, 0.86f, 0.70f }, 1.0f,  0.0f,  1.1f, 0.5f, 0.5f, 0.2f, 0.6f, 0.7f, 0 },
    };
    return p[std::clamp (style, 0, (int) kNumStyles - 1)];
}

// ----------------------------------------------------------------------------
struct Hit
{
    double start    = 0.0;   // in Vierteln ab Taktanfang [0, 4)
    double length   = 0.1;
    int    velocity = 100;
    int    pitch    = 0;     // Halbtoene relativ zum Grundton
    int    kind     = kHit;
};

struct Bar
{
    uint32_t base = 1, var = 1;   // Seeds: base = ganzer Takt, var = zweite Takthaelfte
    bool locked = false;
    std::vector<Hit> hits;        // aktueller Inhalt (bei locked eingefroren)
};

struct Settings
{
    int   style    = kTrap;
    int   bars     = 4;
    int   grid     = kGridAuto;
    float rolls    = 0.5f;
    float spice    = 0.5f;
    float open     = 0.3f;
    float dynamics = 0.6f;
    float swing    = 0.0f;
    float humanize = 0.15f;
    float level    = 0.8f;
    bool  pitch    = true;    // Pitch-Fahrten in Rolls + einzelne gepitchte Hits
    bool  triplets = true;
    bool  gaps     = true;
    bool  flams    = true;
    int   kit      = 1;       // 0 = aus
    int   rootNote = 60;      // C5 in FL Studio
    bool  hostSync = true;
};

struct NoteEvent
{
    double start = 0.0, length = 0.1;   // in Vierteln ab Loop-Anfang
    int velocity = 100, pitch = 0, kind = kHit;
};

struct PatternData
{
    int    numEvents  = 0;
    int    numBars    = 0;
    double totalBeats = 0.0;
    NoteEvent events[kMaxEvents];
};

struct Stats { int hits = 0, rolls = 0, opens = 0; };

// ----------------------------------------------------------------------------
//  Engine
// ----------------------------------------------------------------------------
class Engine
{
public:
    Settings         s;
    std::vector<Bar> bars;
    uint32_t         humanSeed = 1;
    double           bpm = 140.0;      // Tempo, fuer das erzeugt wird (in 5er-Schritten)

    explicit Engine (uint32_t seed = 0x5a1c3u) : rng (seed) {}

    void reseed (uint32_t seed)          { rng.seed (seed); }
    static double bucket (double tempo)  { return std::clamp (std::round (tempo / 5.0) * 5.0, 40.0, 300.0); }

    // ---- Aktionen ------------------------------------------------------------
    void generate()
    {
        const int n = std::clamp (s.bars, 1, kMaxBars);
        bars.resize ((size_t) n);

        const uint32_t seedA = (uint32_t) rng(), seedB = (uint32_t) rng();
        for (int i = 0; i < n; ++i)
        {
            Bar& b = bars[(size_t) i];
            if (b.locked) continue;

            // Phrasen-Logik: A B A' B' - gleiche Basis, andere zweite Takthaelfte
            const bool motif = (uint32_t) rng() % 100u < 62u;
            b.base = motif ? (i % 2 == 0 ? seedA : seedB) : (uint32_t) rng();
            b.var  = (uint32_t) rng();
        }
        humanSeed = (uint32_t) rng();
        rebuild();
    }

    // Variation: erste Takthaelften bleiben, das Ear Candy danach wird neu verteilt
    void vary()
    {
        for (auto& b : bars)
            if (! b.locked)
                b.var = (uint32_t) rng();
        humanSeed = (uint32_t) rng();
        rebuild();
    }

    void rerollBar (int i)
    {
        if (i < 0 || i >= (int) bars.size() || bars[(size_t) i].locked)
            return;
        bars[(size_t) i].base = (uint32_t) rng();
        bars[(size_t) i].var  = (uint32_t) rng();
        rebuild();
    }

    void resize (int n)
    {
        n = std::clamp (n, 1, kMaxBars);
        s.bars = n;
        const int old = (int) bars.size();
        bars.resize ((size_t) n);
        for (int i = old; i < n; ++i)
        {
            bars[(size_t) i].base = i >= 2 ? bars[(size_t) i - 2].base : (uint32_t) rng();
            bars[(size_t) i].var  = (uint32_t) rng();
        }
        rebuild();
    }

    // Alle nicht festgehaltenen Takte aus Seeds + Reglern + Tempo neu berechnen
    void rebuild()
    {
        const int n = (int) bars.size();
        for (int i = 0; i < n; ++i)
            if (! bars[(size_t) i].locked)
                bars[(size_t) i].hits = makeBar (bars[(size_t) i], i, n);
    }

    void applyStyleDefaults()            { s.swing = profile (s.style).swing; }

    // ---- Raster ---------------------------------------------------------------
    double baseStep() const
    {
        switch (s.grid)
        {
            case kGrid8:   return 0.5;
            case kGrid16:  return 0.25;
            case kGrid8T:  return 1.0 / 3.0;
            case kGrid16T: return 1.0 / 6.0;
            default: break;
        }

        // Auto: je nach Stil und Tempo
        switch (s.style)
        {
            case kTrap:
            case kRage:      return bpm >= 118.0 ? 0.5 : 0.25;
            case kDrill:     return 0.25;
            case kDetroit:   return bpm >= 150.0 ? 0.5 : 0.25;
            case kWestCoast: return bpm >= 125.0 ? 0.5 : 0.25;
            case kPhonk:     return bpm >= 150.0 ? 0.5 : 0.25;
            default:         return 0.5;     // Boom-Bap, Lo-Fi
        }
    }

    bool usesMask() const   { return s.grid == kGridAuto && profile (s.style).mask16 != 0; }

    // Millisekunden zwischen zwei Schlaegen bei "perBeat" Schlaegen pro Viertel
    double intervalMs (int perBeat) const   { return 60000.0 / bpm / (double) perBeat; }

    // ---- Ausgabe ----------------------------------------------------------------
    void build (PatternData& out) const
    {
        out.numBars    = std::min ((int) bars.size(), kMaxBars);
        out.totalBeats = 4.0 * out.numBars;
        out.numEvents  = 0;

        std::mt19937 hr (humanSeed);
        std::uniform_real_distribution<float> uni (-1.0f, 1.0f);
        const float h  = std::clamp (s.humanize, 0.0f, 1.0f);
        const float sw = std::clamp (s.swing, 0.0f, 1.0f);

        for (int b = 0; b < out.numBars; ++b)
        {
            for (const Hit& hit : bars[(size_t) b].hits)
            {
                if (out.numEvents >= kMaxEvents) break;

                double start = hit.start;
                float  vel   = (float) hit.velocity;

                if (hit.kind != kRoll && hit.kind != kFlam)
                {
                    // Swing: ungerade 16tel nach hinten (1.0 = Triolen-Gefuehl)
                    const double p16 = start * 4.0;
                    const long   q   = std::lround (p16);
                    if (std::abs (p16 - (double) q) < 0.02 && (q & 1) != 0)
                        start += sw * (1.0 / 12.0);

                    start += h * 0.012 * uni (hr);
                }
                vel += h * 9.0f * uni (hr);

                NoteEvent& e = out.events[out.numEvents++];
                e.start    = std::clamp (4.0 * b + start, 0.0, out.totalBeats - 0.02);
                e.length   = std::max (0.02, hit.length);
                e.velocity = std::clamp ((int) std::lround (vel), 1, 127);
                e.pitch    = std::clamp (hit.pitch, -24, 24);
                e.kind     = hit.kind;
            }
        }

        std::stable_sort (out.events, out.events + out.numEvents,
                          [] (const NoteEvent& a, const NoteEvent& b) { return a.start < b.start; });

        // Laengen: nichts ueberlappt den naechsten Schlag, Open Hats klingen bis dorthin
        for (int i = 0; i < out.numEvents; ++i)
        {
            NoteEvent& e = out.events[i];
            const double next = i + 1 < out.numEvents ? out.events[i + 1].start : out.totalBeats;
            const double room = std::max (0.015, next - e.start - 0.004);
            e.length = e.kind == kOpen ? std::min (room, 0.75) : std::min (e.length, room);
            if (e.start + e.length > out.totalBeats)
                e.length = std::max (0.01, out.totalBeats - e.start - 0.002);
        }
    }

    Stats stats() const
    {
        Stats st;
        for (const auto& b : bars)
        {
            int prevKind = -1;
            double prevEnd = -1.0;
            for (const auto& h : b.hits)
            {
                ++st.hits;
                if (h.kind == kOpen) ++st.opens;
                if (h.kind == kRoll && (prevKind != kRoll || h.start - prevEnd > 0.3)) ++st.rolls;
                prevKind = h.kind;
                prevEnd = h.start;
            }
        }
        return st;
    }

private:
    std::mt19937 rng;

    enum Purpose { P_ROLL = 1, P_ROLLLEN, P_SUB, P_VELSHAPE, P_PITCHON, P_PITCHSHAPE, P_PITCHRANGE, P_ACCEL,
                   P_GAP, P_OPEN, P_GHOST, P_PITCHHIT, P_PITCHVAL, P_FLAM, P_ENDLEN };

    // Zufallszahl 0..1, die nur von (seed, a, b) abhaengt
    static float u01 (uint32_t seed, uint32_t a, uint32_t b)
    {
        uint32_t h = seed * 0x9E3779B1u;
        h ^= (a + 0x7F4A7C15u) * 0x85EBCA6Bu;
        h ^= (b + 0x165667B1u) * 0xC2B2AE35u;
        h ^= h >> 15; h *= 0x2C1B3C6Du;
        h ^= h >> 12; h *= 0x297A2D39u;
        h ^= h >> 15;
        return (float) (h >> 8) / 16777216.0f;
    }

    struct RollSpec
    {
        int perBeat = 8, perBeat2 = 8;   // zweite Haelfte kann schneller werden
        int velShape = 0;                // 0 flach, 1 lauter, 2 leiser, 3 Delle
        int pitchShape = 0;              // 0 keine, 1 hoch, 2 runter, 3 runter-hoch, 4 Treppe, 5 Oktave, 6 Wechsel
        int pitchRange = 0;
    };

    bool subdivisionOk (int perBeat, double step, int lenSlots) const
    {
        const bool triplet = perBeat == 3 || perBeat == 6 || perBeat == 12;
        if (triplet && ! s.triplets) return false;
        if (intervalMs (perBeat) < 19.0) return false;                 // schneller klingt nur noch nach Rauschen
        if (perBeat == 3 && lenSlots < 2) return false;                // 1/8-Triolen brauchen einen ganzen Beat

        const double basePerBeat = 1.0 / step;
        if (std::abs ((double) perBeat - basePerBeat) < 0.01) return false;    // gleich schnell wie das Raster = kein Roll
        if ((double) perBeat < basePerBeat && perBeat != 3) return false;      // langsamer nur als Triolen-Wechsel
        return true;
    }

    RollSpec makeRollSpec (uint32_t seed, int slot, int lenSlots, double step, const Profile& pr) const
    {
        RollSpec r;
        static const int subs[6] = { 4, 8, 16, 6, 12, 3 };
        float w[6], sum = 0.0f;

        for (int i = 0; i < 6; ++i)
        {
            w[i] = 0.0f;
            if (! subdivisionOk (subs[i], step, lenSlots)) continue;

            const double ms = intervalMs (subs[i]);
            const double d  = (ms - 58.0) / 40.0;
            float weight = (float) std::exp (-d * d) + 0.12f;
            if (subs[i] == 3 || subs[i] == 6 || subs[i] == 12) weight *= pr.tripletBias;
            if (subs[i] == 16) weight *= 0.6f;
            if (subs[i] == 3)  weight *= 0.5f;
            w[i] = weight;
            sum += weight;
        }

        if (sum <= 0.0f)
        {
            r.perBeat = r.perBeat2 = step > 0.26 ? 4 : 8;
        }
        else
        {
            float x = u01 (seed, (uint32_t) slot, P_SUB) * sum;
            int pick = 0;
            for (int i = 0; i < 6; ++i)
            {
                if (w[i] <= 0.0f) continue;
                pick = i;
                if (x < w[i]) break;
                x -= w[i];
            }
            r.perBeat = r.perBeat2 = subs[pick];
        }

        // Beschleunigen: zweite Haelfte doppelt so schnell
        if (lenSlots >= 2 && r.perBeat != 3 && u01 (seed, (uint32_t) slot, P_ACCEL) < 0.22f * s.spice
            && subdivisionOk (r.perBeat * 2, step, lenSlots) && r.perBeat * 2 <= 16)
            r.perBeat2 = r.perBeat * 2;

        const float vs = u01 (seed, (uint32_t) slot, P_VELSHAPE);
        r.velShape = vs < 0.28f ? 0 : (vs < 0.62f ? 1 : (vs < 0.86f ? 2 : 3));

        if (s.pitch && u01 (seed, (uint32_t) slot, P_PITCHON) < std::min (0.92f, 0.18f + 0.7f * s.spice * pr.pitchBias))
        {
            static const int ranges[6] = { 3, 5, 7, 12, 12, 7 };
            r.pitchShape = 1 + std::min (5, (int) (u01 (seed, (uint32_t) slot, P_PITCHSHAPE) * 6.0f));
            r.pitchRange = ranges[std::min (5, (int) (u01 (seed, (uint32_t) slot, P_PITCHRANGE) * 6.0f))];
        }
        return r;
    }

    void emitRoll (std::vector<Hit>& out, const RollSpec& r, double t0, double len) const
    {
        struct P { double t, step; };
        std::vector<P> pts;

        if (r.perBeat2 == r.perBeat)
        {
            const int n = std::max (1, (int) std::lround (len * r.perBeat));
            for (int j = 0; j < n; ++j) pts.push_back ({ t0 + (double) j / r.perBeat, 1.0 / r.perBeat });
        }
        else
        {
            const double half = len * 0.5;
            const int n1 = std::max (1, (int) std::lround (half * r.perBeat));
            const int n2 = std::max (1, (int) std::lround (half * r.perBeat2));
            for (int j = 0; j < n1; ++j) pts.push_back ({ t0 + (double) j / r.perBeat, 1.0 / r.perBeat });
            for (int j = 0; j < n2; ++j) pts.push_back ({ t0 + half + (double) j / r.perBeat2, 1.0 / r.perBeat2 });
        }

        const int n = (int) pts.size();
        const float depth = 0.35f + 0.65f * std::clamp (s.dynamics, 0.0f, 1.0f);
        const float lo = 56.0f, hi = 112.0f, mid = 92.0f;
        const float pi = 3.14159265f;

        for (int j = 0; j < n; ++j)
        {
            const float x = n > 1 ? (float) j / (float) (n - 1) : 0.0f;

            float v = mid;
            switch (r.velShape)
            {
                case 1:  v = lo + (hi - lo) * x; break;
                case 2:  v = hi - (hi - lo) * x; break;
                case 3:  v = hi - (hi - lo) * std::sin (pi * x); break;
                default: v = (j == 0) ? 104.0f : mid; break;
            }
            v = mid + (v - mid) * depth;

            int p = 0;
            const float range = (float) r.pitchRange;
            switch (r.pitchShape)
            {
                case 1: p = (int) std::lround (range * x); break;
                case 2: p = -(int) std::lround (range * x); break;
                case 3: p = -(int) std::lround (range * std::sin (pi * x)); break;
                case 4: p = std::min (3, (int) (x * 4.0f)) * std::max (1, r.pitchRange / 4); break;
                case 5: p = 12; break;
                case 6: p = (j % 2 == 1) ? r.pitchRange : 0; break;
                default: break;
            }

            Hit h;
            h.start    = pts[(size_t) j].t;
            h.length   = pts[(size_t) j].step * 0.9;
            h.velocity = std::clamp ((int) std::lround (v * profile (s.style).velScale), 1, 127);
            h.pitch    = p;
            h.kind     = kRoll;
            out.push_back (h);
        }
    }

    std::vector<Hit> makeBar (const Bar& bar, int barIdx, int nBars) const
    {
        const Profile& pr = profile (s.style);
        const double step = baseStep();
        const bool mask   = usesMask();
        const bool lastBar = barIdx == nBars - 1;
        const float intensity = lastBar ? 1.6f : (barIdx % 2 == 1 ? 1.2f : (barIdx == 0 ? 0.7f : 1.0f));
        const float spiceAmt = std::clamp (s.spice, 0.0f, 1.0f);
        const float rollAmt  = std::clamp (s.rolls, 0.0f, 1.0f);
        const float openAmt  = std::clamp (s.open, 0.0f, 1.0f);
        const float dyn      = std::clamp (s.dynamics, 0.0f, 1.0f);

        const uint32_t seedA = bar.base;
        const uint32_t seedB = bar.base ^ (bar.var * 0x9E3779B1u + 0x51ED27u);
        auto seedFor = [&] (int slot) { return slot >= 4 ? seedB : seedA; };
        auto U  = [&] (int slot, int purpose) { return u01 (seedFor (slot), (uint32_t) slot, (uint32_t) purpose); };
        auto UH = [&] (double pos, int purpose)
        {
            const int slot = std::clamp ((int) (pos * 2.0 + 1.0e-6), 0, 7);
            return u01 (seedFor (slot), 1000u + (uint32_t) std::lround (pos * 24.0), (uint32_t) purpose);
        };

        enum { BASE, ROLL, GAP };
        int mode[8] = { BASE, BASE, BASE, BASE, BASE, BASE, BASE, BASE };

        std::vector<Hit> hits;

        // ---- 1) Rolls: Halbe-Beat-Felder aussuchen ----------------------------------
        static const float posW[8] = { 0.05f, 0.35f, 0.5f, 0.9f, 0.3f, 0.6f, 0.9f, 1.6f };
        const int maxRolls = rollAmt < 0.03f ? 0 : 1 + (int) std::floor (rollAmt * 2.2f * intensity);

        struct Cand { int slot; float score; bool ending; };
        std::vector<Cand> cands;
        for (int k = 0; k < 8; ++k)
        {
            float p = rollAmt * pr.rollBias * intensity * posW[k] * 0.55f;
            const bool ending = lastBar && k == 7 && rollAmt > 0.12f;   // Abschluss-Roll am Ende der Phrase
            const float r = U (k, P_ROLL);
            if (ending)       cands.push_back ({ k, -1.0f, true });
            else if (r < p)   cands.push_back ({ k, r / p, false });
        }
        std::sort (cands.begin(), cands.end(), [] (const Cand& a, const Cand& b) { return a.score < b.score; });

        int placed = 0;
        for (const Cand& c : cands)
        {
            if (placed >= maxRolls) break;

            int k = c.slot, len = 1;
            const float lr = U (k, P_ROLLLEN);
            if (c.ending)
            {
                const float er = U (k, P_ENDLEN);
                if (er < 0.5f)                          { k = 6; len = 2; }
                if (er < 0.12f * (1.0f + rollAmt))      { k = 4; len = 4; }
            }
            else
            {
                if (k <= 6 && lr < 0.38f) len = 2;
                if (k <= 4 && lr < 0.05f * (1.0f + rollAmt)) len = 4;
            }

            auto fits = [&] (int start, int length)
            {
                for (int i = start; i < start + length; ++i)
                    if (i > 7 || mode[i] != BASE) return false;
                if (rollAmt < 0.8f)
                {
                    if (start > 0 && mode[start - 1] == ROLL) return false;
                    if (start + length < 8 && mode[start + length] == ROLL) return false;
                }
                return true;
            };

            if (! fits (k, len)) { k = c.slot; len = 1; }
            if (! fits (k, len)) continue;

            for (int i = k; i < k + len; ++i) mode[i] = ROLL;
            emitRoll (hits, makeRollSpec (seedFor (k), k, len, step, pr), 0.5 * k, 0.5 * len);
            ++placed;
        }

        // ---- 2) Pausen ------------------------------------------------------------
        if (s.gaps)
        {
            int gapsPlaced = 0;
            for (int k = 1; k < 8 && gapsPlaced < 2; ++k)
            {
                if (mode[k] != BASE) continue;
                float p = 0.16f * spiceAmt * pr.gapBias * (k % 2 == 1 ? 1.3f : 0.5f);
                if (mode[k - 1] == ROLL) p *= 1.8f;                 // Luft holen nach einem Roll
                if (U (k, P_GAP) < p) { mode[k] = GAP; ++gapsPlaced; }
            }
        }

        // ---- 3) Grundraster mit Betonungen und Verzierungen ---------------------------
        std::vector<double> grid;
        if (mask)
        {
            for (int i = 0; i < 16; ++i)
                if ((pr.mask16 >> i) & 1)
                    grid.push_back (0.25 * i);
        }
        else
        {
            const int n = (int) std::lround (4.0 / step);
            for (int i = 0; i < n; ++i) grid.push_back (step * i);
        }

        const int maxOpen = openAmt < 0.03f ? 0 : 1 + (openAmt > 0.55f ? 1 : 0) + (openAmt > 0.85f ? 1 : 0);
        const double grace = std::clamp (0.028 * bpm / 60.0, 0.03, 0.12);
        int opens = 0;
        double openUntil = -1.0;

        auto slotOf = [] (double pos) { return std::clamp ((int) (pos * 2.0 + 1.0e-6), 0, 7); };

        for (double pos : grid)
        {
            if (mode[slotOf (pos)] != BASE) continue;
            if (pos < openUntil) continue;

            const double frac = pos - std::floor (pos);
            const double p16  = frac * 4.0;
            const int    q16  = (int) std::lround (p16);
            const bool onGrid16 = std::abs (p16 - q16) < 0.05;
            const int  q = onGrid16 ? (q16 & 3) : 2;

            float vel = 104.0f * (1.0f - dyn * (1.0f - pr.accent[q])) * pr.velScale;
            if (q == 0 && ((int) std::floor (pos) % 2) == 0) vel += 4.0f;

            Hit h;
            h.start    = pos;
            h.length   = std::min (step * 0.9, 0.24);
            h.velocity = std::clamp ((int) std::lround (vel), 1, 127);
            h.kind     = kHit;

            // Open Hat auf dem Offbeat
            const bool offbeat = onGrid16 && q == 2;
            if (offbeat && opens < maxOpen)
            {
                const int beat = (int) std::floor (pos);
                static const float openW[4] = { 0.25f, 0.6f, 0.35f, 1.0f };
                const float p = openAmt * pr.openBias * openW[beat & 3] * (lastBar ? 1.15f : 0.95f);
                if (UH (pos, P_OPEN) < p)
                {
                    h.kind = kOpen;
                    h.length = 0.5;
                    h.velocity = std::clamp ((int) std::lround (102.0f * pr.velScale), 1, 127);
                    openUntil = pos + 0.26;      // die Open Hat darf mindestens ein 16tel klingen
                    ++opens;
                }
            }

            // einzelner gepitchter Schlag
            if (h.kind == kHit && s.pitch && pos > 0.01 && UH (pos, P_PITCHHIT) < 0.07f * spiceAmt * pr.pitchBias)
            {
                static const int vals[6] = { 12, -5, 7, -12, 5, 3 };
                h.pitch = vals[std::min (5, (int) (UH (pos, P_PITCHVAL) * 6.0f))];
            }

            // Flam: leiser Vorschlag kurz vor einem betonten Schlag
            if (h.kind == kHit && s.flams && q == 0 && onGrid16 && pos > 0.2
                && mode[slotOf (pos - grace)] == BASE && UH (pos, P_FLAM) < 0.1f * spiceAmt)
            {
                Hit f;
                f.start    = pos - grace;
                f.length   = grace * 0.8;
                f.velocity = std::max (1, (int) (h.velocity * 0.5f));
                f.pitch    = h.pitch;
                f.kind     = kFlam;
                hits.push_back (f);
            }

            hits.push_back (h);

            // Ghost Note auf dem 16tel dazwischen (nur bei 1/8-Raster)
            if (! mask && h.kind == kHit && step > 0.49 && step < 0.51 && pos + 0.25 < 4.0
                && mode[slotOf (pos + 0.25)] == BASE
                && UH (pos + 0.25, P_GHOST) < 0.14f * spiceAmt * pr.ghostBias * (0.5f + dyn))
            {
                Hit g;
                g.start    = pos + 0.25;
                g.length   = 0.2;
                g.velocity = 36 + (int) (UH (pos + 0.25, P_PITCHVAL) * 16.0f);
                g.kind     = kGhost;
                hits.push_back (g);
            }
        }

        // Ghost Notes in den Luecken eines Masken-Patterns (Drill)
        if (mask)
        {
            for (int i = 0; i < 16; ++i)
            {
                const double pos = 0.25 * i;
                if (((pr.mask16 >> i) & 1) || mode[slotOf (pos)] != BASE || pos < openUntil) continue;
                if (UH (pos, P_GHOST) < 0.2f * spiceAmt * pr.ghostBias)
                {
                    Hit g;
                    g.start    = pos;
                    g.length   = 0.2;
                    g.velocity = 38 + (int) (UH (pos, P_PITCHVAL) * 18.0f);
                    g.kind     = kGhost;
                    hits.push_back (g);
                }
            }
        }

        // ---- 4) aufraeumen ----------------------------------------------------------
        std::stable_sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.start < b.start; });

        auto priority = [] (int kind) { return kind == kOpen ? 3 : (kind == kRoll ? 2 : (kind == kHit ? 1 : 0)); };
        std::vector<Hit> clean;
        for (const Hit& h : hits)
        {
            if (h.start < -1.0e-9 || h.start >= 4.0 - 1.0e-9) continue;
            if (! clean.empty() && h.start - clean.back().start < 0.012)
            {
                if (priority (h.kind) > priority (clean.back().kind))
                    clean.back() = h;
                continue;
            }
            clean.push_back (h);
        }
        return clean;
    }
};

// ----------------------------------------------------------------------------
//  Standard-MIDI-Datei (Format 0, 480 PPQ).  which: 0 = Hi-Hats, 1 = Open Hats
// ----------------------------------------------------------------------------
inline int countEvents (const PatternData& p, int which)
{
    int n = 0;
    for (int i = 0; i < p.numEvents; ++i)
        if ((p.events[i].kind == kOpen) == (which == 1))
            ++n;
    return n;
}

inline std::vector<uint8_t> writeMidiFile (const PatternData& p, double bpm, int rootNote, int which)
{
    struct Ev { uint32_t tick; int on; int note; int vel; };
    std::vector<Ev> evs;
    const double ppq = 480.0;

    for (int i = 0; i < p.numEvents; ++i)
    {
        const NoteEvent& e = p.events[i];
        if ((e.kind == kOpen) != (which == 1))
            continue;

        const int note = std::clamp (rootNote + e.pitch, 0, 127);
        const auto on  = (uint32_t) std::max (0L, std::lround (e.start * ppq));
        const auto off = std::max (on + 6u, (uint32_t) std::max (0L, std::lround ((e.start + e.length) * ppq)));
        evs.push_back ({ on, 1, note, e.velocity });
        evs.push_back ({ off, 0, note, 0 });
    }

    std::stable_sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b)
    {
        return a.tick != b.tick ? a.tick < b.tick : a.on < b.on;   // Note-Off vor Note-On
    });

    std::vector<uint8_t> trk;
    auto vlq = [&trk] (uint32_t v)
    {
        uint8_t buf[5];
        int n = 0;
        buf[n++] = (uint8_t) (v & 0x7f);
        while ((v >>= 7) != 0) buf[n++] = (uint8_t) ((v & 0x7f) | 0x80);
        while (n > 0) trk.push_back (buf[--n]);
    };

    const std::string name = which == 1 ? "Purple Spice Open Hat" : "Purple Spice Hi-Hat";
    vlq (0); trk.push_back (0xff); trk.push_back (0x03); trk.push_back ((uint8_t) name.size());
    trk.insert (trk.end(), name.begin(), name.end());

    const auto us = (uint32_t) std::lround (60000000.0 / std::clamp (bpm, 20.0, 999.0));
    vlq (0); trk.push_back (0xff); trk.push_back (0x51); trk.push_back (0x03);
    trk.push_back ((uint8_t) (us >> 16)); trk.push_back ((uint8_t) (us >> 8)); trk.push_back ((uint8_t) us);
    vlq (0); trk.push_back (0xff); trk.push_back (0x58); trk.push_back (0x04);
    trk.push_back (4); trk.push_back (2); trk.push_back (24); trk.push_back (8);

    // gleiche Tonhoehe darf sich in der Datei nicht ueberlappen
    int sounding[128] = {};
    uint32_t last = 0;
    for (const Ev& e : evs)
    {
        if (e.on && sounding[e.note] > 0)
        {
            vlq (e.tick - last); last = e.tick;
            trk.push_back (0x80); trk.push_back ((uint8_t) e.note); trk.push_back (0);
            sounding[e.note] = 0;
        }
        if (! e.on && sounding[e.note] == 0)
            continue;

        vlq (e.tick - last);
        last = e.tick;
        trk.push_back ((uint8_t) (e.on ? 0x90 : 0x80));
        trk.push_back ((uint8_t) e.note);
        trk.push_back ((uint8_t) e.vel);
        sounding[e.note] = e.on ? 1 : 0;
    }

    const auto endTick = (uint32_t) std::lround (p.totalBeats * ppq);
    vlq (endTick > last ? endTick - last : 0);
    trk.push_back (0xff); trk.push_back (0x2f); trk.push_back (0x00);

    std::vector<uint8_t> out;
    auto be32 = [&out] (uint32_t v) { for (int sft = 24; sft >= 0; sft -= 8) out.push_back ((uint8_t) (v >> sft)); };
    auto be16 = [&out] (uint32_t v) { out.push_back ((uint8_t) (v >> 8)); out.push_back ((uint8_t) v); };

    out.insert (out.end(), { 'M', 'T', 'h', 'd' });
    be32 (6); be16 (0); be16 (1); be16 ((uint32_t) ppq);
    out.insert (out.end(), { 'M', 'T', 'r', 'k' });
    be32 ((uint32_t) trk.size());
    out.insert (out.end(), trk.begin(), trk.end());
    return out;
}

} // namespace spice
