#include "PluginEditor.h"

using theme::T8;

namespace
{
const int barChoices[4]  = { 1, 2, 4, 8 };
const int rootChoices[4] = { 36, 48, 60, 72 };     // in FL Studio: C3, C4, C5, C6
}

//==============================================================================
PurpleSpiceEditor::PurpleSpiceEditor (PurpleSpiceProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    // ---- Auswahlboxen ----------------------------------------------------------
    juce::StringArray styles, grids, kits;
    for (int i = 0; i < spice::kNumStyles; ++i)  styles.add (T8 (spice::styleName (i)));
    for (int i = 0; i < spice::kNumGrids; ++i)   grids.add (T8 (spice::gridName (i)));
    for (int i = 0; i < spice::kNumKits; ++i)    kits.add (T8 (spice::kitName (i)));

    setupCombo (styleBox, styles);
    setupCombo (barsBox, { "1", "2", "4", "8" });
    setupCombo (gridBox, grids);
    setupCombo (kitBox, kits);
    setupCombo (rootBox, { "C3", "C4", "C5", "C6" });

    // ---- Regler + Schalter -----------------------------------------------------
    setupKnob (rollsKnob, 0.5);
    setupKnob (spiceKnob, 0.5);
    setupKnob (openKnob, 0.3);
    setupKnob (dynKnob, 0.6);
    setupKnob (swingKnob, 0.0);
    setupKnob (humanKnob, 0.15);
    setupKnob (levelKnob, 0.8);

    pitchChip.setText ("Pitch-Fahrten");
    tripletChip.setText ("Triolen");
    gapChip.setText ("Pausen");
    flamChip.setText ("Flams");
    syncChip.setText ("Host-Sync");
    for (auto* chip : { &pitchChip, &tripletChip, &gapChip, &flamChip, &syncChip })
    {
        chip->onChange = [this] { pushSettings(); };
        addAndMakeVisible (*chip);
    }

    // ---- Buttons ---------------------------------------------------------------
    generateButton.setText ("Generieren");
    generateButton.setIcon (PillButton::Icon::dice);
    generateButton.setPrimary (true);
    generateButton.setFontSize (19.0f);
    generateButton.onClick = [this]
    {
        proc.generate();
        setHint (T8 ("Neues Pattern. Festgehaltene Takte sind geblieben."));
        refresh();
    };

    varyButton.setText ("Variation");
    varyButton.setIcon (PillButton::Icon::sparkle);
    varyButton.onClick = [this]
    {
        proc.vary();
        setHint (T8 ("Gleicher Groove, neues Ear Candy in der zweiten Takthälfte."));
        refresh();
    };

    prevButton.setIcon (PillButton::Icon::prev);
    prevButton.onClick = [this] { if (proc.stepHistory (-1)) { setHint (T8 ("Einen Schritt zurück.")); refresh(); } };

    nextButton.setIcon (PillButton::Icon::next);
    nextButton.onClick = [this] { if (proc.stepHistory (1)) { setHint (T8 ("Einen Schritt vor.")); refresh(); } };

    playButton.setText ("Play");
    playButton.setIcon (PillButton::Icon::play);
    playButton.onClick = [this] { proc.setPlaying (! proc.isPlaying()); };

    for (auto* b : { &generateButton, &varyButton, &prevButton, &nextButton, &playButton })
        addAndMakeVisible (*b);

    // ---- Pattern-Ansicht -----------------------------------------------------------
    pattern.onLock = [this] (int bar)
    {
        setHint (proc.toggleLock (bar) ? T8 ("Takt ") + juce::String (bar + 1) + T8 (" bleibt jetzt, egal was du drehst.")
                                       : T8 ("Takt ") + juce::String (bar + 1) + T8 (" ist wieder frei."));
        refresh();
    };

    pattern.onReroll = [this] (int bar)
    {
        proc.rerollBar (bar);
        setHint (T8 ("Takt ") + juce::String (bar + 1) + T8 (" neu gewürfelt."));
        refresh();
    };
    addAndMakeVisible (pattern);

    // ---- MIDI herausziehen -----------------------------------------------------------
    hatsTile.fileProvider = [this] { return proc.writeTempMidi (0); };
    hatsTile.onHint       = [this] { setHint (T8 ("Feld festhalten und in die Piano Roll deiner Hi-Hat ziehen.")); };
    hatsTile.onDragged    = [this] { setHint (T8 ("Hi-Hats abgeliefert.")); };
    openTile.fileProvider = [this] { return proc.writeTempMidi (1); };
    openTile.onHint       = [this] { setHint (T8 ("Feld festhalten und in die Piano Roll deiner Open Hat ziehen.")); };
    openTile.onDragged    = [this] { setHint (T8 ("Open Hats abgeliefert.")); };
    addAndMakeVisible (hatsTile);
    addAndMakeVisible (openTile);

    hint = T8 ("Schloss hält einen Takt fest, der Würfel würfelt nur diesen Takt neu.");

    setResizable (false, false);
    setSize (1120, 650);
    refresh();
    startTimerHz (30);
}

