// Rendert das echte Overlay ohne Host in PNG-Dateien und prueft den Prozessor.
// Aufruf:  spice_snapshot <ausgabe-ordner>
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

static RunStats run (PurpleSpiceProcessor& proc, int blocks)
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

    PurpleSpiceProcessor proc;
    proc.setRateAndBufferSizeDetails (48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    // --- Stille im Leerlauf, dann ein ganzer Loop --------------------------------
    auto st = run (proc, 20);
    CHECK (st.noteOns == 0 && st.peak <= 0.0f, "idle is silent");

    const int expected = (int) proc.getView().events.size();
    proc.setPlaying (true);
    // 4 Takte bei 140 BPM = 6.857 s = 643 Bloecke
    st = run (proc, 643);
    CHECK (st.finite && st.peak > 0.03f && st.peak <= 1.0f, "playback audio");
    CHECK (std::abs (st.noteOns - expected) <= 2, "one loop plays every hit once");
    CHECK (proc.getPlayPosition() >= 0.0, "play position reported");
    proc.setPlaying (false);
    const auto tail = run (proc, 200);
    CHECK (st.noteOns == st.noteOffs + tail.noteOffs, "all notes released after stop");
    CHECK (proc.getPlayPosition() < 0.0, "stopped");
    std::printf ("Playback: %d Note-Ons (erwartet %d), Peak %.3f\n", st.noteOns, expected, (double) st.peak);

    // --- Overlay --------------------------------------------------------------------
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto* editor = dynamic_cast<PurpleSpiceEditor*> (ed.get());
        CHECK (editor != nullptr, "editor type");
        for (int i = 0; i < 10; ++i) editor->pump();
        savePng (*ed, outDir.getChildFile ("spice_start.png"));

        // Jedes Bedienelement ausloesen (darf nicht abstuerzen)
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

                if (auto* box = dynamic_cast<juce::ComboBox*> (c))
                    for (int id = 1; id <= box->getNumItems(); ++id)
                    {
                        box->setSelectedId (id, juce::sendNotificationSync);
                        const auto r = run (proc, 6);
                        CHECK (r.finite && r.peak <= 1.0f, "audio stays clean");
                        editor->pump();
                    }

                if (auto* slider = dynamic_cast<juce::Slider*> (c))
                    slider->setValue (0.2 + 0.4 * round, juce::sendNotificationSync);

                run (proc, 5);
                editor->pump();
            }
        }
        proc.setPlaying (false);
        run (proc, 200);
        CHECK ((int) proc.getView().locked.size() == proc.getSettings().bars, "bar count consistent");

        // Szene 1: Trap, 4 Takte, laeuft
        auto s = spice::Settings();
        s.style = spice::kTrap; s.bars = 4; s.rolls = 0.6f; s.spice = 0.65f; s.open = 0.45f;
        proc.updateSettings (s);
        proc.generate();
        proc.toggleLock (1);
        proc.setPlaying (true);
        run (proc, 290);
        for (int i = 0; i < 5; ++i) editor->pump();
        savePng (*ed, outDir.getChildFile ("spice_trap_playing.png"));
        proc.setPlaying (false);
        run (proc, 100);
        proc.toggleLock (1);

        // Szene 2: Drill, 8 Takte
        s.style = spice::kDrill; s.bars = 8; s.rolls = 0.5f; s.spice = 0.7f; s.open = 0.6f; s.kit = 2;
        proc.updateSettings (s);
        proc.generate();
        for (int i = 0; i < 5; ++i) editor->pump();
        savePng (*ed, outDir.getChildFile ("spice_drill_8bars.png"));

        // Szene 3: Boom-Bap, 2 Takte
        s.style = spice::kBoomBap; s.bars = 2; s.rolls = 0.35f; s.spice = 0.8f; s.open = 0.7f; s.kit = 3;
        proc.updateSettings (s);
        proc.generate();
        for (int i = 0; i < 5; ++i) editor->pump();
        savePng (*ed, outDir.getChildFile ("spice_boombap_2bars.png"));
    }

    // --- MIDI-Dateien ------------------------------------------------------------------
    const auto hats = proc.writeTempMidi (0);
    CHECK (hats.existsAsFile() && hats.getSize() > 60, "hi-hat midi file written");
    std::printf ("MIDI: %s (%lld Bytes)\n", hats.getFullPathName().toRawUTF8(), (long long) hats.getSize());

    // --- Projekt speichern/laden: Pattern muss exakt wiederkommen -----------------------
    proc.toggleLock (0);
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    PurpleSpiceProcessor other;
    other.setStateInformation (state.getData(), (int) state.getSize());
    const auto a = proc.getView(), b = other.getView();
    CHECK (a.events.size() == b.events.size() && ! a.events.empty() && a.locked == b.locked, "state: same size");
    bool same = true;
    for (size_t i = 0; i < juce::jmin (a.events.size(), b.events.size()); ++i)
        same = same && std::abs (a.events[i].start - b.events[i].start) < 1.0e-4 && a.events[i].velocity == b.events[i].velocity
                    && a.events[i].pitch == b.events[i].pitch && a.events[i].kind == b.events[i].kind;
    CHECK (same, "state: pattern identical");
    CHECK (other.getSettings().style == proc.getSettings().style && other.getSettings().kit == proc.getSettings().kit, "state: settings");

    std::printf ("%s (%d Fehler)\n", failures == 0 ? "ALLE TESTS OK" : "TESTS FEHLGESCHLAGEN", failures);
    return failures == 0 ? 0 : 1;
}
