// Test der Chord-Engine ohne JUCE:  g++ -std=c++17 -O2 -I../Source engine_test.cpp -o engine_test
#include "ChordEngine.h"
#include "PreviewSynth.h"

#include <cstdio>
#include <fstream>
#include <set>

using namespace purr;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } } while (0)

static void checkProg (const Engine& e, const ProgData& p)
{
    CHECK (p.numChords == (int) e.chords.size(), "chord count");
    CHECK (p.totalBeats > 0.0, "total beats");
    CHECK (p.numEvents > 0 && p.numEvents <= kMaxEvents, "event count");

    for (int c = 0; c < p.numChords; ++c)
    {
        CHECK (p.chordSize[c] >= 2 && p.chordSize[c] <= kMaxChordNotes, "chord size");
        for (int k = 0; k < p.chordSize[c]; ++k)
        {
            CHECK (p.chordNotes[c][k] >= 21 && p.chordNotes[c][k] <= 108, "note range");
            if (k > 0) CHECK (p.chordNotes[c][k] > p.chordNotes[c][k - 1], "notes ascending");
        }
    }

    double prev = -1.0;
    for (int i = 0; i < p.numEvents; ++i)
    {
        const auto& ev = p.events[i];
        CHECK (ev.start >= prev, "events sorted");
        CHECK (ev.start >= 0.0 && ev.start < p.totalBeats, "event start in loop");
        CHECK (ev.length > 0.0 && ev.start + ev.length <= p.totalBeats + 1e-9, "event end in loop");
        CHECK (ev.velocity >= 1 && ev.velocity <= 127, "velocity");
        prev = ev.start;
    }
}

