#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

class PurpleSpiceEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit PurpleSpiceEditor (PurpleSpiceProcessor&);
    ~PurpleSpiceEditor() override;

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

    PurpleSpiceProcessor& proc;
    SpiceLookAndFeel lnf;

    PatternView pattern;

    juce::ComboBox styleBox, barsBox, gridBox, kitBox, rootBox;
    juce::Slider rollsKnob, spiceKnob, openKnob, dynKnob, swingKnob, humanKnob, levelKnob;
    ToggleChip pitchChip, tripletChip, gapChip, flamChip, syncChip;
    PillButton generateButton, varyButton, prevButton, nextButton, playButton;
    DragTile hatsTile, openTile;

    struct Caption { juce::Rectangle<int> area; juce::String text; juce::Justification just; };
    std::vector<Caption> captions;
    juce::Rectangle<int> controlsPanel, legendArea, tempoArea;

    juce::String statsText, hint;
    int  seenUiVersion = -1;
    int  shownBpm = -1;
    bool shownHostTempo = false;
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PurpleSpiceEditor)
};
