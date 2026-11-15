// Test der 808-Engine ohne JUCE:  g++ -std=c++17 -O2 -I../Source engine_test.cpp -o engine_test
#include "BassEngine.h"
#include "BassSynth.h"

#include <cstdio>
#include <fstream>

using namespace p808;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } } while (0)

static PatternData pd;

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
        CHECK (ev.note >= 12 && ev.note <= 96, "note range");
        if (i > 0)
        {
            const auto& p = pd.events[i - 1];
            if (ev.kind == kSlide)  CHECK (p.start + p.length > ev.start, "note before a slide overlaps it (legato)");
            else                    CHECK (p.start + p.length <= ev.start + 1e-9, "no overlap without slide");
            const double ms = (ev.start - p.start) * 60000.0 / e.bpm;
            CHECK (ms > 40.0, "hits not faster than an 808 can speak");
        }
        prev = ev.start;
    }
    CHECK (pd.events[0].start < 0.05, "pattern starts on the one");
}

static void printBar (const Engine& e, int bar)
{
    char line[33];
    for (int i = 0; i < 32; ++i) line[i] = (i % 8 == 0) ? '|' : '.';
    line[32] = 0;
    const int root = e.bars[(size_t) bar].root;
    for (const auto& h : e.bars[(size_t) bar].hits)
    {
        const int cell = std::clamp ((int) (h.start * 8.0 + 1e-6), 0, 31);
        char c = h.kind == kSlide ? (h.pitch > root ? '/' : '\\') : (h.kind == kStutter ? 's' : (h.kind == kGhost ? 'g' : (h.pitch == root ? 'X' : (h.pitch > root ? '+' : '-'))));
        line[cell] = c;
    }
    std::printf ("   %s  Grundton %s\n", line, noteName (e.rootMidi() + root).c_str());
}