int main()
{
    static ProgData p;   // gross -> nicht auf den Stack
    std::set<std::string> uniqueProgs, uniqueChords;
    int total = 0;

    for (int style = 0; style < kNumStyles; ++style)
        for (int scale = 0; scale < kNumScales; ++scale)
            for (int key = 0; key < 12; key += 5)
                for (int n : { 2, 3, 4, 6, 8 })
                    for (int rep = 0; rep < 12; ++rep)
                    {
                        Engine e ((uint32_t) (1000 + total * 7919));
                        e.s.style = style; e.s.scale = scale; e.s.key = key; e.s.numChords = n;
                        e.s.colour = (rep % 4) / 3.0f;
                        e.s.spice  = (rep % 3) / 2.0f;
                        e.s.voicing = rep % kNumVoicings;
                        e.s.pattern = (rep + n) % kNumPatterns;
                        e.s.duration = rep % 3;
                        e.s.bass = (rep % 5) != 0;
                        e.s.voiceLead = (rep % 2) == 0;
                        e.s.octave = (rep % 3) - 1;
                        e.s.humanize = (rep % 2) ? 1.0f : 0.0f;
                        e.s.swing = (rep % 3) ? 0.6f : 0.0f;

                        e.generate();
                        CHECK ((int) e.chords.size() == n, "generate size");
                        e.buildProgData (p);
                        checkProg (e, p);

                        std::string line;
                        for (const auto& c : e.chords)
                        {
                            const std::string nm = e.chordName (c);
                            CHECK (! nm.empty() && ! e.roman (c).empty(), "names");
                            uniqueChords.insert (chordType (c.type).suffix);
                            line += nm + " ";
                        }
                        uniqueProgs.insert (line);

                        // Variation, Reroll, Lock
                        e.chords[0].locked = true;
                        const Chord keep = e.chords[0];
                        for (int k = 0; k < 4; ++k) { e.vary(); e.rerollChord (k % n); }
                        e.generate();
                        CHECK (sameChord (keep, e.chords[0]) && e.chords[0].locked, "lock survives");
                        e.resize (8); CHECK (e.chords.size() == 8, "resize up");
                        e.resize (3); CHECK (e.chords.size() == 3, "resize down");
                        e.buildProgData (p);
                        checkProg (e, p);
                        ++total;
                    }

    std::printf ("%d Durchlaeufe, %zu verschiedene Folgen, %zu Akkordtypen benutzt\n",
                 total, uniqueProgs.size(), uniqueChords.size());

    // Beispiele zum Anhoeren/Anschauen
    struct Demo { int style, scale, key; float colour, spice; };
    const Demo demos[] = { { kTrap, kNatMinor, 5, 0.3f, 0.3f }, { kTrap, kPhrygian, 1, 0.2f, 0.6f },
                           { kHipHop, kDorian, 2, 0.5f, 0.3f }, { kLoFi, kMajor, 3, 0.7f, 0.5f },
                           { kJazz, kHarmMinor, 0, 0.8f, 0.7f }, { kRnB, kNatMinor, 9, 0.7f, 0.4f } };
    int di = 0;
    for (const auto& d : demos)
    {
        Engine e ((uint32_t) (42 + 17 * di++));
        e.s.style = d.style; e.s.scale = d.scale; e.s.key = d.key; e.s.colour = d.colour; e.s.spice = d.spice;
        std::printf ("\n%s, %s %s\n", styleName (d.style), e.noteName (d.key).c_str(), scaleDef (d.scale).name);
        for (int k = 0; k < 4; ++k)
        {
            e.generate();
            std::printf ("   ");
            for (const auto& c : e.chords) std::printf ("%-10s", e.chordName (c).c_str());
            std::printf (" | ");
            for (const auto& c : e.chords) std::printf ("%s  ", e.roman (c).c_str());
            std::printf ("\n");
        }
        e.vary();
        std::printf ("   Variation: ");
        for (const auto& c : e.chords) std::printf ("%s  ", e.chordName (c).c_str());
        std::printf ("\n");
    }

    // MIDI-Datei + Synth
    Engine e (7);
    e.s.style = kLoFi; e.s.scale = kDorian; e.s.key = 2; e.s.colour = 0.8f; e.s.pattern = kLoFiBounce; e.s.swing = 0.5f;
    e.generate();
    e.buildProgData (p);
    const auto bytes = writeMidiFile (p, 84.0);
    CHECK (bytes.size() > 40 && bytes[0] == 'M' && bytes[3] == 'd', "midi header");
    std::ofstream ("/tmp/purr_test.mid", std::ios::binary).write ((const char*) bytes.data(), (std::streamsize) bytes.size());

    // Presets: jede Folge laden, in mehrere Tonarten setzen, variieren
    std::printf ("\n%d Presets\n", numPresets());
    for (int i = 0; i < numPresets(); ++i)
    {
        Engine pe ((uint32_t) (99 + i));
        pe.s.key = (i * 5) % 12;
        pe.loadPreset (i);
        CHECK ((int) pe.chords.size() == preset (i).n && pe.s.scale == preset (i).scale, "preset loads");
        pe.buildProgData (p);
        checkProg (pe, p);
        std::string names;
        for (const auto& c : pe.chords) names += pe.chordName (c) + " ";
        std::printf ("  %-8s %-18s %-44s %s\n", presetCategoryName (preset (i).category), preset (i).name,
                     Engine::presetRomans (i).c_str(), names.c_str());
        pe.transpose (3);
        pe.vary(); pe.rerollChord (1);
        pe.buildProgData (p);
        checkProg (pe, p);
    }

    // Instrumente: Pegel und Stimmung (A4 = 440 Hz)
    PreviewSynth synth;
    synth.prepare (48000.0);
    static float L[48000], R[48000];
    for (int snd = 1; snd < kNumSounds; ++snd)
    {
        // einzelner Ton -> Grundfrequenz per Autokorrelation
        std::fill (L, L + 48000, 0.0f); std::fill (R, R + 48000, 0.0f);
        synth.prepare (48000.0);
        synth.setSound (snd);
        synth.noteOn (69, 0.8f);
        synth.render (L, R, 48000);
        double best = -1.0e9; int bestLag = 0;
        for (int lag = 90; lag <= 130; ++lag)
        {
            double acc = 0.0;
            for (int i = 20000; i < 30000; ++i) acc += (double) L[i] * (double) L[i + lag];
            if (acc > best) { best = acc; bestLag = lag; }
        }
        const double freq = 48000.0 / bestLag;

        // Akkord -> Pegel
        std::fill (L, L + 48000, 0.0f); std::fill (R, R + 48000, 0.0f);
        synth.prepare (48000.0);
        synth.setSound (snd);
        for (int k = 0; k < p.chordSize[0]; ++k) synth.noteOn (p.chordNotes[0][k], 0.8f);
        synth.render (L, R, 24000);
        for (int k = 0; k < p.chordSize[0]; ++k) synth.noteOff (p.chordNotes[0][k]);
        synth.render (L + 24000, R + 24000, 24000);
        float peak = 0.0f, rms = 0.0f;
        bool finite = true;
        for (int i = 0; i < 48000; ++i) { peak = std::max (peak, std::abs (L[i])); rms += L[i] * L[i]; finite = finite && std::isfinite (L[i]); }
        rms = std::sqrt (rms / 48000.0f);
        std::printf ("%-9s Grundton %.1f Hz, Akkord: peak %.3f rms %.3f\n", soundName (snd), freq, peak, rms);
        CHECK (finite && peak > 0.05f && peak < 0.98f, "synth level");
        CHECK (rms > 0.02f && rms < 0.45f, "synth loudness");
        if (snd != 7) CHECK (std::abs (freq - 440.0) < 6.0, "synth tuning");   // Bell ist absichtlich unharmonisch
    }

    std::printf ("\n%s (%d Fehler)\n", failures == 0 ? "ALLE TESTS OK" : "TESTS FEHLGESCHLAGEN", failures);
    return failures == 0 ? 0 : 1;
}
