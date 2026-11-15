#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

class Purple808Editor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit Purple808Editor (Purple808Processor&);
    ~Purple808Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void pump()     { timerCallback(); }   // fuer den Schnappschuss-Test ohne Message-Loop

private:
    void timerCallback() override;

    void refresh();                 // Prozessor -> Overlay
    void pushSettings();            // Overlay -> Prozessor
    void setHint (const juce::String& text);

    void setupCombo (juce::ComboBox&, const juce::StringArray& items);
    void setupKnob (juce::Slider&, double defaultValue);

    Purple808Processor& proc;
    DripLookAndFeel lnf;

    PatternView pattern;
    KitGrid kitGrid;

    juce::ComboBox styleBox, barsBox, keyBox, scaleBox, octaveBox;

    // Pattern
    juce::Slider densityKnob, slidesKnob, spiceKnob, moveKnob, gateKnob, swingKnob;
    ToggleChip stutterChip, octaveChip, tripletChip, gapChip, syncChip;

    // Sound
    juce::Slider distKnob, toneKnob, punchKnob, decayKnob, glideKnob, subKnob, reverbKnob, levelKnob;

    PillButton generateButton, varyButton, prevButton, nextButton, playButton;
    DragTile midiTile;

    struct Caption { juce::Rectangle<int> area; juce::String text; juce::Justification just; };
    std::vector<Caption> captions;
    juce::Rectangle<int> patternPanel, soundPanel, legendArea, tempoArea;

    juce::String statsText, hint;
    int  seenUiVersion = -1;
    int  shownBpm = -1;
    bool shownHostTempo = false;
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Purple808Editor)
};