int main()
{
    int total = 0;
    const double bpms[] = { 70, 90, 110, 130, 140, 150, 165, 190 };

    for (int style = 0; style < kNumStyles; ++style)
        for (double bpm : bpms)
            for (int bars : { 1, 2, 4, 8 })
                for (int rep = 0; rep < 10; ++rep)
                {
                    Engine e ((uint32_t) (31 + total * 7919));
                    e.bpm = Engine::bucket (bpm);
                    e.s.style = style; e.s.bars = bars; e.s.key = rep % 12; e.s.scale = rep % kNumScales; e.s.octave = (rep % 3) - 1;
                    e.s.density = (rep % 5) / 4.0f; e.s.slides = ((rep + 1) % 4) / 3.0f; e.s.spice = ((rep + 2) % 4) / 3.0f;
                    e.s.movement = (rep % 3) / 2.0f; e.s.gate = (rep % 4) / 3.0f; e.s.swing = (rep % 3 == 1) ? 0.6f : 0.0f;
                    e.s.stutter = rep % 2; e.s.octaves = (rep % 3) != 0; e.s.triplets = (rep % 4) != 0; e.s.gaps = (rep % 2) == 0;
                    e.generate();
                    CHECK ((int) e.bars.size() == bars, "bar count");
                    checkPattern (e);

                    e.bars[0].locked = true;
                    const auto keep = e.bars[0].hits;
                    e.s.slides = 1.0f; e.s.spice = 1.0f; e.s.density = 1.0f;
                    e.rebuild(); e.vary(); e.rerollBar (0); e.generate();
                    CHECK (keep.size() == e.bars[0].hits.size(), "lock survives");
                    e.resize (8); e.resize (2);
                    checkPattern (e);

                    e.bars[0].locked = false;
                    e.s.slides = 0.0f; e.rebuild();
                    bool anySlide = false;
                    for (const auto& b : e.bars) for (const auto& h : b.hits) anySlide = anySlide || h.kind == kSlide;
                    CHECK (! anySlide, "slides knob at zero");
                    ++total;
                }

    std::printf ("%d Durchlaeufe\n", total);

    // Beispiele (ein Zeichen = 1/32; X Grundton, + - anderer Ton, / \\ Slide hoch/runter, s Stotter, g Ghost)
    struct Demo { int style; double bpm; float density, slides, spice, movement; };
    const Demo demos[] = { { kTrap, 140, 0.5f, 0.5f, 0.5f, 0.5f }, { kDrill, 142, 0.5f, 0.8f, 0.5f, 0.6f }, { kRage, 150, 0.5f, 0.3f, 0.5f, 0.4f },
                           { kDetroit, 100, 0.5f, 0.3f, 0.6f, 0.3f }, { kWestCoast, 95, 0.5f, 0.4f, 0.5f, 0.6f }, { kBoomBap, 90, 0.5f, 0.2f, 0.4f, 0.5f },
                           { kPhonk, 130, 0.5f, 0.6f, 0.6f, 0.4f }, { kJersey, 140, 0.5f, 0.5f, 0.5f, 0.5f } };
    int di = 0;
    for (const auto& d : demos)
    {
        Engine e ((uint32_t) (808 + 17 * di++));
        e.bpm = Engine::bucket (d.bpm);
        e.s.style = d.style; e.s.density = d.density; e.s.slides = d.slides; e.s.spice = d.spice; e.s.movement = d.movement; e.s.bars = 4;
        e.applyStyleDefaults();
        e.generate();
        const auto st = e.stats();
        std::printf ("\n%s, %.0f BPM: %d Noten, %d Slides\n", styleName (d.style), e.bpm, st.notes, st.slides);
        for (int b = 0; b < 4; ++b) printBar (e, b);
    }

    // MIDI-Datei
    Engine e (5);
    e.s.slides = 0.9f; e.generate(); e.build (pd);
    const auto bytes = writeMidiFile (pd, 140.0);
    CHECK (bytes.size() > 60 && bytes[0] == 'M', "midi");
    std::ofstream ("/tmp/purple808.mid", std::ios::binary).write ((const char*) bytes.data(), (std::streamsize) bytes.size());

    // Synth: Stimmung (A1 = 55 Hz), Pegel, Glide
    BassSynth synth;
    static float buf[96000];
    auto pitchAt = [&] (int from) -> double
    {
        double best = -1.0e18; int bestLag = 0;
        for (int lag = 300; lag <= 1300; ++lag)
        {
            double acc = 0.0;
            for (int i = from; i < from + 6000; ++i) acc += (double) buf[i] * (double) buf[i + lag];
            acc /= 1.0 + 0.0002 * lag;          // kleinsten passenden Abstand bevorzugen
            if (acc > best) { best = acc; bestLag = lag; }
        }
        return 48000.0 / bestLag;
    };

    for (int kit = 0; kit < kNumKits; ++kit)
    {
        SoundParams sp;
        sp.kit = kit; sp.decay = 1.0f;
        std::fill (buf, buf + 96000, 0.0f);
        synth.prepare (48000.0);
        synth.setParams (sp);
        synth.noteOn (33, 0.9f, 1);
        synth.render (buf, 48000);
        const double f1 = pitchAt (14000);
        synth.glideTo (45, 2);                 // Slide eine Oktave hoch
        synth.render (buf + 48000, 30000);
        const double f2 = pitchAt (48000 + 20000);
        synth.noteOff (2);
        synth.render (buf + 78000, 18000);

        float peak = 0.0f, tail = 0.0f; bool finite = true;
        for (int i = 0; i < 96000; ++i) { peak = std::max (peak, std::abs (buf[i])); finite = finite && std::isfinite (buf[i]); }
        for (int i = 94000; i < 96000; ++i) tail = std::max (tail, std::abs (buf[i]));
        std::printf ("%-14s %.1f Hz -> Slide %.1f Hz, peak %.3f, nach Loslassen %.4f\n", kitName (kit), f1, f2, peak, tail);
        CHECK (finite && peak > 0.15f && peak < 0.99f, "kit level");
        CHECK (std::abs (f1 - 55.0) < 1.5, "kit tuning");
        CHECK (std::abs (f2 - 110.0) < 4.0, "glide reaches target");
        CHECK (tail < 0.02f, "note off releases");
    }

    std::printf ("\n%s (%d Fehler)\n", failures == 0 ? "ALLE TESTS OK" : "TESTS FEHLGESCHLAGEN", failures);
    return failures == 0 ? 0 : 1;
}
