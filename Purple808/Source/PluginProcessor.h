#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "BassEngine.h"
#include "BassSynth.h"

#include <atomic>
#include <vector>

class Purple808Processor : public juce::AudioProcessor
{
public:
    Purple808Processor();
    ~Purple808Processor() override = default;

    // ---- juce::AudioProcessor ------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }

    const juce::String getName() const override              { return "Purple 808"; }
    bool acceptsMidi() const override                        { return true; }
    bool producesMidi() const override                       { return true; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 3.0; }

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
        std::vector<p808::NoteEvent> events;
        std::vector<bool> locked;
        juce::StringArray barLabels;      // Grundton je Takt
        p808::Stats stats;
        int rootPitchClass = 5;
    };

    p808::Settings getSettings() const;
    void updateSettings (const p808::Settings& newSettings);
    View getView() const;

    void generate();
    void vary();
    void rerollBar (int bar);
    bool toggleLock (int bar);               // liefert den neuen Zustand
    bool stepHistory (int direction);        // -1 zurueck, +1 vor
    bool canStepHistory (int direction) const;
    bool syncTempo();                        // Pattern ans Host-Tempo anpassen; true = hat sich geaendert
    void audition();                         // Grundton kurz anspielen

    void setPlaying (bool shouldPlay)        { internalPlay.store (shouldPlay); }
    bool isPlaying() const                   { return internalPlay.load(); }
    double getPlayPosition() const           { return playPos.load(); }      // in Vierteln, -1 = steht
    double getBpm() const                    { return lastBpm.load(); }
    bool hasHostTempo() const                { return haveHostBpm.load(); }
    int  getUiVersion() const                { return uiVersion.load(); }

    juce::File writeTempMidi() const;        // fuer Drag & Drop in den Host

private:
    void publish();
    void pushHistory();
    void pushSoundParams();

    void fire (const p808::NoteEvent& e, double offsetBeats, double blockBeats, int numSamples);
    void flushActive (int samplePosition);
    void addTrigger (int sample, int type, int note, float velocity, int serial);

    // ---- Zustand (Message-Thread, durch stateLock geschuetzt) ----------------
    juce::CriticalSection stateLock;
    p808::Engine engine;
    p808::PatternData scratch;

    struct Snapshot { std::vector<p808::Bar> bars; uint32_t prog = 0, seed = 0; };
    std::vector<Snapshot> history;
    int historyPos = -1;

    // ---- Uebergabe an den Audio-Thread ----------------------------------------
    juce::SpinLock patternLock;
    p808::PatternData shared;
    std::atomic<int> patternVersion { 0 };

    std::atomic<bool>   internalPlay { false };
    std::atomic<bool>   hostSync { true };
    std::atomic<int>    kit { 2 };
    std::atomic<float>  pDistortion { 0.3f }, pTone { 0.6f }, pPunch { 0.5f }, pDecay { 0.6f },
                        pGlide { 0.4f }, pSub { 0.4f }, pReverb { 0.1f }, pLevel { 0.8f };
    std::atomic<int>    auditionNote { -1 };
    std::atomic<double> playPos { -1.0 };
    std::atomic<double> lastBpm { 140.0 };
    std::atomic<bool>   haveHostBpm { false };
    std::atomic<int>    uiVersion { 0 };

    // ---- nur Audio-Thread -------------------------------------------------------
    p808::PatternData audioPattern;
    int    seenVersion = -1;
    double pos = 0.0;
    bool   running = false, hostMode = false;
    double expectedPpq = 0.0;
    int    serialCounter = 0;
    float  lastReverb = -1.0f, hpState = 0.0f;

    struct ActiveNote { int note = 0, serial = 0; double remaining = 0.0; };
    static constexpr int maxActive = 32;
    ActiveNote active[maxActive];
    int numActive = 0;

    enum { trigNote, trigGlide, trigOff };
    struct Trigger { int sample = 0, type = 0, note = 0; float velocity = 0.0f; int serial = 0; };
    static constexpr int maxTriggers = 128;
    Trigger triggers[maxTriggers];
    int numTriggers = 0;

    juce::MidiBuffer midiOut;
    p808::BassSynth synth;
    juce::Reverb reverb;
    std::vector<float> monoBuf, wetL, wetR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Purple808Processor)
};
