#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ChordEngine.h"
#include "PreviewSynth.h"

#include <atomic>
#include <vector>

class PurrgressionProcessor : public juce::AudioProcessor
{
public:
    PurrgressionProcessor();
    ~PurrgressionProcessor() override = default;

    // ---- juce::AudioProcessor ------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }

    const juce::String getName() const override              { return "Purrgression"; }
    bool acceptsMidi() const override                        { return true; }
    bool producesMidi() const override                       { return true; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 1.0; }

    int getNumPrograms() override                            { return 1; }
    int getCurrentProgram() override                         { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Schnittstelle fuer das Overlay (Message-Thread) ---------------------
    struct ChordView
    {
        juce::String name, roman, notes;
        bool locked = false;
        int quality = purr::kQMajor;
        std::vector<int> midi;
    };

    enum class SettingsResult { soundOnly, revoiced, transposed, resized, regenerated };

    purr::Settings getSettings() const;
    SettingsResult updateSettings (const purr::Settings& newSettings);

    std::vector<ChordView> getChordViews() const;
    juce::String getKeyLabel() const;

    void generate();
    void vary();
    void rerollChord (int index);
    bool toggleLock (int index);            // liefert den neuen Zustand
    bool stepHistory (int direction);       // -1 zurueck, +1 vor
    bool canStepHistory (int direction) const;

    void setPlaying (bool shouldPlay)        { internalPlay.store (shouldPlay); }
    bool isPlaying() const                   { return internalPlay.load(); }
    bool isSounding() const                  { return running.load(); }
    void audition (int chordIndex)           { auditionRequest.store (chordIndex); }
    int  getCurrentChord() const             { return currentChord.load(); }
    float getBeatPhase() const               { return beatPhase.load(); }
    int  getUiVersion() const                { return uiVersion.load(); }

    juce::File writeTempMidi() const;        // fuer Drag & Drop in den Host

private:
    void publish();                          // Engine -> Audio-Thread + UI
    void pushHistory();

    void startNote (int note, int velocity, double offsetBeats, double lengthBeats, double blockBeats, int numSamples);
    void flushActive (int samplePosition);
    void releaseTrigger (int index, int samplePosition);

    // ---- Zustand (Message-Thread, durch stateLock geschuetzt) ----------------
    juce::CriticalSection stateLock;
    purr::Engine engine;
    purr::ProgData scratch;

    struct Snapshot { std::vector<purr::Chord> chords; uint32_t seed = 0; int key = 0; };
    std::vector<Snapshot> history;
    int historyPos = -1;

    // ---- Uebergabe an den Audio-Thread ----------------------------------------
    juce::SpinLock progLock;
    purr::ProgData shared;
    std::atomic<int> progVersion { 0 };

    std::atomic<bool>   internalPlay { false };
    std::atomic<bool>   hostSync { true };
    std::atomic<int>    sound { 1 };
    std::atomic<int>    auditionRequest { -1 };
    std::atomic<int>    currentChord { -1 };
    std::atomic<float>  beatPhase { 0.0f };
    std::atomic<bool>   running { false };
    std::atomic<double> lastBpm { 120.0 };
    std::atomic<int>    uiVersion { 0 };

    // ---- nur Audio-Thread -------------------------------------------------------
    purr::ProgData audioProg;
    int    seenVersion = -1;
    double pos = 0.0;
    bool   hostMode = false;
    double expectedPpq = 0.0;
    int    lastSound = -1;

    struct ActiveNote { int note = 0; double remaining = 0.0; };
    static constexpr int maxActive = 96;
    ActiveNote active[maxActive];
    int numActive = 0;

    bool trigOn[purr::kMaxChords] {};
    int  trigSize[purr::kMaxChords] {};
    int  trigNotes[purr::kMaxChords][purr::kMaxChordNotes] {};

    juce::MidiBuffer midiOut;
    purr::PreviewSynth synth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PurrgressionProcessor)
};