PurpleSpiceEditor::~PurpleSpiceEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PurpleSpiceEditor::setupCombo (juce::ComboBox& box, const juce::StringArray& items)
{
    box.addItemList (items, 1);
    box.onChange = [this] { pushSettings(); };
    addAndMakeVisible (box);
}

void PurpleSpiceEditor::setupKnob (juce::Slider& knob, double defaultValue)
{
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setRange (0.0, 1.0, 0.01);
    knob.setDoubleClickReturnValue (true, defaultValue);
    knob.onValueChange = [this] { pushSettings(); };
    addAndMakeVisible (knob);
}

void PurpleSpiceEditor::setHint (const juce::String& text)
{
    hint = text;
    repaint (legendArea);
}

//==============================================================================
void PurpleSpiceEditor::pushSettings()
{
    auto s = proc.getSettings();
    const int oldStyle = s.style;

    s.style    = styleBox.getSelectedId() - 1;
    s.bars     = barChoices[juce::jlimit (0, 3, barsBox.getSelectedId() - 1)];
    s.grid     = gridBox.getSelectedId() - 1;
    s.kit      = kitBox.getSelectedId() - 1;
    s.rootNote = rootChoices[juce::jlimit (0, 3, rootBox.getSelectedId() - 1)];
    s.rolls    = (float) rollsKnob.getValue();
    s.spice    = (float) spiceKnob.getValue();
    s.open     = (float) openKnob.getValue();
    s.dynamics = (float) dynKnob.getValue();
    s.swing    = (float) swingKnob.getValue();
    s.humanize = (float) humanKnob.getValue();
    s.level    = (float) levelKnob.getValue();
    s.pitch    = pitchChip.getState();
    s.triplets = tripletChip.getState();
    s.gaps     = gapChip.getState();
    s.flams    = flamChip.getState();
    s.hostSync = syncChip.getState();

    proc.updateSettings (s);

    if (s.style != oldStyle)
        setHint (T8 (spice::styleName (s.style)) + T8 (": Raster, Betonungen und Swing passen jetzt zum Stil."));

    refresh();
}

