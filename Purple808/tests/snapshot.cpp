// Rendert das echte Overlay ohne Host in PNG-Dateien und prueft den Prozessor.
// Aufruf:  p808_snapshot <ausgabe-ordner>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

#include <cstdio>

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("FAIL: %s\n", msg); } } while (0)

static void savePng (juce::Component& c, const juce::File& file)
{
    const auto image = c.createComponentSnapshot (c.getLocalBounds(), true, 2.0f);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;
    CHECK (out.openedOk() && png.writeImageToStream (image, out), "png write");
    std::printf ("geschrieben: %s (%d x %d)\n", file.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
}

struct RunStats { int noteOns = 0, noteOffs = 0; float peak = 0.0f; bool finite = true; };

static RunStats run (Purple808Processor& proc, int blocks)
{
    RunStats st;
    juce::AudioBuffer<float> buffer (2, 512);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        proc.processBlock (buffer, midi);
        for (const auto meta : midi)
        {
            if (meta.getMessage().isNoteOn())  ++st.noteOns;
            if (meta.getMessage().isNoteOff()) ++st.noteOffs;
        }
        st.peak = juce::jmax (st.peak, buffer.getMagnitude (0, 512));
        for (int i = 0; i < 512; ++i)
            st.finite = st.finite && std::isfinite (buffer.getSample (0, i)) && std::isfinite (buffer.getSample (1, i));
    }
    return st;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : ".");
    outDir.createDirectory();

    Purple808Processor proc;
    proc.setRateAndBufferSizeDetails (48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    // --- Stille im Leerlauf, Anspielen, dann ein ganzer Loop ---------------------------
    auto st = run (proc, 20);
    CHECK (st.noteOns == 0 && st.peak <= 0.0f, "idle is silent");

    proc.audition();
    st = run (proc, 150);
    CHECK (st.noteOns == 1 && st.noteOffs == 1 && st.peak > 0.1f, "audition plays one note");
    run (proc, 300);

    const int expected = (int) proc.getView().events.size();
    proc.setPlaying (true);
    st = run (proc, 643);              // 4 Takte bei 140 BPM
    CHECK (st.finite && st.peak > 0.1f && st.peak <= 1.0f, "playback audio");
    CHECK (std::abs (st.noteOns - expected) <= 1, "one loop plays every note once");
    CHECK (proc.getPlayPosition() >= 0.0, "play position reported");
    proc.setPlaying (false);
    const auto tail = run (proc, 400);
    CHECK (st.noteOns == st.noteOffs + tail.noteOffs, "all notes released after stop");
    std::printf ("Playback: %d Note-Ons (erwartet %d), Peak %.3f\n", st.noteOns, expected, (double) st.peak);

    // --- Overlay --------------------------------------------------------------------
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto* editor = dynamic_cast<Purple808Editor*> (ed.get());
        CHECK (editor != nullptr, "editor type");
        for (int i = 0; i < 10; ++i) editor->pump();
        savePng (*ed, outDir.getChildFile ("p808_start.png"));

        // Jedes Bedienelement ausloesen (darf nicht abstuerzen, Audio muss sauber bleiben)
        for (int round = 0; round < 3; ++round)
        {
            for (auto* c : ed->getChildren())
            {
                if (auto* button = dynamic_cast<PillButton*> (c))
                    if (button->onClick != nullptr)
                        button->onClick();

                if (auto* view = dynamic_cast<PatternView*> (c))
                    for (int bar = 0; bar < proc.getSettings().bars; ++bar)
                    {
                        view->onReroll (bar); view->onLock (bar); view->onReroll (bar); view->onLock (bar);
                    }

                if (auto* grid = dynamic_cast<KitGrid*> (c))
                    for (int kit = 0; kit < p808::kNumKits; ++kit)
                    {
                        grid->onSelect (kit);
                        const auto r = run (proc, 40);
                        CHECK (r.finite && r.peak > 0.004f && r.peak <= 1.0f, "every 808 sounds and stays clean");
                    }

                if (auto* box = dynamic_cast<juce::ComboBox*> (c))
                    for (int id = 1; id <= box->getNumItems(); ++id)
                    {
                        box->setSelectedId (id, juce::sendNotificationSync);
                        const auto r = run (proc, 6);
                        CHECK (r.finite && r.peak <= 1.0f, "audio stays clean");
                        editor->pump();
                    }

                if (auto* slider = dynamic_cast<juce::Slider*> (c))
                    slider->setValue (0.1 + 0.45 * round, juce::sendNotificationSync);

                run (proc, 5);
                editor->pump();
            }
        }
        proc.setPlaying (false);
        run (proc, 300);
        CHECK ((int) proc.getView().locked.size() == proc.getSettings().bars, "bar count consistent");

        // Szene 1: Drill mit vielen Slides, laeuft
        auto s = p808::Settings();
        s.style = p808::kDrill; s.bars = 4; s.slides = 0.85f; s.spice = 0.6f; s.movement = 0.6f; s.kit = 4;
        proc.updateSettings (s);
        proc.generate();
        proc.toggleLock (2);
        proc.setPlaying (true);
        run (proc, 290);
        for (int i = 0; i < 5; ++i) editor->pump();
        savePng (*ed, outDir.getChildFile ("p808_drill_playing.png"));
        proc.setPlaying (false);
        run (proc, 100);
        proc.toggleLock (2);

        // Szene 2: Trap, 8 Takte
        s.style = p808::kTrap; s.bars = 8; s.slides = 0.5f; s.kit = 3; s.key = 1;
        proc.updateSettings (s);
        proc.generate();
        for (int i = 0; i < 5; ++i) editor->pump();
        savePng (*ed, outDir.getChildFile ("p808_trap_8bars.png"));
    }

    // --- MIDI-Datei ------------------------------------------------------------------
    const auto file = proc.writeTempMidi();
    CHECK (file.existsAsFile() && file.getSize() > 60, "midi file written");
    std::printf ("MIDI: %s (%lld Bytes)\n", file.getFullPathName().toRawUTF8(), (long long) file.getSize());

    // --- Projekt speichern/laden: Pattern muss exakt wiederkommen -----------------------
    proc.toggleLock (0);
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    Purple808Processor other;
    other.setStateInformation (state.getData(), (int) state.getSize());
    const auto a = proc.getView(), b = other.getView();
    CHECK (a.events.size() == b.events.size() && ! a.events.empty() && a.locked == b.locked, "state: same size");
    bool same = true;
    for (size_t i = 0; i < juce::jmin (a.events.size(), b.events.size()); ++i)
        same = same && std::abs (a.events[i].start - b.events[i].start) < 1.0e-4 && std::abs (a.events[i].length - b.events[i].length) < 1.0e-4
                    && a.events[i].note == b.events[i].note && a.events[i].kind == b.events[i].kind;
    CHECK (same, "state: pattern identical");
    CHECK (other.getSettings().kit == proc.getSettings().kit && other.getSettings().key == proc.getSettings().key, "state: settings");

    std::printf ("%s (%d Fehler)\n", failures == 0 ? "ALLE TESTS OK" : "TESTS FEHLGESCHLAGEN", failures);
    return failures == 0 ? 0 : 1;
}
