// Test der Hi-Hat-Engine ohne JUCE:  g++ -std=c++17 -O2 -I../Source engine_test.cpp -o engine_test
#include "HatEngine.h"
#include "HatSynth.h"

#include <cstdio>
#include <fstream>

using namespace spice;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } } while (0)

static PatternData pd;   // gross -> nicht auf den Stack

static void checkPattern (const Engine& e)
{
    e.build (pd);
    CHECK (pd.numBars == (int) e.bars.size() && pd.totalBeats == 4.0 * pd.numBars, "bars");
    CHECK (pd.numEvents > 0 && pd.numEvents <= kMaxEvents, "event count");

    double prev = -1.0;
    for (int i = 0; i < pd.numEvents; ++i)
    {
        const auto& ev = pd.events[i];
        CHECK (ev.start >= prev, "sorted");
        CHECK (ev.start >= 0.0 && ev.start < pd.totalBeats, "start in loop");
        CHECK (ev.length > 0.0 && ev.start + ev.length <= pd.totalBeats + 1e-9, "end in loop");
        CHECK (ev.velocity >= 1 && ev.velocity <= 127, "velocity");
        CHECK (ev.pitch >= -24 && ev.pitch <= 24, "pitch");
        if (i > 0)
        {
            CHECK (pd.events[i - 1].start + pd.events[i - 1].length <= ev.start + 1e-9, "no overlap");
            if (ev.kind == kRoll && pd.events[i - 1].kind == kRoll)
            {
                const double ms = (ev.start - pd.events[i - 1].start) * 60000.0 / e.bpm;
                CHECK (ms > 17.0, "roll not faster than ~19 ms");
            }
        }
        prev = ev.start;
    }
}

static void printBar (const Engine& e, int bar)
{
    char line[33];
    for (int i = 0; i < 32; ++i) line[i] = (i % 8 == 0) ? '|' : '.';
    line[32] = 0;
    int count[32] = {};
    for (const auto& h : e.bars[(size_t) bar].hits)
    {
        const int cell = std::clamp ((int) (h.start * 8.0 + 1e-6), 0, 31);
        const char c = h.kind == kOpen ? 'O' : (h.kind == kRoll ? (h.pitch > 0 ? '^' : (h.pitch < 0 ? 'v' : 'r')) : (h.kind == kGhost ? 'g' : (h.kind == kFlam ? 'f' : (h.pitch != 0 ? 'P' : 'x'))));
        if (count[cell]++ == 0 || c == 'O') line[cell] = c;
        else if (line[cell] == 'r' || line[cell] == '^' || line[cell] == 'v') line[cell] = (char) (c == 'r' ? 'R' : c);
    }
    std::printf ("   %s\n", line);
}