void PurpleSpiceEditor::refresh()
{
    const auto s = proc.getSettings();
    const auto view = proc.getView();
    seenUiVersion = proc.getUiVersion();

    const auto dont = juce::dontSendNotification;
    styleBox.setSelectedId (s.style + 1, dont);
    gridBox.setSelectedId (s.grid + 1, dont);
    kitBox.setSelectedId (s.kit + 1, dont);
    for (int i = 0; i < 4; ++i)
    {
        if (barChoices[i] == s.bars)       barsBox.setSelectedId (i + 1, dont);
        if (rootChoices[i] == s.rootNote)  rootBox.setSelectedId (i + 1, dont);
    }

    rollsKnob.setValue (s.rolls, dont);
    spiceKnob.setValue (s.spice, dont);
    openKnob.setValue (s.open, dont);
    dynKnob.setValue (s.dynamics, dont);
    swingKnob.setValue (s.swing, dont);
    humanKnob.setValue (s.humanize, dont);
    levelKnob.setValue (s.level, dont);

    pitchChip.setState (s.pitch);
    tripletChip.setState (s.triplets);
    gapChip.setState (s.gaps);
    flamChip.setState (s.flams);
    syncChip.setState (s.hostSync);

    pattern.setPattern (view.events, view.locked);

    int closedCount = 0, openCount = 0;
    for (const auto& e : view.events)
        (e.kind == spice::kOpen ? openCount : closedCount)++;

    hatsTile.setTexts ("Hi-Hats ziehen", juce::String (closedCount) + T8 (closedCount == 1 ? " Note" : " Noten inkl. Rolls"));
    openTile.setTexts ("Open Hats ziehen", openCount > 0 ? juce::String (openCount) + T8 (openCount == 1 ? " Note" : " Noten")
                                                         : T8 ("keine im Pattern"));

    statsText = juce::String (view.stats.hits) + T8 (" Schläge  \xc2\xb7  ")
              + juce::String (view.stats.rolls) + T8 (view.stats.rolls == 1 ? " Roll  \xc2\xb7  " : " Rolls  \xc2\xb7  ")
              + juce::String (view.stats.opens) + T8 (view.stats.opens == 1 ? " Open Hat" : " Open Hats");

    prevButton.setEnabled (proc.canStepHistory (-1));
    nextButton.setEnabled (proc.canStepHistory (1));
    repaint();
}

void PurpleSpiceEditor::timerCallback()
{
    proc.syncTempo();                       // Pattern folgt dem Tempo des Projekts

    if (proc.getUiVersion() != seenUiVersion)
        refresh();

    pattern.setPlayhead (proc.getPlayPosition());

    const bool playing = proc.isPlaying();
    if (playing != wasPlaying)
    {
        wasPlaying = playing;
        playButton.setIcon (playing ? PillButton::Icon::stop : PillButton::Icon::play);
        playButton.setText (playing ? "Stop" : "Play");
    }

    const int bpm = juce::roundToInt (proc.getBpm());
    const bool host = proc.hasHostTempo();
    if (bpm != shownBpm || host != shownHostTempo)
    {
        shownBpm = bpm;
        shownHostTempo = host;
        repaint (tempoArea);
    }
}

