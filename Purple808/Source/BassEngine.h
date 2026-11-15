// =============================================================================
//  Purple 808 - Pattern-Engine
//  Reines C++17 ohne JUCE: Stile mit Rhythmus-Vorlagen, Tonart und Skala,
//  Grundton-Folgen, Slides, Stotter, Oktav-Spruenge, Ghost Notes, MIDI-Export.
//
//  Die Erzeugung ist deterministisch: jeder Takt hat zwei Zufalls-Seeds. Aus
//  Seeds + Reglern + Tempo entsteht immer dasselbe Pattern, deshalb wirken die
//  Regler live.
// =============================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace p808
{
constexpr int kMaxBars   = 8;
constexpr int kMaxEvents = 512;

enum Style { kTrap, kDrill, kRage, kDetroit, kWestCoast, kBoomBap, kPhonk, kJersey, kNumStyles };
enum Scale { kNatMinor, kHarmMinor, kPhrygian, kDorian, kMajor, kNumScales };
enum Kind  { kNote, kSlide, kStutter, kGhost };

inline const char* styleName (int id)
{
    static const char* n[kNumStyles] = { "Trap", "Drill", "Rage / Plugg", "Detroit", "West Coast", "Boom-Bap", "Phonk", "Jersey Club" };
    return n[std::clamp (id, 0, (int) kNumStyles - 1)];
}

inline const char* scaleName (int id)
{
    static const char* n[kNumScales] = { "Moll", "Harmonisch Moll", "Phrygisch", "Dorisch", "Dur" };
    return n[std::clamp (id, 0, (int) kNumScales - 1)];
}

inline const int* scaleSteps (int id)
{
    static const int s[kNumScales][7] = {
        { 0, 2, 3, 5, 7, 8, 10 },
        { 0, 2, 3, 5, 7, 8, 11 },
        { 0, 1, 3, 5, 7, 8, 10 },
        { 0, 2, 3, 5, 7, 9, 10 },
        { 0, 2, 4, 5, 7, 9, 11 },
    };
    return s[std::clamp (id, 0, (int) kNumScales - 1)];
}

inline std::string noteName (int midi)
{
    static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return std::string (n[((midi % 12) + 12) % 12]) + std::to_string (midi / 12);   // FL-Zaehlung: MIDI 60 = C5
}

// ----------------------------------------------------------------------------
struct Profile
{
    const char* rhythms[8];   // 16tel-Raster eines Takts, 'x' = Schlag
    int   numRhythms;
    float slideBias, stutterBias, octaveBias, gateBias;
    float swing;
};

inline const Profile& profile (int style)
{
    static const Profile p[kNumStyles] = {
        /* Trap */       { { "x.........x.....", "x......x..x.....", "x..x......x..x..", "x.....x...x...x.",
                             "x..x..x...x.....", "x.......x..x..x.", "x..x.....x.x....", "x.....x.....x.x." }, 8, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f },
        /* Drill */      { { "x..x..x...x..x..", "x..x...x..x...x.", "x.....x..x.x..x.", "x..x..x..x....x.",
                             "x...x..x..x..x..", "x..x......x.x.x.", "", "" }, 6, 1.6f, 0.8f, 0.7f, 0.95f, 0.0f },
        /* Rage */       { { "x.......x.......", "x.....x.x.......", "x.......x.....x.", "x..x....x..x....",
                             "x.....x...x.....", "x...x...x...x...", "", "" }, 6, 0.7f, 0.9f, 1.2f, 1.0f, 0.0f },
        /* Detroit */    { { "x.x...x.x.x...x.", "x..x.x..x..x.x..", "x.x.x...x.x.x...", "x...x.x.x...x.x.",
                             "x.x..x..x.x..x..", "", "", "" }, 5, 0.5f, 1.2f, 0.6f, 0.7f, 0.0f },
        /* West Coast */ { { "x..x..x...x.x...", "x.....x.x..x..x.", "x..x....x.x..x..", "x...x..x.x....x.",
                             "x..x.x....x..x..", "", "", "" }, 5, 0.8f, 0.7f, 1.1f, 0.75f, 0.15f },
        /* Boom-Bap */   { { "x......x.x......", "x.........x.....", "x.....x...x.....", "x..x......x.....",
                             "x.......x.x.....", "x......x..x...x.", "", "" }, 6, 0.4f, 0.5f, 0.4f, 0.7f, 0.4f },
        /* Phonk */      { { "x...x...x...x...", "x..x..x.x..x..x.", "x...x..xx...x.x.", "x.x.x...x.x.x...",
                             "x...x.x.x...x..x", "", "", "" }, 5, 1.1f, 1.0f, 0.8f, 0.85f, 0.0f },
        /* Jersey */     { { "x...x...x..x..x.", "x...x...x..x.x..", "x..x.x..x..x..x.", "x...x..xx..x..x.",
                             "", "", "", "" }, 4, 0.9f, 0.9f, 0.9f, 0.7f, 0.0f },
    };
    return p[std::clamp (style, 0, (int) kNumStyles - 1)];
}

// ----------------------------------------------------------------------------
struct Hit
{
    double start    = 0.0;    // in Vierteln ab Taktanfang [0, 4)
    double length   = -1.0;   // < 0 = bis zum naechsten Schlag (mal Laenge-Regler), sonst fest
    int    pitch    = 0;      // Halbtoene relativ zum Grundton der Tonart
    int    velocity = 100;
    int    kind     = kNote;
};

struct Bar
{
    uint32_t base = 1, var = 1;   // Seeds: base = ganzer Takt, var = zweite Takthaelfte
    bool locked = false;
    int  root = 0;                // Grundton dieses Takts (Halbtoene relativ zur Tonart)
    std::vector<Hit> hits;
};

struct Settings
{
    int   style    = kTrap;
    int   bars     = 4;
    int   key      = 5;        // F
    int   scale    = kNatMinor;
    int   octave   = 0;        // -1..+1
    float density  = 0.5f;
    float slides   = 0.5f;
    float spice    = 0.5f;
    float movement = 0.5f;     // 0 = bleibt auf dem Grundton, 1 = wandert viel
    float gate     = 0.85f;    // Laenge der Noten
    float swing    = 0.0f;
    bool  stutter  = true;
    bool  octaves  = true;
    bool  triplets = true;
    bool  gaps     = true;
    bool  hostSync = true;

    // Sound
    int   kit        = 2;
    float distortion = 0.3f;
    float tone       = 0.6f;
    float punch      = 0.5f;
    float decay      = 0.6f;
    float glide      = 0.4f;
    float sub        = 0.4f;
    float reverb     = 0.1f;
    float level      = 0.8f;
};

struct NoteEvent
{
    double start = 0.0, length = 0.5;   // in Vierteln ab Loop-Anfang
    int note = 36, velocity = 100, kind = kNote;
};

struct PatternData
{
    int    numEvents  = 0;
    int    numBars    = 0;
    double totalBeats = 0.0;
    NoteEvent events[kMaxEvents];
};

struct Stats { int notes = 0, slides = 0; };

// ----------------------------------------------------------------------------
//  Engine
// ----------------------------------------------------------------------------
class Engine
{
public:
    Settings         s;
    std::vector<Bar> bars;
    uint32_t         progSeed = 1, humanSeed = 1;
    double           bpm = 140.0;      // Tempo, fuer das erzeugt wird (in 5er-Schritten)

    explicit Engine (uint32_t seed = 0x808808u) : rng (seed) {}

    void reseed (uint32_t seed)          { rng.seed (seed); }
    static double bucket (double tempo)  { return std::clamp (std::round (tempo / 5.0) * 5.0, 40.0, 300.0); }

    // tiefster sinnvoller Bereich fuer 808s: Grundton zwischen F und E
    int rootMidi() const   { return 29 + ((s.key - 5) % 12 + 12) % 12 + 12 * std::clamp (s.octave, -1, 1); }

    // ---- Aktionen ------------------------------------------------------------
    void generate()
    {
        const int n = std::clamp (s.bars, 1, kMaxBars);
        bars.resize ((size_t) n);

        const uint32_t seedA = (uint32_t) rng();
        const uint32_t seedB = (uint32_t) rng() % 100u < 50u ? seedA : (uint32_t) rng();   // oft ist Takt 2 = Takt 1
        for (int i = 0; i < n; ++i)
        {
            Bar& b = bars[(size_t) i];
            if (b.locked) continue;
            const bool motif = (uint32_t) rng() % 100u < 78u;
            b.base = motif ? (i % 2 == 0 ? seedA : seedB) : (uint32_t) rng();
            b.var  = (uint32_t) rng();
        }
        progSeed  = (uint32_t) rng();
        humanSeed = (uint32_t) rng();
        rebuild();
    }

    // Variation: Grundtoene und erste Takthaelften bleiben, der Rest wird neu verteilt
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
            bars[(size_t) i].base = i >= 2 ? bars[(size_t) i - 2].base : (i >= 1 ? bars[(size_t) i - 1].base : (uint32_t) rng());
            bars[(size_t) i].var  = (uint32_t) rng();
        }
        rebuild();
    }

    void rebuild()
    {
        const int n = (int) bars.size();
        for (int i = 0; i < n; ++i)
        {
            Bar& b = bars[(size_t) i];
            if (b.locked) continue;
            b.root = barRoot (i, n);
            b.hits = makeBar (b, i, n);
        }
    }

    void applyStyleDefaults()            { s.swing = profile (s.style).swing; }

    double stepMs() const                { return 60000.0 / bpm / 4.0; }   // Dauer eines 16tels

    // ---- Ausgabe ----------------------------------------------------------------
    void build (PatternData& out) const
    {
        out.numBars    = std::min ((int) bars.size(), kMaxBars);
        out.totalBeats = 4.0 * out.numBars;
        out.numEvents  = 0;

        std::mt19937 hr (humanSeed);
        std::uniform_real_distribution<float> uni (-1.0f, 1.0f);
        const float sw = std::clamp (s.swing, 0.0f, 1.0f);
        const int root = rootMidi();

        struct Tmp { double start, fixed; int note, vel, kind; };
        std::vector<Tmp> tmp;

        for (int b = 0; b < out.numBars; ++b)
        {
            for (const Hit& h : bars[(size_t) b].hits)
            {
                double start = h.start;
                const double p16 = start * 4.0;
                const long   q   = std::lround (p16);
                if (h.kind != kStutter && std::abs (p16 - (double) q) < 0.02 && (q & 1) != 0)
                    start += sw * (1.0 / 12.0);        // Stotter bleiben starr, sonst ruecken sie zu eng zusammen

                Tmp t;
                t.start = std::clamp (4.0 * b + start, 0.0, out.totalBeats - 0.05);
                t.fixed = h.length;
                t.note  = std::clamp (root + h.pitch, 12, 96);
                t.vel   = std::clamp ((int) std::lround ((float) h.velocity + 5.0f * uni (hr)), 1, 127);
                t.kind  = h.kind;
                tmp.push_back (t);
            }
        }

        std::stable_sort (tmp.begin(), tmp.end(), [] (const Tmp& a, const Tmp& b) { return a.start < b.start; });

        const double gate = 0.3 + 0.7 * std::clamp (s.gate, 0.0f, 1.0f) * profile (s.style).gateBias;
        const int n = std::min ((int) tmp.size(), kMaxEvents);

        for (int i = 0; i < n; ++i)
        {
            const Tmp& t = tmp[(size_t) i];
            const bool hasNext = i + 1 < n;
            const double next = hasNext ? tmp[(size_t) i + 1].start : out.totalBeats;
            const double gap  = std::max (0.05, next - t.start);

            double len = t.fixed > 0.0 ? std::min (t.fixed, gap) : std::max (0.12, std::min (gap * std::min (gate, 1.0), 8.0));

            // vor einem Slide klingt die Note in den Slide hinein (Legato = Gleiten statt neu anschlagen)
            if (hasNext && tmp[(size_t) i + 1].kind == kSlide)
                len = gap + 0.06;

            len = std::min (len, out.totalBeats - t.start);

            NoteEvent& e = out.events[out.numEvents++];
            e.start    = t.start;
            e.length   = std::max (0.03, len);
            e.note     = t.note;
            e.velocity = t.vel;
            e.kind     = t.kind;
        }
    }

    Stats stats() const
    {
        Stats st;
        for (const auto& b : bars)
            for (const auto& h : b.hits)
            {
                ++st.notes;
                if (h.kind == kSlide) ++st.slides;
            }
        return st;
    }

