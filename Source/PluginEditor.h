#pragma once

#include "PluginProcessor.h"
#include "Mascot.h"
#include "Widgets.h"

#include <array>
#include <initializer_list>

class PurrgressionEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PurrgressionEditor (PurrgressionProcessor&);
    ~PurrgressionEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void pump()     { timerCallback(); }   // fuer den Schnappschuss-Test ohne Message-Loop

private:
    void timerCallback() override;

    void refresh();                 // Prozessor -> Overlay
    void pushSettings();            // Overlay -> Prozessor
    void layoutCards();
    void updateKeyboard();
    void say (std::initializer_list<const char*> options);
    void sayForStyle();

    void setupCombo (juce::ComboBox&, const juce::StringArray& items);
    void setupKnob (juce::Slider&, double defaultValue);

    PurrgressionProcessor& proc;
    PurrLookAndFeel lnf;

    CatComponent cat;
    std::array<ChordCard, purr::kMaxChords> cards;
    KeyboardView keyboard;
    PresetBrowser presets;

    juce::ComboBox keyBox, scaleBox, styleBox;
    juce::ComboBox countBox, durationBox, voicingBox, patternBox, soundBox, octaveBox;
    juce::Slider colourKnob, spiceKnob, humanKnob, swingKnob, reverbKnob;
    ToggleChip bassChip, leadChip, syncChip;
    PillButton generateButton, varyButton, prevButton, nextButton, playButton;
    DragTile dragTile;

    struct Caption { juce::Rectangle<int> area; juce::String text; juce::Justification just; };
    std::vector<Caption> captions;
    juce::Rectangle<int> cardsArea, controlsPanel, legendArea;

    std::vector<PurrgressionProcessor::ChordView> views;
    juce::Colour accent = theme::purple;
    juce::Random random;

    int  seenUiVersion = -1;
    int  shownCurrent = -2;
    int  selected = 0;
    int  numCards = 0;
    bool wasPlaying = false;
    bool hintedKnobs = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PurrgressionEditor)
};
