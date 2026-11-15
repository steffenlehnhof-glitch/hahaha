// =============================================================================
//  Purrgression - Chord-Engine
//  Reines C++17 ohne JUCE: Harmonielehre, Generator, Variationen, Voicings,
//  Patterns und MIDI-Datei-Export. Laesst sich dadurch auch ohne Plugin testen.
// =============================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace purr
{
constexpr int kMaxChords     = 8;
constexpr int kMaxChordNotes = 10;
constexpr int kMaxEvents     = 2048;

enum Style   { kTrap, kHipHop, kLoFi, kJazz, kRnB, kNumStyles };
enum ScaleId { kNatMinor, kHarmMinor, kDorian, kPhrygian, kMelMinor, kMajor, kMixolydian, kLydian, kNumScales };
enum Voicing { kClose, kOpen, kDrop2, kShell, kRootless, kNumVoicings };
enum Pattern { kSustain, kStrum, kStabs, kLoFiBounce, kBoomBap, kArpUp, kArpUpDown, kArpTriplet, kNumPatterns };
enum Quality { kQMajor, kQMinor, kQDominant, kQTense };

// ----------------------------------------------------------------------------
//  Tabellen
// ----------------------------------------------------------------------------
struct ScaleDef
{
    const char* name;      // UTF-8
    int  iv[7];            // Halbtonschritte ab Grundton
    bool minorish;         // Moll-artig (kleine Terz)
    int  relMajorOffset;   // Abstand zur parallelen Dur-Tonart (fuer b/#-Schreibweise)
};

inline const ScaleDef& scaleDef (int id)
{
    static const ScaleDef defs[kNumScales] = {
        { "Moll (nat\xc3\xbcrlich)", { 0, 2, 3, 5, 7, 8, 10 }, true,  3 },
        { "Harmonisch Moll",         { 0, 2, 3, 5, 7, 8, 11 }, true,  3 },
        { "Dorisch",                 { 0, 2, 3, 5, 7, 9, 10 }, true, 10 },
        { "Phrygisch",               { 0, 1, 3, 5, 7, 8, 10 }, true,  8 },
        { "Melodisch Moll",          { 0, 2, 3, 5, 7, 9, 11 }, true,  3 },
        { "Dur",                     { 0, 2, 4, 5, 7, 9, 11 }, false, 0 },
        { "Mixolydisch",             { 0, 2, 4, 5, 7, 9, 10 }, false, 5 },
        { "Lydisch",                 { 0, 2, 4, 6, 7, 9, 11 }, false, 7 },
    };
    return defs[std::clamp (id, 0, (int) kNumScales - 1)];
}

inline const char* styleName (int id)
{
    static const char* n[kNumStyles] = { "Trap", "Hip-Hop", "Lo-Fi", "Jazz", "R&B / Neo-Soul" };
    return n[std::clamp (id, 0, (int) kNumStyles - 1)];
}

inline const char* voicingName (int id)
{
    static const char* n[kNumVoicings] = { "Eng", "Offen", "Drop 2", "Shell", "Rootless" };
    return n[std::clamp (id, 0, (int) kNumVoicings - 1)];
}

inline const char* patternName (int id)
{
    static const char* n[kNumPatterns] = { "Gehalten", "Strum", "Stabs 3-3-2", "Lo-Fi Bounce",
                                           "Boom-Bap", "Arp auf", "Arp auf/ab", "Arp Triolen" };
    return n[std::clamp (id, 0, (int) kNumPatterns - 1)];
}

struct ChordType
{
    const char* suffix;
    int n;
    int iv[7];
};

enum TypeId
{
    T_MAJ, T_MIN, T_DIM, T_AUG, T_SUS2, T_SUS4, T_6, T_M6, T_ADD9, T_MADD9,
    T_MAJ7, T_7, T_M7, T_MMAJ7, T_M7B5, T_DIM7, T_MAJ7S5, T_7SUS4, T_7S5,
    T_MAJ9, T_9, T_M9, T_MMAJ9, T_7B9, T_7S9, T_69, T_M69, T_9SUS4, T_MAJ7S11, T_M7ADD11,
    T_MAJ9S11, T_9S11, T_M11, T_13, T_MAJ13,
    kNumTypes
};

inline const ChordType& chordType (int id)
{
    static const ChordType t[kNumTypes] = {
        { "",        3, { 0, 4, 7 } },
        { "m",       3, { 0, 3, 7 } },
        { "dim",     3, { 0, 3, 6 } },
        { "aug",     3, { 0, 4, 8 } },
        { "sus2",    3, { 0, 2, 7 } },
        { "sus4",    3, { 0, 5, 7 } },
        { "6",       4, { 0, 4, 7, 9 } },
        { "m6",      4, { 0, 3, 7, 9 } },
        { "add9",    4, { 0, 4, 7, 14 } },
        { "m(add9)", 4, { 0, 3, 7, 14 } },
        { "maj7",    4, { 0, 4, 7, 11 } },
        { "7",       4, { 0, 4, 7, 10 } },
        { "m7",      4, { 0, 3, 7, 10 } },
        { "m(maj7)", 4, { 0, 3, 7, 11 } },
        { "m7b5",    4, { 0, 3, 6, 10 } },
        { "dim7",    4, { 0, 3, 6, 9 } },
        { "maj7#5",  4, { 0, 4, 8, 11 } },
        { "7sus4",   4, { 0, 5, 7, 10 } },
        { "7#5",     4, { 0, 4, 8, 10 } },
        { "maj9",    5, { 0, 4, 7, 11, 14 } },
        { "9",       5, { 0, 4, 7, 10, 14 } },
        { "m9",      5, { 0, 3, 7, 10, 14 } },
        { "m(maj9)", 5, { 0, 3, 7, 11, 14 } },
        { "7b9",     5, { 0, 4, 7, 10, 13 } },
        { "7#9",     5, { 0, 4, 7, 10, 15 } },
        { "6/9",     5, { 0, 4, 7, 9, 14 } },
        { "m6/9",    5, { 0, 3, 7, 9, 14 } },
        { "9sus4",   5, { 0, 5, 7, 10, 14 } },
        { "maj7#11", 5, { 0, 4, 7, 11, 18 } },
        { "m7(11)",  5, { 0, 3, 7, 10, 17 } },
        { "maj9#11", 6, { 0, 4, 7, 11, 14, 18 } },
        { "9#11",    6, { 0, 4, 7, 10, 14, 18 } },
        { "m11",     6, { 0, 3, 7, 10, 14, 17 } },
        { "13",      6, { 0, 4, 7, 10, 14, 21 } },
        { "maj13",   6, { 0, 4, 7, 11, 14, 21 } },
    };
    return t[std::clamp (id, 0, (int) kNumTypes - 1)];
}

inline int pc (int x) { return ((x % 12) + 12) % 12; }

inline bool typeHas (int type, int interval)
{
    const auto& t = chordType (type);
    for (int i = 0; i < t.n; ++i)
        if (t.iv[i] == interval)
            return true;
    return false;
}

inline bool isMinorThird (int type) { return chordType (type).iv[1] == 3; }
inline bool isDimType    (int type) { return type == T_DIM || type == T_DIM7 || type == T_M7B5; }
inline bool isAugType    (int type) { return type == T_AUG || type == T_MAJ7S5 || type == T_7S5; }
inline bool isDominant   (int type)
{
    const int third = chordType (type).iv[1];
    return typeHas (type, 10) && (third == 4 || third == 5);
}

inline int qualityOf (int type)
{
    if (isDimType (type) || isAugType (type)) return kQTense;
    if (isDominant (type))                    return kQDominant;
    if (isMinorThird (type))                  return kQMinor;
    return kQMajor;
}

// Spezial-Token in Vorlagen (alles ausserhalb 1..7)
constexpr int kN2 = 22;  // bII   (Neapolitaner / phrygisch)
constexpr int kB4 = 24;  // geliehene IV: Dur-IV in Moll, Moll-iv in Dur
constexpr int kD5 = 25;  // Dominante (Dur-V bzw. V7)
constexpr int kB6 = 26;  // bVI   (in Dur geliehen)
constexpr int kB7 = 27;  // bVII  (in Dur geliehen)

// ----------------------------------------------------------------------------
//  Datenstrukturen
// ----------------------------------------------------------------------------
struct Chord
{
    int  root     = 0;       // Tonklasse 0..11
    int  type     = T_MIN;
    int  degree   = 1;       // funktionale Stufe 1..7, 0 = chromatisch
    int  level    = 0;       // Farbstufe 0..3 (Dreiklang .. 11/13)
    bool dia      = true;    // rein leitereigen
    bool locked   = false;
    int  secOf    = -1;      // Zwischendominante: Ziel (Halbtoene ueber Tonika), sonst -1
    bool secMinor = false;
};

inline bool sameChord (const Chord& a, const Chord& b)
{
    return a.root == b.root && a.type == b.type;
}

struct Settings
{
    int   key       = 5;          // F
    int   scale     = kNatMinor;
    int   style     = kTrap;
    int   numChords = 4;
    int   duration  = 1;          // 0 = halber Takt, 1 = 1 Takt, 2 = 2 Takte
    float colour    = 0.35f;      // "Farbe": wie reich die Akkorde sind
    float spice     = 0.30f;      // "Wuerze": Substitutionen
    float humanize  = 0.20f;
    float swing     = 0.0f;
    int   voicing   = kClose;
    int   pattern   = kSustain;
    int   octave    = 0;          // -1..+1
    bool  bass      = true;
    bool  voiceLead = true;
    int   sound     = 1;          // 0 = aus, 1 = Keys, 2 = Pad, 3 = Pluck
    bool  hostSync  = true;
};

struct NoteEvent
{
    double start    = 0.0;   // in Vierteln ab Loop-Anfang
    double length   = 0.0;   // in Vierteln
    int    note     = 60;
    int    velocity = 90;
};

struct ProgData
{
    int    numChords = 0;
    int    chordSize[kMaxChords] {};
    int    chordNotes[kMaxChords][kMaxChordNotes] {};
    double beatsPerChord = 4.0;
    double totalBeats    = 0.0;
    int    numEvents     = 0;
    NoteEvent events[kMaxEvents];
};

// ----------------------------------------------------------------------------
//  Engine
// ----------------------------------------------------------------------------
class Engine
{
public:
    Settings           s;
    std::vector<Chord> chords;
    uint32_t           humanSeed = 1;

    explicit Engine (uint32_t seed = 0x51ab3c7u) : rng (seed) { humanSeed = (uint32_t) rng(); }

    void reseed (uint32_t seed) { rng.seed (seed); }

    // ---- Namen --------------------------------------------------------------
    bool useFlats() const
    {
        const int rel = pc (s.key + scaleDef (s.scale).relMajorOffset);
        return rel == 5 || rel == 10 || rel == 3 || rel == 8 || rel == 1 || rel == 6;
    }

    std::string noteName (int pitchClass) const
    {
        static const char* sharp[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const char* flat[12]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        return (useFlats() ? flat : sharp)[pc (pitchClass)];
    }

    std::string chordName (const Chord& c) const
    {
        return noteName (c.root) + chordType (c.type).suffix;
    }

    std::string numeral (int interval, bool lower) const
    {
        static const char* up[12] = { "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII" };
        static const char* lo[12] = { "i", "bii", "ii", "biii", "iii", "iv", "#iv", "v", "bvi", "vi", "bvii", "vii" };
        return (lower ? lo : up)[pc (interval)];
    }

    // Stufenbezeichnung (UTF-8, enthaelt ggf. ° und ø)
    std::string roman (const Chord& c) const
    {
        const std::string suffix = chordType (c.type).suffix;

        if (c.secOf >= 0)
            return "V" + suffix + "/" + numeral (c.secOf, c.secMinor);

        const bool lower = isMinorThird (c.type);
        std::string r = numeral (c.root - s.key, lower);

        if      (c.type == T_DIM)  r += "\xc2\xb0";
        else if (c.type == T_DIM7) r += "\xc2\xb0" "7";
        else if (c.type == T_M7B5) r += "\xc3\xb8" "7";
        else if (c.type == T_AUG)  r += "+";
        else if (lower && ! suffix.empty() && suffix[0] == 'm') r += suffix.substr (1);
        else r += suffix;

        return r;
    }

    std::string chordNoteNames (const Chord& c) const
    {
        const auto& t = chordType (c.type);
        std::string out;
        for (int i = 0; i < t.n; ++i)
        {
            if (i > 0) out += " ";
            out += noteName (c.root + t.iv[i]);
        }
        return out;
    }

    // ---- Generieren -----------------------------------------------------------
    void generate()
    {
        const int n = std::clamp (s.numChords, 2, kMaxChords);
        const std::vector<int> tok = makeTokens (n);

        std::vector<Chord> out ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            if (i < (int) chords.size() && chords[(size_t) i].locked)
                out[(size_t) i] = chords[(size_t) i];
            else
                out[(size_t) i] = realise (tok[(size_t) i], pickLevel());
        }
        chords = out;

        for (int i = 0; i < n; ++i)
            applySpice (i, false);

        humanSeed = (uint32_t) rng();
    }

    // Kleine Mutation der aktuellen Folge
    void vary()
    {
        const int n = (int) chords.size();
        if (n == 0) { generate(); return; }

        std::vector<int> freeIdx;
        for (int i = 0; i < n; ++i)
            if (! chords[(size_t) i].locked)
                freeIdx.push_back (i);

        humanSeed = (uint32_t) rng();
        if (freeIdx.empty())
            return;

        const int ops = 1 + (chance (0.5f) ? 1 : 0);
        int done = 0;

        for (int attempt = 0; attempt < 40 && done < ops; ++attempt)
        {
            const int i = freeIdx[(size_t) rndInt ((int) freeIdx.size())];
            const Chord before = chords[(size_t) i];

            switch (rndInt (5))
            {
                case 0: recolour (i, chance (0.6f) ? 1 : -1); break;
                case 1: if (i > 0) relativeSub (i); break;
                case 2: applySpice (i, true); break;
                case 3: if (i == n - 1) turnaround (i); else if (i > 0) rerollChord (i); break;
                default: if (i > 0 || n <= 2) rerollChord (i); break;
            }

            // keine Dopplung mit dem Nachbarn erzeugen
            const bool dupPrev = i > 0     && sameChord (chords[(size_t) i], chords[(size_t) i - 1]);
            const bool dupNext = i < n - 1 && sameChord (chords[(size_t) i], chords[(size_t) i + 1]);
            if (dupPrev || dupNext)
                chords[(size_t) i] = before;
            else if (! sameChord (before, chords[(size_t) i]))
                ++done;
        }
    }

    void rerollChord (int i)
    {
        const int n = (int) chords.size();
        if (i < 0 || i >= n || chords[(size_t) i].locked)
            return;

        const bool minor = scaleDef (s.scale).minorish;
        const Chord old = chords[(size_t) i];

        for (int attempt = 0; attempt < 16; ++attempt)
        {
            int deg;
            if (i == 0)
            {
                static const int startMinor[6] = { 1, 1, 1, 6, 4, 1 };
                static const int startMajor[6] = { 1, 1, 1, 4, 6, 2 };
                deg = (minor ? startMinor : startMajor)[rndInt (6)];
            }
            else
            {
                deg = markovNext (functionalDegree (chords[(size_t) i - 1]), minor, old.root >= 0 ? old.degree : 0);
            }

            chords[(size_t) i] = realise (deg, pickLevel());
            applySpice (i, false);

            const bool dupPrev = i > 0 && sameChord (chords[(size_t) i], chords[(size_t) i - 1]);
            if (! sameChord (old, chords[(size_t) i]) && (! dupPrev || attempt >= 10))
                break;
        }
    }

    void resize (int n)
    {
        n = std::clamp (n, 2, kMaxChords);
        s.numChords = n;
        if ((int) chords.size() > n)
            chords.resize ((size_t) n);

        while ((int) chords.size() < n)
        {
            Chord placeholder;
            placeholder.root = -1;   // garantiert "anders" als jedes Ergebnis
            chords.push_back (placeholder);
            rerollChord ((int) chords.size() - 1);
        }
    }

    void transpose (int semitones)
    {
        for (auto& c : chords)
            c.root = pc (c.root + semitones);
    }

    // ---- Voicings + Noten-Events ------------------------------------------------
    double beatsPerChord() const
    {
        return s.duration <= 0 ? 2.0 : (s.duration == 1 ? 4.0 : 8.0);
    }

    void buildProgData (ProgData& out) const
    {
        out.numChords     = std::min ((int) chords.size(), kMaxChords);
        out.beatsPerChord = beatsPerChord();
        out.totalBeats    = out.beatsPerChord * out.numChords;
        out.numEvents     = 0;

        std::mt19937 hr (humanSeed);
        std::uniform_real_distribution<float> uni (-1.0f, 1.0f);

        const double D     = out.beatsPerChord;
        const double total = out.totalBeats;
        const float  h     = std::clamp (s.humanize, 0.0f, 1.0f);
        const float  sw    = std::clamp (s.swing, 0.0f, 1.0f);

        auto add = [&] (double start, double len, int note, float vel)
        {
            if (out.numEvents >= kMaxEvents || note < 0 || note > 127)
                return;

            // Swing: Offbeat-Achtel nach hinten schieben
            const double frac = start - std::floor (start);
            if (std::abs (frac - 0.5) < 0.01)
                start += sw * 0.16;

            start += h * 0.03 * uni (hr);
            vel   += h * 14.0f * uni (hr);

            start = std::clamp (start, 0.0, total - 0.06);
            len   = std::max (0.05, std::min (len, total - start - 0.01));

            NoteEvent& e = out.events[out.numEvents++];
            e.start    = start;
            e.length   = len;
            e.note     = note;
            e.velocity = std::clamp ((int) std::lround (vel), 1, 127);
        };

        std::vector<int> prevUpper;

        for (int ci = 0; ci < out.numChords; ++ci)
        {
            const Chord& c = chords[(size_t) ci];
            std::vector<int> U = voiceUpper (c, prevUpper);
            prevUpper = U;

            int bass = -1;
            if (s.bass)
            {
                bass = 36 + 12 * s.octave + c.root;
                while (! U.empty() && bass >= U.front())
                    bass -= 12;
            }

            // fuer Anzeige + Tasten-Trigger
            int k = 0;
            if (bass >= 0) out.chordNotes[ci][k++] = bass;
            for (int note : U)
                if (k < kMaxChordNotes)
                    out.chordNotes[ci][k++] = note;
            out.chordSize[ci] = k;

            const double B  = ci * D;
            const int    nu = (int) U.size();

            auto velOf = [&] (int idx) -> float
            {
                if (idx == nu - 1) return 96.0f;   // Oberstimme leicht betont
                if (idx == 0)      return 88.0f;
                return 82.0f;
            };

            auto hitAll = [&] (double at, double len, float scale)
            {
                if (bass >= 0) add (at, len, bass, 92.0f * scale);
                for (int i = 0; i < nu; ++i)
                    add (at, len, U[(size_t) i], velOf (i) * scale);
            };

            switch (s.pattern)
            {
                default:
                case kSustain:
                    hitAll (B, D - 0.04, 1.0f);
                    break;

                case kStrum:
                    if (bass >= 0) add (B, D - 0.04, bass, 92.0f);
                    for (int i = 0; i < nu; ++i)
                    {
                        const double off = 0.045 * (i + 1);
                        add (B + off, D - 0.04 - off, U[(size_t) i], velOf (i) - 2.0f * (float) (nu - 1 - i));
                    }
                    break;

                case kStabs:
                    for (double bar = 0.0; bar < D - 0.01; bar += 4.0)
                    {
                        static const double at[3]  = { 0.0, 1.5, 3.0 };
                        static const double len[3] = { 1.2, 1.2, 0.8 };
                        static const float  sc[3]  = { 1.0f, 0.84f, 0.92f };
                        for (int j = 0; j < 3; ++j)
                            if (bar + at[j] < D - 0.01)
                                hitAll (B + bar + at[j], std::min (len[j], D - bar - at[j] - 0.04), sc[j]);
                    }
                    break;

                case kLoFiBounce:
                    for (double bar = 0.0; bar < D - 0.01; bar += 4.0)
                    {
                        if (D < 3.0)
                        {
                            hitAll (B, 1.3, 1.0f);
                            hitAll (B + 1.5, 0.45, 0.7f);
                        }
                        else
                        {
                            hitAll (B + bar, 1.6, 1.0f);
                            hitAll (B + bar + 2.5, 1.2, 0.72f);
                        }
                    }
                    break;

                case kBoomBap:
                {
                    const int low = bass >= 0 ? bass : (nu > 0 ? U[0] : -1);
                    for (double beat = 0.0; beat < D - 0.01; beat += 2.0)
                    {
                        add (B + beat, 0.9, low, 96.0f);
                        if (beat + 1.0 < D - 0.01)
                            for (int i = (bass >= 0 ? 0 : 1); i < nu; ++i)
                                add (B + beat + 1.0, 0.7, U[(size_t) i], velOf (i) * 0.9f);
                    }
                    break;
                }

                case kArpUp:
                case kArpUpDown:
                case kArpTriplet:
                {
                    if (bass >= 0) add (B, D - 0.04, bass, 90.0f);
                    if (nu == 0) break;

                    std::vector<int> seq = U;
                    if (s.pattern == kArpUpDown)
                        for (int i = nu - 2; i >= 1; --i)
                            seq.push_back (U[(size_t) i]);

                    const double step = s.pattern == kArpTriplet ? 1.0 / 3.0 : 0.5;
                    const int steps = (int) std::lround (D / step);
                    for (int j = 0; j < steps; ++j)
                    {
                        const bool accent = (j % (s.pattern == kArpTriplet ? 3 : 2)) == 0;
                        add (B + j * step, step * 0.95, seq[(size_t) j % seq.size()], accent ? 94.0f : 80.0f);
                    }
                    break;
                }
            }
        }

        std::stable_sort (out.events, out.events + out.numEvents,
                          [] (const NoteEvent& a, const NoteEvent& b) { return a.start < b.start; });

        // gleiche Tonhoehe darf sich nicht ueberlappen
        int last[128];
        std::fill (last, last + 128, -1);
        for (int i = 0; i < out.numEvents; ++i)
        {
            NoteEvent& e = out.events[i];
            const int p = last[e.note];
            if (p >= 0)
            {
                NoteEvent& prev = out.events[p];
                if (prev.start + prev.length > e.start - 0.01)
                    prev.length = std::max (0.02, e.start - prev.start - 0.01);
            }
            last[e.note] = i;
        }
    }

private:
    mutable std::mt19937 rng;

    // ---- Zufall ---------------------------------------------------------------
    float rnd() const               { return std::uniform_real_distribution<float> (0.0f, 1.0f) (rng); }
    int   rndInt (int n) const      { return n <= 1 ? 0 : (int) (rng() % (uint32_t) n); }
    bool  chance (float p) const    { return rnd() < p; }

    bool inScale (int pitchClass) const
    {
        const auto& sc = scaleDef (s.scale);
        for (int i = 0; i < 7; ++i)
            if (pc (s.key + sc.iv[i]) == pc (pitchClass))
                return true;
        return false;
    }

    int pickLevel() const
    {
        static const float offs[kNumStyles] = { -0.4f, -0.1f, 0.3f, 0.45f, 0.3f };
        const float x = std::clamp (s.colour, 0.0f, 1.0f) * 3.0f + offs[std::clamp (s.style, 0, (int) kNumStyles - 1)];
        const float fl = std::floor (x);
        const int lvl = (int) fl + (chance (x - fl) ? 1 : 0);
        return std::clamp (lvl, 0, 3);
    }

    static int lookupType (const std::vector<int>& iv)
    {
        for (int t = 0; t < kNumTypes; ++t)
        {
            const auto& ct = chordType (t);
            if (ct.n == (int) iv.size() && std::equal (iv.begin(), iv.end(), ct.iv))
                return t;
        }
        return -1;
    }

    // Leitereigener Akkord durch Terzschichtung auf der Stufe
    Chord diatonic (int deg, int level) const
    {
        deg = std::clamp (deg, 1, 7);
        level = std::clamp (level, 0, 3);

        const int* sc = scaleDef (s.scale).iv;
        const int d = deg - 1;
        auto tone = [&] (int idx) { return sc[idx % 7] + 12 * (idx / 7); };
        auto iv   = [&] (int steps) { return tone (d + steps) - sc[d]; };

        std::vector<int> ivs { 0, iv (2), iv (4) };
        const bool min3 = ivs[1] == 3, maj3 = ivs[1] == 4, p5 = ivs[2] == 7;

        if (level >= 1)
            ivs.push_back (iv (6));

        const bool dom = level >= 1 && maj3 && p5 && ivs[3] == 10;
        bool has9 = false;

        if (level >= 2)
        {
            const int ninth = iv (8);
            if (ninth == 14 || (ninth == 13 && dom))
            {
                ivs.push_back (ninth);
                has9 = (ninth == 14);
            }
        }

        if (level >= 3)
        {
            const int eleventh = iv (10), thirteenth = iv (12);
            if      (min3 && p5 && ivs[3] == 10 && eleventh == 17)           ivs.push_back (17);
            else if (maj3 && p5 && ivs[3] == 11 && eleventh == 18)           ivs.push_back (18);
            else if (maj3 && p5 && has9 && thirteenth == 21)                 ivs.push_back (21);
        }

        int type = lookupType (ivs);
        while (type < 0 && ivs.size() > 3)
        {
            ivs.pop_back();
            type = lookupType (ivs);
        }
        if (type < 0)
            type = min3 ? T_MIN : T_MAJ;

        // Stil-Faerbungen
        const bool nat9  = iv (8) == 14;
        const bool maj6  = iv (5) == 9;
        const bool trap  = s.style == kTrap, rnb = s.style == kRnB;
        const bool jazzy = s.style == kJazz || s.style == kLoFi || rnb;

        if      (type == T_MAJ  && nat9 && chance ((trap || rnb) ? 0.18f : 0.08f))  type = T_ADD9;
        else if (type == T_MIN  && nat9 && chance (trap ? 0.2f : 0.1f))            type = T_MADD9;
        else if (type == T_MAJ7 && maj6 && jazzy && chance (0.2f))                 type = T_6;
        else if (type == T_MAJ9 && maj6 && chance (0.25f))                         type = T_69;
        else if (type == T_M7   && maj6 && s.style != kTrap && chance (0.15f))     type = T_M6;
        else if (type == T_M9   && maj6 && chance (0.12f))                         type = T_M69;

        Chord c;
        c.root   = pc (s.key + sc[d]);
        c.type   = type;
        c.degree = deg;
        c.level  = level;
        c.dia    = true;
        return c;
    }

    Chord realise (int tok, int level) const
    {
        if (tok >= 1 && tok <= 7)
        {
            Chord c = diatonic (tok, level);

            // Verminderte Stufen klingen ausserhalb von Jazz schnell schraeg: meist durch den
            // verwandten Akkord ersetzen, der dieselben Toene enthaelt (vii° -> V, ii° -> bVII ...)
            if (isDimType (c.type) && s.style != kJazz && chance (0.75f))
            {
                for (int shift : { 5, 3 })
                {
                    const Chord alt = diatonic ((tok - 1 + shift) % 7 + 1, level);
                    if (! isDimType (alt.type))
                        return alt;
                }
            }
            return c;
        }

        const bool minor = scaleDef (s.scale).minorish;
        level = std::clamp (level, 0, 3);
        auto pick = [level] (int a, int b, int c2, int d) { const int arr[4] = { a, b, c2, d }; return arr[level]; };

        Chord c;
        c.level  = level;
        c.dia    = false;
        c.degree = 0;

        switch (tok)
        {
            case kN2: c.root = pc (s.key + 1);  c.type = pick (T_MAJ, T_MAJ7, T_MAJ7, T_MAJ7S11); break;
            case kD5: c.root = pc (s.key + 7);  c.degree = 5;
                      c.type = minor ? pick (T_MAJ, T_7, T_7B9, T_7S9) : pick (T_MAJ, T_7, T_9, T_13); break;
            case kB4: c.root = pc (s.key + 5);  c.degree = 4;
                      c.type = minor ? pick (T_MAJ, T_7, T_9, T_13) : pick (T_MIN, T_M6, T_M7, T_M9); break;
            case kB6: c.root = pc (s.key + 8);  c.type = pick (T_MAJ, T_MAJ7, T_MAJ9, T_MAJ7S11); break;
            case kB7: c.root = pc (s.key + 10); c.type = pick (T_MAJ, T_MAJ7, T_MAJ9, T_13); break;
            default:  return diatonic (1, level);
        }
        return c;
    }

    int functionalDegree (const Chord& c) const
    {
        if (c.degree >= 1 && c.degree <= 7)
            return c.degree;

        const int* sc = scaleDef (s.scale).iv;
        for (int i = 0; i < 7; ++i)
            if (pc (s.key + sc[i]) == c.root)
                return i + 1;
        return 1;
    }

    int markovNext (int fromDeg, bool minor, int avoidDeg = 0) const
    {
        static const int wMinor[7][7] = {
            { 0, 1, 2, 3, 2, 3, 3 },
            { 1, 0, 0, 0, 5, 0, 1 },
            { 1, 0, 0, 3, 0, 3, 2 },
            { 3, 1, 0, 0, 3, 1, 3 },
            { 5, 0, 0, 1, 0, 3, 0 },
            { 2, 1, 2, 2, 3, 0, 3 },
            { 3, 0, 4, 0, 1, 2, 0 },
        };
        static const int wMajor[7][7] = {
            { 0, 2, 1, 3, 3, 3, 0 },
            { 0, 0, 1, 1, 5, 0, 0 },
            { 0, 1, 0, 3, 0, 4, 0 },
            { 3, 2, 1, 0, 3, 1, 0 },
            { 4, 0, 1, 2, 0, 3, 0 },
            { 0, 3, 1, 4, 2, 0, 0 },
            { 3, 0, 3, 0, 0, 1, 0 },
        };

        const int from = std::clamp (fromDeg, 1, 7) - 1;
        const bool fifthsy = s.style == kJazz || s.style == kLoFi;

        int w[7], sum = 0;
        for (int to = 0; to < 7; ++to)
        {
            w[to] = 3 * (minor ? wMinor : wMajor)[from][to];
            if (fifthsy && to == (from + 3) % 7)
                w[to] *= 2;
            if (to + 1 == avoidDeg)
                w[to] /= 3;
            sum += w[to];
        }

        int r = rndInt (std::max (1, sum));
        for (int to = 0; to < 7; ++to)
        {
            if (r < w[to]) return to + 1;
            r -= w[to];
        }
        return 1;
    }

    struct Tpl { int style; int minor; int d[4]; };

    std::vector<int> pickTemplate (bool minor) const
    {
        static const Tpl tpl[] = {
            // Trap - Moll
            { kTrap, 1, { 1, 6, 3, 7 } },   { kTrap, 1, { 1, 6, 7, 1 } },   { kTrap, 1, { 1, 4, 6, 5 } },
            { kTrap, 1, { 1, 7, 6, kD5 } }, { kTrap, 1, { 1, 1, 6, 7 } },   { kTrap, 1, { 1, 3, 7, 4 } },
            { kTrap, 1, { 1, 6, 4, 5 } },   { kTrap, 1, { 1, kN2, 1, 7 } }, { kTrap, 1, { 6, 7, 1, 1 } },
            { kTrap, 1, { 1, 5, 6, 4 } },   { kTrap, 1, { 1, 4, 1, kD5 } }, { kTrap, 1, { 1, kN2, 7, 1 } },
            { kTrap, 1, { 4, 6, 1, 7 } },   { kTrap, 1, { 1, 7, 6, 7 } },   { kTrap, 1, { 6, 5, 1, 1 } },
            { kTrap, 1, { 1, 6, kN2, 1 } },
            // Trap - Dur
            { kTrap, 0, { 1, 5, 6, 4 } },   { kTrap, 0, { 6, 4, 1, 5 } },   { kTrap, 0, { 1, 3, 6, 4 } },
            { kTrap, 0, { 4, 1, 5, 6 } },   { kTrap, 0, { 1, kB7, 4, 1 } }, { kTrap, 0, { 1, 4, kB4, 1 } },
            // Hip-Hop - Moll
            { kHipHop, 1, { 1, 4, 7, 3 } },   { kHipHop, 1, { 2, kD5, 1, 1 } }, { kHipHop, 1, { 1, 7, 6, 7 } },
            { kHipHop, 1, { 6, kD5, 1, 1 } }, { kHipHop, 1, { 1, 4, 5, 1 } },   { kHipHop, 1, { 1, 3, 4, 6 } },
            { kHipHop, 1, { 1, 6, 2, kD5 } }, { kHipHop, 1, { 1, kB4, 1, kB4 } }, { kHipHop, 1, { 4, 7, 1, 1 } },
            { kHipHop, 1, { 1, 7, kB4, 1 } },
            // Hip-Hop - Dur
            { kHipHop, 0, { 1, 6, 2, 5 } },   { kHipHop, 0, { 4, 3, 2, 1 } },   { kHipHop, 0, { 1, 3, 4, 5 } },
            { kHipHop, 0, { 2, 5, 1, 6 } },   { kHipHop, 0, { 1, kB7, 4, 1 } }, { kHipHop, 0, { 6, 2, 5, 1 } },
            // Lo-Fi - Moll
            { kLoFi, 1, { 1, 4, 7, 3 } },   { kLoFi, 1, { 6, 7, 1, 1 } },   { kLoFi, 1, { 4, 7, 3, 6 } },
            { kLoFi, 1, { 2, kD5, 1, 6 } }, { kLoFi, 1, { 1, 3, 6, 7 } },   { kLoFi, 1, { 1, kB4, 7, 3 } },
            { kLoFi, 1, { 6, 5, 1, 3 } },   { kLoFi, 1, { 4, 5, 1, 1 } },
            // Lo-Fi - Dur
            { kLoFi, 0, { 2, 5, 1, 6 } },   { kLoFi, 0, { 1, 6, 2, 5 } },   { kLoFi, 0, { 4, 3, 2, 1 } },
            { kLoFi, 0, { 4, 5, 3, 6 } },   { kLoFi, 0, { 2, 5, 3, 6 } },   { kLoFi, 0, { 1, 3, 4, kB4 } },
            { kLoFi, 0, { 4, kB4, 3, 6 } }, { kLoFi, 0, { 1, 4, 3, 6 } },
            // Jazz - Moll
            { kJazz, 1, { 2, kD5, 1, 1 } }, { kJazz, 1, { 1, 6, 2, kD5 } }, { kJazz, 1, { 4, 7, 3, 6 } },
            { kJazz, 1, { 1, 4, 7, 3 } },   { kJazz, 1, { 2, kD5, 1, 6 } }, { kJazz, 1, { 6, 2, kD5, 1 } },
            { kJazz, 1, { 1, kN2, 1, kD5 } }, { kJazz, 1, { 1, 4, 2, kD5 } },
            // Jazz - Dur
            { kJazz, 0, { 2, 5, 1, 6 } },   { kJazz, 0, { 1, 6, 2, 5 } },   { kJazz, 0, { 3, 6, 2, 5 } },
            { kJazz, 0, { 2, 5, 1, 4 } },   { kJazz, 0, { 4, kB4, 3, 6 } }, { kJazz, 0, { 1, 4, 3, 6 } },
            { kJazz, 0, { 2, 5, 3, 6 } },   { kJazz, 0, { 1, kB7, 6, 5 } },
            // R&B - Moll
            { kRnB, 1, { 1, 4, 5, 1 } },    { kRnB, 1, { 6, 7, 1, 1 } },    { kRnB, 1, { 4, 5, 1, 1 } },
            { kRnB, 1, { 1, 7, 6, 5 } },    { kRnB, 1, { 6, 5, 4, 5 } },    { kRnB, 1, { 4, 5, 6, 7 } },
            { kRnB, 1, { 1, 3, kB4, 6 } },  { kRnB, 1, { 6, 7, 5, 1 } },    { kRnB, 1, { 4, 7, 3, 6 } },
            // R&B - Dur
            { kRnB, 0, { 4, 3, 2, 1 } },    { kRnB, 0, { 4, 5, 3, 6 } },    { kRnB, 0, { 2, 3, 4, 5 } },
            { kRnB, 0, { 1, 3, 4, kB4 } },  { kRnB, 0, { 6, 2, 5, 1 } },    { kRnB, 0, { 4, 5, 6, 1 } },
            { kRnB, 0, { 2, 5, 1, 4 } },    { kRnB, 0, { 1, kB6, kB7, 1 } },
        };

        std::vector<const Tpl*> fit;
        for (const auto& t : tpl)
            if (t.style == s.style && (t.minor != 0) == minor)
                fit.push_back (&t);

        if (fit.empty())
            return { 1, 4, 5, 1 };

        const Tpl* t = fit[(size_t) rndInt ((int) fit.size())];
        return { t->d[0], t->d[1], t->d[2], t->d[3] };
    }

    std::vector<int> varyTemplate (std::vector<int> a, bool minor) const
    {
        static const int endMinor[5] = { kD5, 7, 5, kN2, 4 };
        static const int endMajor[5] = { 5, 4, kB7, 2, 5 };

        for (int attempt = 0; attempt < 8; ++attempt)
        {
            const int e = (minor ? endMinor : endMajor)[rndInt (5)];
            if (e != a[3]) { a[3] = e; break; }
        }

        if (chance (0.4f) && a[1] >= 1 && a[1] <= 7)
            a[2] = markovNext (a[1], minor);

        return a;
    }

    std::vector<int> makeTokens (int n) const
    {
        const bool minor = scaleDef (s.scale).minorish;
        std::vector<int> t;

        if (n == 2)
        {
            static const int pairsMinor[7][2] = { { 1, 6 }, { 1, 7 }, { 1, 4 }, { 6, 7 }, { 1, kB4 }, { 1, kN2 }, { 2, kD5 } };
            static const int pairsMajor[6][2] = { { 1, 4 }, { 2, 5 }, { 1, 6 }, { 4, 5 }, { 1, kB7 }, { 4, 1 } };
            const int* p = minor ? pairsMinor[rndInt (7)] : pairsMajor[rndInt (6)];
            return { p[0], p[1] };
        }

        if (chance (0.35f))
        {
            // freie Wanderung nach funktionalen Uebergaengen
            static const int startMinor[6] = { 1, 1, 1, 1, 6, 4 };
            static const int startMajor[6] = { 1, 1, 1, 4, 6, 2 };
            int deg = (minor ? startMinor : startMajor)[rndInt (6)];
            t.push_back (deg);
            while ((int) t.size() < n)
            {
                const int twoBack = t.size() >= 2 ? t[t.size() - 2] : 0;
                deg = markovNext (deg, minor, twoBack);
                t.push_back (deg);
            }
            return t;
        }

        const bool circle = (s.style == kJazz || s.style == kLoFi) && n == 8 && chance (0.25f);
        if (circle)
            return minor ? std::vector<int> { 1, 4, 7, 3, 6, 2, kD5, 1 }
                         : std::vector<int> { 1, 4, 7, 3, 6, 2, 5, 1 };

        t = pickTemplate (minor);
        if (n > 4)
        {
            const std::vector<int> b = chance (0.6f) ? varyTemplate (t, minor) : pickTemplate (minor);
            t.insert (t.end(), b.begin(), b.end());
        }
        t.resize ((size_t) n);
        return t;
    }

    // Substitutionen. force = aus "Variation" heraus (ignoriert den Wuerze-Regler)
    bool applySpice (int i, bool force)
    {
        const int n = (int) chords.size();
        if (i < 0 || i >= n) return false;

        Chord& c = chords[(size_t) i];
        if (c.locked) return false;

        const Chord next  = chords[(size_t) ((i + 1) % n)];
        const bool  minor = scaleDef (s.scale).minorish;
        const float sp    = force ? 1.0f : std::clamp (s.spice, 0.0f, 1.0f);
        const bool  jazzy = s.style == kJazz || s.style == kLoFi || s.style == kRnB;
        const int   lvl   = c.level;

        // (a) Moll-v vor der Tonika wird zur Dur-Dominante
        if (minor && c.dia && c.degree == 5 && isMinorThird (c.type) && next.root == pc (s.key)
            && chance (force ? 0.6f : 0.2f + 0.6f * sp + (s.style == kJazz ? 0.2f : 0.0f)))
        {
            c = realise (kD5, std::max (lvl, jazzy ? 1 : 0));
            return true;
        }

        if (i > 0 || force)
        {
            // (b) Tritonus-Substitution
            if (isDominant (c.type) && next.root == pc (c.root + 5) && chance ((jazzy ? 0.3f : 0.08f) * sp))
            {
                c.root   = pc (c.root + 6);
                c.type   = lvl >= 3 ? T_9S11 : (lvl == 2 ? T_9 : T_7);
                c.level  = std::max (1, lvl);
                c.dia    = false;
                c.degree = 0;
                c.secOf  = -1;
                return true;
            }

            // (c) Zwischendominante zum naechsten Akkord
            if (next.root != c.root && next.secOf < 0 && ! isDimType (next.type)
                && chance (0.2f * sp * (jazzy ? 1.2f : 0.7f)))
            {
                const bool targetMinor = isMinorThird (next.type);
                Chord d;
                d.root     = pc (next.root + 7);
                d.type     = lvl <= 1 ? T_7 : (lvl == 2 ? (targetMinor ? T_7B9 : T_9) : (targetMinor ? T_7S9 : T_13));
                d.level    = std::max (1, lvl);
                d.dia      = false;
                d.degree   = 0;
                d.secOf    = pc (next.root - s.key);
                d.secMinor = targetMinor;
                if (d.secOf == 0) { d.secOf = -1; d.degree = 5; }   // das ist einfach die V

                if (! sameChord (d, c))
                {
                    c = d;
                    return true;
                }
            }

            // (d) geliehene Akkorde (Modal Interchange)
            if (c.dia && chance (0.18f * sp))
            {
                Chord b = c;
                if (c.degree == 4)                       b = realise (kB4, lvl);
                else if (minor && c.degree == 2)         b = realise (kN2, lvl);
                else if (! minor && c.degree == 7)       b = realise (kB7, lvl);
                else if (! minor && c.degree == 6)       b = realise (kB6, lvl);

                if (! sameChord (b, c) && isMinorThird (b.type) != isMinorThird (c.type))
                {
                    c = b;
                    return true;
                }
                if (! sameChord (b, c) && b.root != c.root)
                {
                    c = b;
                    return true;
                }
            }

            // (e) verminderter Durchgangsakkord
            if ((s.style == kJazz || s.style == kLoFi) && next.root != c.root && chance (0.07f * sp))
            {
                c.root   = pc (next.root + 11);
                c.type   = lvl >= 1 ? T_DIM7 : T_DIM;
                c.dia    = false;
                c.degree = 0;
                c.secOf  = -1;
                return true;
            }
        }

        // (f) Sus-Akkorde
        if ((c.type == T_MAJ || c.type == T_MIN)
            && chance (0.2f * sp * ((s.style == kTrap || s.style == kRnB) ? 1.2f : 0.6f)))
        {
            if (inScale (c.root + 2))      { c.type = T_SUS2; return true; }
            if (inScale (c.root + 5))      { c.type = T_SUS4; return true; }
        }

        if ((c.type == T_7 || c.type == T_9) && jazzy && inScale (c.root + 5) && chance (0.2f * sp))
        {
            c.type = (c.type == T_7) ? T_7SUS4 : T_9SUS4;
            return true;
        }

        return false;
    }

    void recolour (int i, int delta)
    {
        Chord& c = chords[(size_t) i];
        const int nl = std::clamp (c.level + delta, 0, 3);
        if (nl == c.level) return;

        if (c.dia && c.degree >= 1)
        {
            c = diatonic (c.degree, nl);
            return;
        }

        static const int ladMaj[4] = { T_MAJ, T_MAJ7, T_MAJ9, T_MAJ13 };
        static const int ladDom[4] = { T_7, T_7, T_9, T_13 };
        static const int ladMin[4] = { T_MIN, T_M7, T_M9, T_M11 };

        if (isDimType (c.type) || isAugType (c.type)) return;
        if      (isDominant (c.type))   c.type = ladDom[nl];
        else if (isMinorThird (c.type)) c.type = ladMin[nl];
        else                            c.type = ladMaj[nl];
        c.level = nl;
    }

    // Parallel-/Gegenklang: Stufe +2 oder -2 (teilt zwei Toene mit dem Original)
    void relativeSub (int i)
    {
        Chord& c = chords[(size_t) i];
        if (c.degree < 1) return;
        const int nd = ((c.degree - 1) + (chance (0.5f) ? 2 : 5)) % 7 + 1;
        c = diatonic (nd, c.level);
    }

    void turnaround (int i)
    {
        static const int endMinor[4] = { kD5, 7, kN2, 5 };
        static const int endMajor[4] = { 5, 4, kB7, 2 };
        const bool minor = scaleDef (s.scale).minorish;
        const int lvl = chords[(size_t) i].level;
        chords[(size_t) i] = realise ((minor ? endMinor : endMajor)[rndInt (4)], lvl);
    }

    // ---- Voicing --------------------------------------------------------------
    std::vector<int> voiceUpper (const Chord& c, const std::vector<int>& prev) const
    {
        const ChordType& t = chordType (c.type);
        std::vector<int> iv (t.iv, t.iv + t.n);

        if (iv.size() >= 6)   // grosse Akkorde: Quinte weglassen
            iv.erase (std::remove (iv.begin(), iv.end(), 7), iv.end());

        if (s.voicing == kShell && t.n >= 4)
        {
            std::vector<int> sh;
            if (! s.bass) sh.push_back (0);
            sh.push_back (t.iv[1]);
            sh.push_back (t.iv[3]);
            if (t.n >= 5) sh.push_back (t.iv[t.n - 1]);
            iv = sh;
        }
        else if (s.voicing == kRootless && iv.size() >= 4)
        {
            iv.erase (iv.begin());
        }

        const int oct    = std::clamp (s.octave, -1, 1);
        const int base   = 48 + 12 * oct + c.root;
        const double mid = 62.0 + 12.0 * oct;

        std::vector<int> cur;
        for (int x : iv) cur.push_back (base + x);
        std::sort (cur.begin(), cur.end());

        auto mean = [] (const std::vector<int>& v)
        {
            double m = 0.0;
            for (int x : v) m += x;
            return v.empty() ? 0.0 : m / (double) v.size();
        };

        std::vector<int> best = cur;
        double bestCost = 1.0e9;
        const int rotations = s.voiceLead ? (int) cur.size() : 1;

        for (int r = 0; r < rotations; ++r)
        {
            if (r > 0)
            {
                cur[0] += 12;
                std::sort (cur.begin(), cur.end());
                cur.erase (std::unique (cur.begin(), cur.end()), cur.end());
            }

            for (int shift = -12; shift <= 12; shift += 12)
            {
                std::vector<int> cand = cur;
                for (int& x : cand) x += shift;

                const double m = mean (cand);
                double cost = 0.0;
                if (cand.front() < 43 + 12 * oct) cost += 50.0;
                if (cand.back()  > 86 + 12 * oct) cost += 50.0;
                if (cand.size() > 1 && cand.front() < 52 && cand[1] - cand[0] <= 2) cost += 4.0;

                if (! prev.empty() && s.voiceLead)
                    cost += std::abs (m - mean (prev)) + 0.6 * std::abs (cand.back() - prev.back()) + 0.2 * std::abs (m - mid);
                else
                    cost += std::abs (m - mid) + 0.5 * r;

                if (cost < bestCost)
                {
                    bestCost = cost;
                    best = cand;
                }
            }
        }

        const int n = (int) best.size();
        if (s.voicing == kOpen && n >= 3)
        {
            if (n == 3) best[1] += 12;
            else for (int i = 1; i < n; i += 2) best[(size_t) i] += 12;
            std::sort (best.begin(), best.end());
            if (best.back() > 91 && best.front() - 12 >= 40)
                for (int& x : best) x -= 12;
        }
        else if (s.voicing == kDrop2 && n >= 3)
        {
            best[(size_t) (n == 3 ? 1 : n - 2)] -= 12;
            std::sort (best.begin(), best.end());
            if (best.front() < 40)
                for (int& x : best) x += 12;
        }

        best.erase (std::unique (best.begin(), best.end()), best.end());
        return best;
    }
};

// ----------------------------------------------------------------------------
//  Standard-MIDI-Datei (Format 0, 480 PPQ)
// ----------------------------------------------------------------------------
inline std::vector<uint8_t> writeMidiFile (const ProgData& p, double bpm)
{
    struct Ev { uint32_t tick; int on; int note; int vel; };
    std::vector<Ev> evs;
    const double ppq = 480.0;

    for (int i = 0; i < p.numEvents; ++i)
    {
        const NoteEvent& e = p.events[i];
        const auto on  = (uint32_t) std::max (0L, std::lround (e.start * ppq));
        const auto off = std::max (on + 10u, (uint32_t) std::max (0L, std::lround ((e.start + e.length) * ppq)));
        evs.push_back ({ on, 1, e.note, e.velocity });
        evs.push_back ({ off, 0, e.note, 0 });
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

    // Spurname
    const std::string name = "Purrgression";
    vlq (0); trk.push_back (0xff); trk.push_back (0x03); trk.push_back ((uint8_t) name.size());
    trk.insert (trk.end(), name.begin(), name.end());

    // Tempo + Taktart
    const auto us = (uint32_t) std::lround (60000000.0 / std::clamp (bpm, 20.0, 999.0));
    vlq (0); trk.push_back (0xff); trk.push_back (0x51); trk.push_back (0x03);
    trk.push_back ((uint8_t) (us >> 16)); trk.push_back ((uint8_t) (us >> 8)); trk.push_back ((uint8_t) us);
    vlq (0); trk.push_back (0xff); trk.push_back (0x58); trk.push_back (0x04);
    trk.push_back (4); trk.push_back (2); trk.push_back (24); trk.push_back (8);

    uint32_t last = 0;
    for (const Ev& e : evs)
    {
        vlq (e.tick - last);
        last = e.tick;
        trk.push_back ((uint8_t) (e.on ? 0x90 : 0x80));
        trk.push_back ((uint8_t) e.note);
        trk.push_back ((uint8_t) e.vel);
    }

    // Spur bis zum Loop-Ende auffuellen, damit die Laenge im DAW stimmt
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

} // namespace purr