int main()
{
    int total = 0;
    const double bpms[] = { 70, 88, 100, 120, 140, 155, 170, 200 };

    for (int style = 0; style < kNumStyles; ++style)
        for (double bpm : bpms)
            for (int bars : { 1, 2, 4, 8 })
                for (int rep = 0; rep < 10; ++rep)
                {
                    Engine e ((uint32_t) (77 + total * 7919));
                    e.bpm = Engine::bucket (bpm);
                    e.s.style = style; e.s.bars = bars; e.s.grid = rep % kNumGrids;
                    e.s.rolls = (rep % 5) / 4.0f; e.s.spice = ((rep + 2) % 4) / 3.0f; e.s.open = (rep % 3) / 2.0f;
                    e.s.dynamics = (rep % 2) ? 1.0f : 0.3f; e.s.swing = (rep % 3 == 1) ? 0.6f : 0.0f; e.s.humanize = (rep % 2) ? 1.0f : 0.0f;
                    e.s.pitch = rep % 2; e.s.triplets = (rep % 3) != 0; e.s.gaps = (rep % 4) != 0; e.s.flams = (rep % 2) == 0;
                    e.generate();
                    CHECK ((int) e.bars.size() == bars, "bar count");
                    checkPattern (e);

                    // deterministisch: gleicher Zustand -> gleiches Pattern
                    const auto before = e.bars;
                    e.rebuild();
                    bool same = true;
                    for (size_t b = 0; b < before.size(); ++b)
                        same = same && before[b].hits.size() == e.bars[b].hits.size();
                    CHECK (same, "deterministic");

                    // festgehaltene Takte bleiben bei allem stehen
                    e.bars[0].locked = true;
                    const auto keep = e.bars[0].hits;
                    e.s.rolls = 1.0f; e.s.spice = 1.0f;
                    e.rebuild(); e.vary(); e.rerollBar (0); e.generate();
                    CHECK (keep.size() == e.bars[0].hits.size(), "lock survives");
                    e.resize (8); e.resize (2);
                    checkPattern (e);

                    // Rolls = 0 -> keine Roll-Schlaege
                    e.bars[0].locked = false;
                    e.s.rolls = 0.0f; e.rebuild();
                    bool anyRoll = false;
                    for (const auto& b : e.bars) for (const auto& h : b.hits) anyRoll = anyRoll || h.kind == kRoll;
                    CHECK (! anyRoll, "rolls knob at zero");
                    ++total;
                }

    std::printf ("%d Durchlaeufe\n", total);

    // Beispiele (ein Zeichen = 1/32; x Hat, r Roll, ^ v gepitchter Roll, O Open, g Ghost, f Flam, P gepitchter Hit)
    struct Demo { int style; double bpm; float rolls, spice, open; };
    const Demo demos[] = { { kTrap, 140, 0.6f, 0.6f, 0.4f }, { kDrill, 142, 0.5f, 0.6f, 0.5f }, { kRage, 150, 0.4f, 0.5f, 0.8f },
                           { kDetroit, 100, 0.4f, 0.7f, 0.3f }, { kWestCoast, 95, 0.4f, 0.5f, 0.7f }, { kBoomBap, 90, 0.3f, 0.7f, 0.6f },
                           { kLoFi, 80, 0.3f, 0.7f, 0.4f }, { kPhonk, 130, 0.7f, 0.5f, 0.3f } };
    int di = 0;
    for (const auto& d : demos)
    {
        Engine e ((uint32_t) (4711 + 13 * di++));
        e.bpm = Engine::bucket (d.bpm);
        e.s.style = d.style; e.s.rolls = d.rolls; e.s.spice = d.spice; e.s.open = d.open; e.s.bars = 4;
        e.applyStyleDefaults();
        e.generate();
        const auto st = e.stats();
        std::printf ("\n%s, %.0f BPM, Raster %.3f: %d Hits, %d Rolls, %d Open\n", styleName (d.style), e.bpm, e.baseStep(), st.hits, st.rolls, st.opens);
        for (int b = 0; b < 4; ++b) printBar (e, b);
    }

    // Regler wirken live: mehr Rolls -> nie weniger Roll-Schlaege (gleiche Seeds)
    {
        Engine e (999);
        e.s.bars = 4; e.generate();
        int prevCount = -1;
        bool monotonic = true;
        for (int i = 0; i <= 10; ++i)
        {
            e.s.rolls = i / 10.0f; e.rebuild();
            int c = 0;
            for (const auto& b : e.bars) for (const auto& h : b.hits) c += h.kind == kRoll ? 1 : 0;
            if (c + 6 < prevCount) monotonic = false;
            prevCount = c;
        }
        CHECK (monotonic, "rolls knob roughly monotonic");
    }

    // MIDI-Dateien
    Engine e (5);
    e.s.open = 0.9f; e.generate(); e.build (pd);
    const auto closed = writeMidiFile (pd, 140.0, 60, 0), open = writeMidiFile (pd, 140.0, 60, 1);
    CHECK (closed.size() > 60 && closed[0] == 'M' && open.size() > 30, "midi");
    CHECK (countEvents (pd, 0) + countEvents (pd, 1) == pd.numEvents && countEvents (pd, 1) > 0, "event split");
    std::ofstream ("/tmp/spice_hats.mid", std::ios::binary).write ((const char*) closed.data(), (std::streamsize) closed.size());
    std::ofstream ("/tmp/spice_open.mid", std::ios::binary).write ((const char*) open.data(), (std::streamsize) open.size());

    // Kits
    HatSynth synth;
    static float L[48000], R[48000];
    for (int kit = 1; kit < kNumKits; ++kit)
    {
        for (int open01 = 0; open01 < 2; ++open01)
        {
            std::fill (L, L + 48000, 0.0f); std::fill (R, R + 48000, 0.0f);
            synth.prepare (48000.0);
            synth.setKit (kit);
            synth.trigger (open01 == 1, 0.9f, 0, 0.0f);
            synth.render (L, R, 48000, 0.8f);
            float peak = 0.0f; double e1 = 0.0, e2 = 0.0; bool finite = true;
            for (int i = 0; i < 48000; ++i)
            {
                peak = std::max (peak, std::abs (L[i])); finite = finite && std::isfinite (L[i]);
                if (i < 2400) e1 += L[i] * L[i]; else if (i >= 9600 && i < 12000) e2 += L[i] * L[i];
            }
            std::printf ("%-6s %-6s peak %.3f, Energie 0-50 ms %.2f, 200-250 ms %.4f\n", kitName (kit), open01 ? "open" : "closed", peak, e1, e2);
            CHECK (finite && peak > 0.08f && peak < 0.98f, "kit level");
            if (open01 == 0) CHECK (e2 < e1 * 0.001, "closed hat is short");
            else             CHECK (e2 > e1 * 0.002, "open hat rings");
        }
    }

    std::printf ("\n%s (%d Fehler)\n", failures == 0 ? "ALLE TESTS OK" : "TESTS FEHLGESCHLAGEN", failures);
    return failures == 0 ? 0 : 1;
}