//==============================================================================
void PurpleSpiceEditor::resized()
{
    captions.clear();
    auto area = getLocalBounds().reduced (14);
    const auto left = juce::Justification::centredLeft;
    const auto centre = juce::Justification::centred;

    // ---- Kopfzeile ---------------------------------------------------------------
    auto header = area.removeFromTop (66);
    {
        auto right = header.removeFromRight (518).withTrimmedTop (8);
        tempoArea = right.removeFromLeft (118);
        right.removeFromLeft (10);

        auto place = [&] (juce::ComboBox& box, int width, const char* caption)
        {
            auto col = right.removeFromLeft (width);
            captions.push_back ({ col.removeFromTop (17), T8 (caption), left });
            box.setBounds (col.removeFromTop (32));
            right.removeFromLeft (10);
        };
        place (styleBox, 160, "STIL");
        place (barsBox, 80, "TAKTE");
        place (gridBox, 130, "RASTER");
    }
    area.removeFromTop (10);

    // ---- Fusszeile -----------------------------------------------------------------
    auto footer = area.removeFromBottom (58);
    area.removeFromBottom (12);
    generateButton.setBounds (footer.removeFromLeft (230));  footer.removeFromLeft (12);
    varyButton.setBounds (footer.removeFromLeft (150));      footer.removeFromLeft (10);
    prevButton.setBounds (footer.removeFromLeft (46));       footer.removeFromLeft (6);
    nextButton.setBounds (footer.removeFromLeft (46));       footer.removeFromLeft (10);
    playButton.setBounds (footer.removeFromLeft (118));      footer.removeFromLeft (12);
    hatsTile.setBounds (footer.removeFromLeft ((footer.getWidth() - 10) / 2));
    footer.removeFromLeft (10);
    openTile.setBounds (footer);

    // ---- Regler-Panel ----------------------------------------------------------------
    controlsPanel = area.removeFromBottom (152);
    area.removeFromBottom (8);
    legendArea = area.removeFromBottom (20);
    area.removeFromBottom (8);
    pattern.setBounds (area);

    auto inner = controlsPanel.reduced (14, 10);

    auto combos = inner.removeFromRight (150);
    inner.removeFromRight (14);
    {
        combos = combos.withSizeKeepingCentre (combos.getWidth(), 2 * 47 + 8);
        auto top = combos.removeFromTop (47);
        captions.push_back ({ top.removeFromTop (16), T8 ("SOUND"), left });
        kitBox.setBounds (top.removeFromTop (30));
        combos.removeFromTop (8);
        captions.push_back ({ combos.removeFromTop (16), T8 ("GRUNDTON (MIDI)"), left });
        rootBox.setBounds (combos.removeFromTop (30));
    }

    auto chips = inner.removeFromRight (290);
    inner.removeFromRight (14);
    {
        const int gap = 8, h = 34;
        chips = chips.withSizeKeepingCentre (chips.getWidth(), 3 * h + 2 * gap);
        ToggleChip* grid[3][2] = { { &pitchChip, &tripletChip }, { &gapChip, &flamChip }, { &syncChip, nullptr } };
        const int w = (chips.getWidth() - gap) / 2;
        for (int r = 0; r < 3; ++r)
        {
            auto row = chips.removeFromTop (h);
            chips.removeFromTop (gap);
            if (grid[r][0] != nullptr) grid[r][0]->setBounds (row.removeFromLeft (w));
            row.removeFromLeft (gap);
            if (grid[r][1] != nullptr) grid[r][1]->setBounds (row);
        }
    }

    {
        juce::Slider* knobs[7] = { &rollsKnob, &spiceKnob, &openKnob, &dynKnob, &swingKnob, &humanKnob, &levelKnob };
        const char* names[7] = { "ROLLS", "SPICE", "OPEN HATS", "DYNAMIK", "SWING", "HUMANIZE", "PEGEL" };
        const int w = inner.getWidth() / 7;
        for (int i = 0; i < 7; ++i)
        {
            auto col = inner.removeFromLeft (w);
            captions.push_back ({ col.removeFromBottom (16), T8 (names[i]), centre });
            knobs[i]->setBounds (col.reduced (3, 6));
        }
    }
}

