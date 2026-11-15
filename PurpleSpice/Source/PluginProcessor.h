#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "HatEngine.h"
#include "HatSynth.h"

#include <atomic>
#include <vector>

class PurpleSpiceProcessor : public juce::AudioProcessor
{
public:
    PurpleSpiceProcessor();
    ~PurpleSpiceProcessor() override = default;

    // ---- juce::AudioProcessor ------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }

    const juce::String getName() const override              { return "Purple Spice"; }
    bool acceptsMidi() const override                        { return true; }
    bool producesMidi() const override                       { return true; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 0.5; }

    int getNumPrograms() override                            { return 1; }
    int getCurrentProgram() override                         { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Schnittstelle fuer das Overlay (Message-Thread) ---------------------
    struct View
    {
        std::vector<spice::NoteEvent> events;
        std::vector<bool> locked;
        spice::Stats stats;
    };

    enum class SettingsResult { soundOnly, rebuilt, resized };

    spice::Settings getSettings() const;
    SettingsResult updateSettings (const spice::Settings& newSettings);
    View getView() const;

    void generate();
    void vary();
    void rerollBar (int bar);
    bool toggleLock (int bar);               // liefert den neuen Zustand
    bool stepHistory (int direction);        // -1 zurueck, +1 vor
    bool canStepHistory (int direction) const;
    bool syncTempo();                        // Pattern ans Host-Tempo anpassen; true = hat sich geaendert

    void setPlaying (bool shouldPlay)        { internalPlay.store (shouldPlay); }
    bool isPlaying() const                   { return internalPlay.load(); }
    bool isSounding() const                  { return running.load(); }
    double getPlayPosition() const           { return playPos.load(); }      // in Vierteln, -1 = steht
    double getBpm() const                    { return lastBpm.load(); }
    bool hasHostTempo() const                { return haveHostBpm.load(); }
    int  getUiVersion() const                { return uiVersion.load(); }

    juce::File writeTempMidi (int which) const;   // 0 = Hi-Hats, 1 = Open Hats

private:
    void publish();
    void pushHistory();

    void fire (const spice::NoteEvent& e, double offsetBeats, double blockBeats, int numSamples);
    void flushActive (int samplePosition);
    void addTrigger (int sample, int type, float velocity, int pitch, float pan);

    // ---- Zustand (Message-Thread, durch stateLock geschuetzt) ----------------
    juce::CriticalSection stateLock;
    spice::Engine engine;
    spice::PatternData scratch;

    struct Snapshot { std::vector<spice::Bar> bars; uint32_t seed = 0; };
    std::vector<Snapshot> history;
    int historyPos = -1;

    // ---- Uebergabe an den Audio-Thread ----------------------------------------
    juce::SpinLock patternLock;
    spice::PatternData shared;
    std::atomic<int> patternVersion { 0 };

    std::atomic<bool>   internalPlay { false };
    std::atomic<bool>   hostSync { true };
    std::atomic<int>    kit { 1 };
    std::atomic<int>    rootNote { 60 };
    std::atomic<float>  level { 0.8f };
    std::atomic<double> playPos { -1.0 };
    std::atomic<bool>   running { false };
    std::atomic<double> lastBpm { 140.0 };
    std::atomic<bool>   haveHostBpm { false };
    std::atomic<int>    uiVersion { 0 };

    // ---- nur Audio-Thread -------------------------------------------------------
    spice::PatternData audioPattern;
    int    seenVersion = -1;
    double pos = 0.0;
    bool   hostMode = false;
    double expectedPpq = 0.0;
    int    lastKit = -1;
    bool   rollFlip = false;

    struct ActiveNote { int note = 0, channel = 1; bool open = false; double remaining = 0.0; };
    static constexpr int maxActive = 64;
    ActiveNote active[maxActive];
    int numActive = 0;

    enum { trigClosed, trigOpen, trigRelease };
    struct Trigger { int sample = 0, type = 0; float velocity = 0.0f; int pitch = 0; float pan = 0.0f; };
    static constexpr int maxTriggers = 256;
    Trigger triggers[maxTriggers];
    int numTriggers = 0;

    juce::MidiBuffer midiOut;
    spice::HatSynth synth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PurpleSpiceProcessor)
};
