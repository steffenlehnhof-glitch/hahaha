// Rendert das echte Overlay ohne Host in PNG-Dateien und prueft den Prozessor.
// Aufruf:  purr_snapshot <ausgabe-ordner>
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

struct RunStats { int noteOns = 0, noteOffs = 0; float peak = 0.0f; };

static RunStats run (PurrgressionProcessor& proc, int blocks, juce::MidiBuffer* firstBlockInput = nullptr)
{
    RunStats st;
    juce::AudioBuffer<float> buffer (2, 512);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0 && firstBlockInput != nullptr)
            midi = *firstBlockInput;
        proc.processBlock (buffer, midi);
        for (const auto meta : midi)
        {
            if (meta.getMessage().isNoteOn())  ++st.noteOns;
            if (meta.getMessage().isNoteOff()) ++st.noteOffs;
        }
        st.peak = juce::jmax (st.peak, buffer.getMagnitude (0, 512));
    }
    return st;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : ".");
    outDir.createDirectory();

    PurrgressionProcessor proc;
    proc.setRateAndBufferSizeDetails (48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    // --- Stille im Leerlauf ------------------------------------------------------
    auto st = run (proc, 20);
    CHECK (st.noteOns == 0 && st.peak <= 0.0f, "idle is silent");

    // --- Taste C5 (60) spielt Akkord 1, Loslassen beendet ihn ----------------------
    juce::MidiBuffer in;
    in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    st = run (proc, 40, &in);
    CHECK (st.noteOns >= 3 && st.noteOffs == 0 && st.peak > 0.01f, "key trigger plays chord");
    const int held = st.noteOns;
    in.clear();
    in.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    st = run (proc, 4, &in);
    CHECK (st.noteOffs == held, "key release stops chord");
    run (proc, 200);

    // --- Anhoeren + Abspielen --------------------------------------------------------
    proc.audition (1);
    st = run (proc, 200);
    CHECK (st.noteOns >= 3 && st.noteOns == st.noteOffs, "audition on/off balanced");

    proc.setPlaying (true);
    st = run (proc, 94 * 9);          // gut 9 Sekunden = mehr als ein Loop bei 120 BPM
    CHECK (st.noteOns > 12 && st.peak > 0.02f && st.peak < 1.0f, "playback produces notes + audio");
    CHECK (proc.getCurrentChord() >= 0, "current chord reported");
    proc.setPlaying (false);
    auto tail = run (proc, 300);
    CHECK (st.noteOns == st.noteOffs + tail.noteOffs, "all notes released after stop");
    std::printf ("Playback: %d Note-Ons, Peak %.3f\n", st.noteOns, (double) st.peak);

    // --- Overlay: Startzustand ---------------------------------------------------------
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto* editor = dynamic_cast<PurrgressionEditor*> (ed.get());
        CHECK (editor != nullptr, "editor type");
        for (int i = 0; i < 40; ++i) editor->pump();
        savePng (*ed, outDir.getChildFile ("overlay_start.png"));

        // Jedes Bedienelement einmal ausloesen (darf nicht abstuerzen, Texte muessen gueltig sein)
        for (int round = 0; round < 3; ++round)
        {
            for (auto* c : ed->getChildren())
            {
                if (auto* button = dynamic_cast<PillButton*> (c))
                    if (button->onClick != nullptr)
                        button->onClick();

                if (auto* card = dynamic_cast<ChordCard*> (c))
                    if (card->isVisible())
                    {
                        card->onSelect(); card->onLock(); card->onReroll(); card->onLock(); card->onReroll();
                    }

                if (auto* box = dynamic_cast<juce::ComboBox*> (c))
                    for (int id = 1; id <= box->getNumItems(); ++id)
                    {
                        box->setSelectedId (id, juce::sendNotificationSync);
                        run (proc, 3);
                        editor->pump();
                    }

                if (auto* slider = dynamic_cast<juce::Slider*> (c))
                    slider->setValue (0.25 + 0.25 * round, juce::sendNotificationSync);

                run (proc, 5);
                editor->pump();
            }
        }
        proc.setPlaying (false);
        run (proc, 300);
        CHECK (proc.getChordViews().size() == 8, "ui: chord count follows the box");

        // 8 Akkorde, Jazz, laeuft gerade
        auto s = proc.getSettings();
        s.style = purr::kJazz; s.scale = purr::kDorian; s.key = 2; s.numChords = 8;
        s.colour = 0.85f; s.spice = 0.6f; s.voicing = purr::kDrop2; s.pattern = purr::kLoFiBounce; s.swing = 0.5f;
        proc.updateSettings (s);
        proc.generate();
        proc.toggleLock (0);
        proc.setPlaying (true);
        run (proc, 94 * 5 + 20);
        for (int i = 0; i < 25; ++i) editor->pump();
        savePng (*ed, outDir.getChildFile ("overlay_jazz_playing.png"));
        proc.setPlaying (false);
        run (proc, 300);

        // 3 Akkorde, Lo-Fi in Dur
        s = proc.getSettings();
        s.style = purr::kLoFi; s.scale = purr::kMajor; s.key = 3; s.numChords = 3; s.colour = 0.6f;
        proc.updateSettings (s);
        proc.toggleLock (0);
        for (int i = 0; i < 70; ++i) editor->pump();
        savePng (*ed, outDir.getChildFile ("overlay_lofi.png"));
    }

    // --- MIDI-Datei -------------------------------------------------------------------
    const auto midiFile = proc.writeTempMidi();
    CHECK (midiFile.existsAsFile() && midiFile.getSize() > 60, "midi file written");
    std::printf ("MIDI: %s (%lld Bytes)\n", midiFile.getFullPathName().toRawUTF8(), (long long) midiFile.getSize());

    // --- Projekt speichern/laden ---------------------------------------------------------
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    PurrgressionProcessor other;
    other.setStateInformation (state.getData(), (int) state.getSize());
    const auto a = proc.getChordViews(), b = other.getChordViews();
    CHECK (a.size() == b.size() && ! a.empty(), "state: chord count");
    for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
        CHECK (a[i].name == b[i].name && a[i].roman == b[i].roman && a[i].midi == b[i].midi, "state: chords identical");
    CHECK (other.getSettings().style == proc.getSettings().style && other.getSettings().key == proc.getSettings().key, "state: settings");

    std::printf ("%s (%d Fehler)\n", failures == 0 ? "ALLE TESTS OK" : "TESTS FEHLGESCHLAGEN", failures);
    return failures == 0 ? 0 : 1;
}