private:
    std::mt19937 rng;

    enum Purpose { P_TEMPLATE = 1, P_DROP, P_ADD, P_REST, P_APPROACH, P_APPDIR, P_TONE, P_TONEVAL, P_OCT, P_SLIDETO,
                   P_TAIL, P_TAILIV, P_STUT, P_STUTN, P_GHOST, P_TRIP, P_PROG, P_TURN };

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

    bool inScale (int semis) const
    {
        const int* sc = scaleSteps (s.scale);
        const int p = ((semis % 12) + 12) % 12;
        for (int i = 0; i < 7; ++i)
            if (sc[i] == p) return true;
        return false;
    }

    // Grundton eines Takts aus einer typischen Folge von Stufen
    int barRoot (int i, int n) const
    {
        static const int progs[14][4] = {
            { 1, 1, 1, 1 }, { 1, 1, 6, 7 }, { 1, 1, 6, 5 }, { 1, 6, 3, 7 }, { 1, 1, 7, 6 }, { 1, 7, 6, 7 }, { 1, 4, 6, 5 },
            { 1, 3, 6, 7 }, { 1, 1, 4, 5 }, { 1, 6, 7, 1 }, { 6, 7, 1, 1 }, { 1, 2, 1, 7 }, { 1, 5, 6, 4 }, { 1, 1, 3, 4 },
        };

        const float mv = std::clamp (s.movement, 0.0f, 1.0f);
        int deg = 1;
        if (mv >= 0.12f)
        {
            const int choices = 2 + (int) (mv * 12.0f);                 // mehr Bewegung = mehr moegliche Folgen
            const int idx = 1 + (int) (u01 (progSeed, 3, P_PROG) * (float) (choices - 1));
            deg = progs[std::clamp (idx, 0, 13)][i % 4];

            // zweite Phrase bei 8 Takten: letzter Takt als Wendepunkt
            if (n == 8 && i == 7 && u01 (progSeed, 7, P_TURN) < 0.5f)
                deg = u01 (progSeed, 8, P_TURN) < 0.5f ? 5 : 7;
        }

        int offset = scaleSteps (s.scale)[deg - 1];
        if (offset > 6) offset -= 12;        // Basslinie kompakt um den Grundton halten
        return offset;
    }

    // Skalenton direkt unter oder ueber dem Zielton (Durchgangsnote)
    int neighbour (int target, bool below) const
    {
        for (int d = 1; d <= 3; ++d)
        {
            const int cand = below ? target - d : target + d;
            if (inScale (cand)) return cand;
        }
        return target;
    }

    std::vector<Hit> makeBar (const Bar& bar, int barIdx, int nBars) const
    {
        const Profile& pr = profile (s.style);
        const bool lastBar = barIdx == nBars - 1;
        const float intensity = lastBar ? 1.5f : (barIdx % 2 == 1 ? 1.15f : (barIdx == 0 ? 0.8f : 1.0f));
        const float dens  = std::clamp (s.density, 0.0f, 1.0f);
        const float slide = std::clamp (s.slides, 0.0f, 1.0f);
        const float spice = std::clamp (s.spice, 0.0f, 1.0f);
        const float mv    = std::clamp (s.movement, 0.0f, 1.0f);

        const uint32_t seedA = bar.base;
        const uint32_t seedB = bar.base ^ (bar.var * 0x9E3779B1u + 0x808u);
        auto U = [&] (int step, int purpose) { return u01 (step >= 8 ? seedB : seedA, (uint32_t) step, (uint32_t) purpose); };

        // ---- 1) Rhythmus: Vorlage des Stils, vom Dichte-Regler ausgeduennt oder aufgefuellt ----
        const char* tpl = pr.rhythms[std::min (pr.numRhythms - 1, (int) (u01 (seedA, 99, P_TEMPLATE) * (float) pr.numRhythms))];
        bool on[16];
        for (int i = 0; i < 16; ++i) on[i] = tpl[i] == 'x';

        static const float addW[16] = { 0.0f, 0.1f, 0.35f, 0.6f, 0.4f, 0.15f, 0.6f, 0.5f, 0.5f, 0.15f, 0.55f, 0.6f, 0.4f, 0.2f, 0.65f, 0.45f };
        for (int i = 1; i < 16; ++i)
        {
            if (on[i] && dens < 0.5f)
            {
                const float strength = (i % 4 == 0) ? 0.75f : (i % 2 == 0 ? 0.5f : 0.35f);
                if (U (i, P_DROP) < (0.5f - dens) * 2.0f * (1.0f - strength) * 1.5f) on[i] = false;
            }
            else if (! on[i] && dens > 0.5f)
            {
                if (U (i, P_ADD) < (dens - 0.5f) * 2.0f * addW[i] * 0.55f) on[i] = true;
            }
        }

        if (s.gaps && ! lastBar && U (8, P_REST) < 0.1f * spice)       // Luft: zweite Takthaelfte fast leer
            for (int i = 8; i < 14; ++i) on[i] = false;

        on[0] = true;                                                   // die Eins sitzt immer

        std::vector<int> steps;
        for (int i = 0; i < 16; ++i) if (on[i]) steps.push_back (i);

        bool occ[32] = {};                                              // Belegung im 32tel-Raster
        for (int st : steps) occ[st * 2] = true;

        // ---- 2) Tonhoehen ------------------------------------------------------------
        const int root = bar.root;
        const int nextRoot = barRoot ((barIdx + 1) % nBars, nBars);
        const int count = (int) steps.size();

        std::vector<Hit> hits;
        std::vector<int> pitches ((size_t) count, root);

        for (int k = 1; k < count; ++k)
        {
            const int st = steps[(size_t) k];
            int pitch = root;

            if (k == count - 1 && st >= 12 && U (st, P_APPROACH) < 0.42f * mv)
            {
                pitch = neighbour (nextRoot, U (st, P_APPDIR) < 0.6f);  // Durchgangsnote zum naechsten Takt
            }
            else if (U (st, P_TONE) < 0.22f * mv)
            {
                const float r = U (st, P_TONEVAL);
                if      (r < 0.3f)  pitch = root + 7;
                else if (r < 0.6f)  pitch = root - 5;
                else if (r < 0.8f)  pitch = neighbour (root, true);
                else                pitch = root + (inScale (root + 3) ? 3 : 4);
            }

            if (s.octaves && pitch == root && st % 2 == 1 && U (st, P_OCT) < 0.14f * spice * pr.octaveBias)
                pitch = root + 12;

            pitches[(size_t) k] = pitch;
        }

        const double stepMsNow = stepMs();
        int tails = 0;
        const int maxTails = slide < 0.03f ? 0 : 1 + (slide > 0.6f ? 1 : 0);

        for (int k = 0; k < count; ++k)
        {
            const int st = steps[(size_t) k];
            const int pitch = pitches[(size_t) k];
            const int nextStep = k + 1 < count ? steps[(size_t) k + 1] : 16;
            const int gapSteps = nextStep - st;

            Hit h;
            h.start    = 0.25 * st;
            h.pitch    = pitch;
            h.velocity = st % 4 == 0 ? 112 : (st % 2 == 0 ? 102 : 96);
            h.kind     = kNote;

            // Slide zur Note: gleitet vom vorigen Ton herueber
            if (k > 0 && pitch != pitches[(size_t) k - 1] && U (st, P_SLIDETO) < slide * pr.slideBias * 0.55f * intensity)
                h.kind = kSlide;

            hits.push_back (h);

            // Slide am Ende einer langen Note: zieht kurz vor dem naechsten Schlag weg (typisch Drill)
            if (gapSteps >= 3 && tails < maxTails
                && U (st, P_TAIL) < slide * pr.slideBias * 0.42f * intensity * (k == count - 1 ? 1.5f : 1.0f))
            {
                const int tailSteps = (gapSteps >= 6 || stepMsNow < 95.0) && gapSteps >= 4 ? 2 : 1;
                const float r = U (st, P_TAILIV);
                const int iv = r < 0.45f ? 12 : (r < 0.62f ? 7 : (r < 0.74f ? -12 : (r < 0.86f ? 5 : (neighbour (pitch, false) - pitch))));

                Hit t;
                t.start    = 0.25 * (nextStep - tailSteps);
                t.length   = 0.25 * tailSteps * 0.96;
                t.pitch    = pitch + iv;
                t.velocity = 100;
                t.kind     = kSlide;
                hits.push_back (t);
                for (int c = (nextStep - tailSteps) * 2; c < nextStep * 2; ++c) occ[c] = true;
                ++tails;
            }
        }

        // ---- 3) Stotter: schnelle Anschlaege direkt vor einem Schlag ------------------------
        if (s.stutter)
        {
            for (int k = 1; k < count; ++k)
            {
                const int st = steps[(size_t) k];
                if (st < 4 || occ[st * 2 - 1] || occ[st * 2 - 2] || occ[st * 2 - 3] || occ[st * 2 - 4]) continue;
                if (U (st, P_STUT) >= 0.16f * spice * pr.stutterBias * intensity) continue;

                if (stepMsNow * 0.5 >= 45.0)       // 32tel sind bei diesem Tempo noch als einzelne Schlaege hoerbar
                {
                    const int n = U (st, P_STUTN) < 0.5f ? 2 : 3;
                    for (int j = n; j >= 1; --j)
                    {
                        Hit r;
                        r.start    = 0.25 * st - 0.125 * j;
                        r.length   = 0.11;
                        r.pitch    = pitches[(size_t) k];
                        r.velocity = 100 - 12 * j;
                        r.kind     = kStutter;
                        hits.push_back (r);
                        occ[st * 2 - j] = true;
                    }
                }
                else                               // schnelles Tempo: ein einzelner Vorschlag ein 16tel vorher
                {
                    Hit r;
                    r.start    = 0.25 * (st - 1);
                    r.length   = 0.22;
                    r.pitch    = pitches[(size_t) k];
                    r.velocity = 84;
                    r.kind     = kStutter;
                    hits.push_back (r);
                    occ[st * 2 - 2] = true;
                }
            }
        }

        // ---- 4) Triolen-Bounce im letzten Beat ----------------------------------------------
        if (s.triplets && U (12, P_TRIP) < 0.12f * spice * intensity)
        {
            bool free = true;
            for (int c = 25; c < 32; ++c) free = free && ! occ[c];
            if (free)
            {
                hits.erase (std::remove_if (hits.begin(), hits.end(), [] (const Hit& h) { return h.start > 3.01; }), hits.end());
                for (int j = 1; j < 3; ++j)
                {
                    Hit t;
                    t.start    = 3.0 + j / 3.0;
                    t.length   = 0.3;
                    t.pitch    = j == 2 ? neighbour (nextRoot, true) : root;
                    t.velocity = 98;
                    t.kind     = kStutter;
                    hits.push_back (t);
                }
                if (! occ[24])
                {
                    Hit t;
                    t.start = 3.0; t.length = 0.3; t.pitch = root; t.velocity = 106; t.kind = kNote;
                    hits.push_back (t);
                }
                for (int c = 24; c < 32; ++c) occ[c] = true;
            }
        }

        // ---- 5) Ghost Notes -------------------------------------------------------------------
        for (int i = 3; i < 16; i += 2)
        {
            if (occ[i * 2] || occ[i * 2 - 1] || occ[i * 2 + 1] || (i < 15 && occ[i * 2 + 2]) || occ[i * 2 - 2]) continue;
            if (U (i, P_GHOST) < 0.07f * spice)
            {
                Hit g;
                g.start = 0.25 * i; g.length = 0.2; g.pitch = root; g.velocity = 60; g.kind = kGhost;
                hits.push_back (g);
                occ[i * 2] = true;
            }
        }

        // ---- 6) aufraeumen -------------------------------------------------------------------------
        std::stable_sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.start < b.start; });

        auto priority = [] (int kind) { return kind == kNote ? 3 : (kind == kSlide ? 2 : (kind == kStutter ? 1 : 0)); };
        std::vector<Hit> clean;
        for (Hit h : hits)
        {
            if (h.start < -1.0e-9 || h.start >= 4.0 - 1.0e-9) continue;
            h.pitch = std::clamp (h.pitch, -17, 24);
            if (! clean.empty() && h.start - clean.back().start < 0.02)
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
//  Standard-MIDI-Datei (Format 0, 480 PPQ). Slides ueberlappen die Note davor:
//  mit "Mono + Porta" im Ziel-Kanal gleitet der Ton dann.
// ----------------------------------------------------------------------------
inline std::vector<uint8_t> writeMidiFile (const PatternData& p, double bpm)
{
    struct Ev { uint32_t tick; int on; int note; int vel; };
    std::vector<Ev> evs;
    const double ppq = 480.0;

    for (int i = 0; i < p.numEvents; ++i)
    {
        const NoteEvent& e = p.events[i];
        const auto on  = (uint32_t) std::max (0L, std::lround (e.start * ppq));
        const auto off = std::max (on + 12u, (uint32_t) std::max (0L, std::lround ((e.start + e.length) * ppq)));
        evs.push_back ({ on, 1, std::clamp (e.note, 0, 127), e.velocity });
        evs.push_back ({ off, 0, std::clamp (e.note, 0, 127), 0 });
    }

    // bei gleicher Zeit: erst Note-On (damit Slides wirklich ueberlappen), dann Note-Off
    std::stable_sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b)
    {
        return a.tick != b.tick ? a.tick < b.tick : a.on > b.on;
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

    const std::string name = "Purple 808";
    vlq (0); trk.push_back (0xff); trk.push_back (0x03); trk.push_back ((uint8_t) name.size());
    trk.insert (trk.end(), name.begin(), name.end());

    const auto us = (uint32_t) std::lround (60000000.0 / std::clamp (bpm, 20.0, 999.0));
    vlq (0); trk.push_back (0xff); trk.push_back (0x51); trk.push_back (0x03);
    trk.push_back ((uint8_t) (us >> 16)); trk.push_back ((uint8_t) (us >> 8)); trk.push_back ((uint8_t) us);
    vlq (0); trk.push_back (0xff); trk.push_back (0x58); trk.push_back (0x04);
    trk.push_back (4); trk.push_back (2); trk.push_back (24); trk.push_back (8);

    int sounding[128] = {};
    uint32_t last = 0;
    for (const Ev& e : evs)
    {
        if (e.on && sounding[e.note] > 0)        // gleiche Tonhoehe klingt noch: erst beenden
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

} // namespace p808