void PurpleSpiceEditor::paint (juce::Graphics& g)
{
    const float width = (float) getWidth();

    g.fillAll (theme::bg);
    g.setGradientFill (juce::ColourGradient (theme::bgHi, 0.0f, 0.0f, theme::bg, 0.0f, (float) getHeight() * 0.5f, false));
    g.fillRect (getLocalBounds());

    // Lila laeuft von der Oberkante: lange Tropfen in der freien Mitte, kurze ueber Titel und Auswahl
    {
        auto drips = theme::makeDrips (8.0f, 338.0f, 77u, 3.0f, 7.0f, 4.0f, 7.0f, 16.0f, 50.0f);
        const auto mid   = theme::makeDrips (352.0f, 574.0f, 4321u, 9.0f, 52.0f, 6.0f, 11.0f, 5.0f, 24.0f);
        const auto right = theme::makeDrips (590.0f, width - 8.0f, 808u, 3.0f, 7.0f, 4.0f, 7.0f, 16.0f, 54.0f);
        drips.insert (drips.end(), mid.begin(), mid.end());
        drips.insert (drips.end(), right.begin(), right.end());
        theme::drawDrips (g, 0.0f, width, 0.0f, 6.0f, drips, theme::purple);

        theme::drawDroplet (g, 421.0f, 70.0f, 5.0f, theme::purple.withAlpha (0.8f));
        theme::drawDroplet (g, 506.0f, 61.0f, 3.6f, theme::purple.withAlpha (0.55f));
    }

    // Schriftzug in Chrom-Lila, darunter eine tropfende Linie
    {
        const auto textBounds = theme::drawChromeText (g, "Purple Spice", 20.0f, 55.0f, 46.0f);
        const float l = textBounds.getX() + 2.0f, r = textBounds.getRight() - 6.0f, y = 62.0f;
        const auto drips = theme::makeDrips (l + 10.0f, r - 6.0f, 2024u, 6.0f, 15.0f, 4.0f, 6.5f, 18.0f, 60.0f);
        theme::drawDrips (g, l, r, y, y + 3.0f, drips, theme::purpleHi);
        theme::drawSparkle (g, { textBounds.getRight() - 4.0f, textBounds.getY() - 9.0f, 18.0f, 18.0f }, juce::Colours::white);

        g.setColour (theme::textDim);
        g.setFont (theme::font (11.5f, true));
        g.drawText ("HI-HAT GENERATOR", juce::Rectangle<float> (textBounds.getRight() + 22.0f, 36.0f, 150.0f, 16.0f).toNearestInt(),
                    juce::Justification::centredLeft, false);
    }

    // Tempo-Anzeige
    {
        auto t = tempoArea;
        g.setColour (theme::textDim);
        g.setFont (theme::font (11.0f, true));
        g.drawText (shownHostTempo ? "TEMPO (HOST)" : "TEMPO", t.removeFromTop (17), juce::Justification::centredLeft, false);

        const auto pill = t.removeFromTop (32).toFloat().reduced (0.75f);
        g.setGradientFill (juce::ColourGradient (theme::purpleDark, 0.0f, pill.getY(), theme::panelHi, 0.0f, pill.getBottom(), false));
        g.fillRoundedRectangle (pill, 9.0f);
        g.setColour (theme::purple);
        g.drawRoundedRectangle (pill, 9.0f, 1.2f);
        g.setColour (theme::text);
        g.setFont (theme::font (16.0f, true));
        g.drawText (juce::String (juce::jmax (0, shownBpm)) + " BPM", pill.toNearestInt(), juce::Justification::centred, false);
    }

    // Regler-Panel
    g.setColour (theme::panel);
    g.fillRoundedRectangle (controlsPanel.toFloat(), 16.0f);
    g.setColour (theme::line.withAlpha (0.6f));
    g.drawRoundedRectangle (controlsPanel.toFloat().reduced (0.5f), 16.0f, 1.0f);

    // Beschriftungen
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.0f, true));
    for (const auto& c : captions)
        g.drawText (c.text, c.area, c.just, false);

    // Legende, Hinweis und Zahlen
    {
        auto row = legendArea;
        const char* names[5] = { "Hi-Hat", "Roll", "Open Hat", "Ghost", "Flam" };
        const int kinds[5] = { spice::kHit, spice::kRoll, spice::kOpen, spice::kGhost, spice::kFlam };
        const int widths[5] = { 62, 48, 78, 58, 52 };

        g.setFont (theme::font (12.0f));
        for (int i = 0; i < 5; ++i)
        {
            auto cell = row.removeFromLeft (widths[i]);
            g.setColour (theme::kindColour (kinds[i]));
            g.fillEllipse ((float) cell.getX(), (float) cell.getCentreY() - 4.5f, 9.0f, 9.0f);
            g.setColour (theme::textDim);
            g.drawText (names[i], cell.withTrimmedLeft (14), juce::Justification::centredLeft, false);
        }

        g.setColour (theme::text.withAlpha (0.9f));
        g.setFont (theme::font (12.0f, true));
        g.drawText (statsText, row.removeFromRight (250), juce::Justification::centredRight, false);

        g.setColour (theme::purpleHi);
        g.setFont (theme::font (12.0f));
        g.drawFittedText (hint, row.reduced (10, 0), juce::Justification::centred, 1, 0.8f);
    }
}
