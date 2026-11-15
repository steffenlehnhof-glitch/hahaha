#include "PluginEditor.h"

using theme::T8;

namespace
{
const int barChoices[4] = { 1, 2, 4, 8 };
}

//==============================================================================
Purple808Editor::Purple808Editor (Purple808Processor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    // ---- Auswahlboxen ----------------------------------------------------------
    juce::StringArray styles, scales;
    for (int i = 0; i < p808::kNumStyles; ++i)  styles.add (T8 (p808::styleName (i)));
    for (int i = 0; i < p808::kNumScales; ++i)  scales.add (T8 (p808::scaleName (i)));

    setupCombo (styleBox, styles);
    setupCombo (barsBox, { "1", "2", "4", "8" });
    setupCombo (keyBox, { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" });
    setupCombo (scaleBox, scales);
    setupCombo (octaveBox, { "tief", "normal", "hoch" });

    // ---- Regler ------------------------------------------------------------------
    setupKnob (densityKnob, 0.5);
    setupKnob (slidesKnob, 0.5);
    setupKnob (spiceKnob, 0.5);
    setupKnob (moveKnob, 0.5);
    setupKnob (gateKnob, 0.85);
    setupKnob (swingKnob, 0.0);

    setupKnob (distKnob, 0.3);
    setupKnob (toneKnob, 0.6);
    setupKnob (punchKnob, 0.5);
    setupKnob (decayKnob, 0.6);
    setupKnob (glideKnob, 0.4);
    setupKnob (subKnob, 0.4);
    setupKnob (reverbKnob, 0.1);
    setupKnob (levelKnob, 0.8);

    stutterChip.setText ("Stotter");
    octaveChip.setText ("Oktaven");
    tripletChip.setText ("Triolen");
    gapChip.setText ("Pausen");
    syncChip.setText ("Host-Sync");
    for (auto* chip : { &stutterChip, &octaveChip, &tripletChip, &gapChip, &syncChip })
    {
        chip->onChange = [this] { pushSettings(); };
        addAndMakeVisible (*chip);
    }

    // ---- 808-Auswahl ---------------------------------------------------------------
    kitGrid.onSelect = [this] (int kit)
    {
        auto s = proc.getSettings();
        s.kit = kit;
        proc.updateSettings (s);
        proc.audition();
        setHint (T8 (p808::kitName (kit)) + T8 (" geladen. Mit den Reglern rechts formst du den Sound weiter."));
        refresh();
    };
    addAndMakeVisible (kitGrid);

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
        setHint (T8 ("Gleiche Grundtöne, neue Slides und Fills in der zweiten Takthälfte."));
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

    midiTile.setTexts ("808 MIDI ziehen", "in die Piano Roll deiner 808");
    midiTile.fileProvider = [this] { return proc.writeTempMidi(); };
    midiTile.onHint       = [this] { setHint (T8 ("Feld festhalten und in die Piano Roll ziehen. Für Slides im Ziel-Kanal Mono + Porta einschalten.")); };
    midiTile.onDragged    = [this] { setHint (T8 ("MIDI abgeliefert. Für Slides im Ziel-Kanal Mono + Porta einschalten.")); };
    addAndMakeVisible (midiTile);

    hint = T8 ("Klick auf eine 808 spielt sie an. Schloss hält einen Takt fest, der Würfel würfelt ihn neu.");

    setResizable (false, false);
    setSize (1180, 690);
    refresh();
    startTimerHz (30);
}

Purple808Editor::~Purple808Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void Purple808Editor::setupCombo (juce::ComboBox& box, const juce::StringArray& items)
{
    box.addItemList (items, 1);
    box.onChange = [this] { pushSettings(); };
    addAndMakeVisible (box);
}

void Purple808Editor::setupKnob (juce::Slider& knob, double defaultValue)
{
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setRange (0.0, 1.0, 0.01);
    knob.setDoubleClickReturnValue (true, defaultValue);
    knob.onValueChange = [this] { pushSettings(); };
    addAndMakeVisible (knob);
}

void Purple808Editor::setHint (const juce::String& text)
{
    hint = text;
    repaint (legendArea);
}

//==============================================================================
void Purple808Editor::pushSettings()
{
    auto s = proc.getSettings();
    const int oldStyle = s.style;

    s.style    = styleBox.getSelectedId() - 1;
    s.bars     = barChoices[juce::jlimit (0, 3, barsBox.getSelectedId() - 1)];
    s.key      = keyBox.getSelectedId() - 1;
    s.scale    = scaleBox.getSelectedId() - 1;
    s.octave   = octaveBox.getSelectedId() - 2;

    s.density  = (float) densityKnob.getValue();
    s.slides   = (float) slidesKnob.getValue();
    s.spice    = (float) spiceKnob.getValue();
    s.movement = (float) moveKnob.getValue();
    s.gate     = (float) gateKnob.getValue();
    s.swing    = (float) swingKnob.getValue();

    s.stutter  = stutterChip.getState();
    s.octaves  = octaveChip.getState();
    s.triplets = tripletChip.getState();
    s.gaps     = gapChip.getState();
    s.hostSync = syncChip.getState();

    s.distortion = (float) distKnob.getValue();
    s.tone       = (float) toneKnob.getValue();
    s.punch      = (float) punchKnob.getValue();
    s.decay      = (float) decayKnob.getValue();
    s.glide      = (float) glideKnob.getValue();
    s.sub        = (float) subKnob.getValue();
    s.reverb     = (float) reverbKnob.getValue();
    s.level      = (float) levelKnob.getValue();

    proc.updateSettings (s);

    if (s.style != oldStyle)
        setHint (T8 (p808::styleName (s.style)) + T8 (": Rhythmus und Swing passen jetzt zum Stil."));

    refresh();
}

void Purple808Editor::refresh()
{
    const auto s = proc.getSettings();
    const auto view = proc.getView();
    seenUiVersion = proc.getUiVersion();

    const auto dont = juce::dontSendNotification;
    styleBox.setSelectedId (s.style + 1, dont);
    keyBox.setSelectedId (s.key + 1, dont);
    scaleBox.setSelectedId (s.scale + 1, dont);
    octaveBox.setSelectedId (s.octave + 2, dont);
    for (int i = 0; i < 4; ++i)
        if (barChoices[i] == s.bars)
            barsBox.setSelectedId (i + 1, dont);

    densityKnob.setValue (s.density, dont);
    slidesKnob.setValue (s.slides, dont);
    spiceKnob.setValue (s.spice, dont);
    moveKnob.setValue (s.movement, dont);
    gateKnob.setValue (s.gate, dont);
    swingKnob.setValue (s.swing, dont);

    distKnob.setValue (s.distortion, dont);
    toneKnob.setValue (s.tone, dont);
    punchKnob.setValue (s.punch, dont);
    decayKnob.setValue (s.decay, dont);
    glideKnob.setValue (s.glide, dont);
    subKnob.setValue (s.sub, dont);
    reverbKnob.setValue (s.reverb, dont);
    levelKnob.setValue (s.level, dont);

    stutterChip.setState (s.stutter);
    octaveChip.setState (s.octaves);
    tripletChip.setState (s.triplets);
    gapChip.setState (s.gaps);
    syncChip.setState (s.hostSync);

    kitGrid.setSelected (s.kit);
    pattern.setPattern (view.events, view.locked, view.barLabels, view.rootPitchClass);

    statsText = juce::String (view.stats.notes) + T8 (view.stats.notes == 1 ? " Note  \xc2\xb7  " : " Noten  \xc2\xb7  ")
              + juce::String (view.stats.slides) + T8 (view.stats.slides == 1 ? " Slide" : " Slides");

    prevButton.setEnabled (proc.canStepHistory (-1));
    nextButton.setEnabled (proc.canStepHistory (1));
    repaint();
}

void Purple808Editor::timerCallback()
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
void Purple808Editor::resized()
{
    captions.clear();
    auto area = getLocalBounds().reduced (14);
    const auto left = juce::Justification::centredLeft;
    const auto centre = juce::Justification::centred;

    // ---- Kopfzeile ---------------------------------------------------------------
    auto header = area.removeFromTop (66);
    {
        auto right = header.removeFromRight (668).withTrimmedTop (8);
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
        place (barsBox, 76, "TAKTE");
        place (keyBox, 84, "TONART");
        place (scaleBox, 170, "SKALA");
    }
    area.removeFromTop (10);

    // ---- Fusszeile -----------------------------------------------------------------
    auto footer = area.removeFromBottom (58);
    area.removeFromBottom (12);
    generateButton.setBounds (footer.removeFromLeft (240));  footer.removeFromLeft (12);
    varyButton.setBounds (footer.removeFromLeft (160));      footer.removeFromLeft (10);
    prevButton.setBounds (footer.removeFromLeft (46));       footer.removeFromLeft (6);
    nextButton.setBounds (footer.removeFromLeft (46));       footer.removeFromLeft (10);
    playButton.setBounds (footer.removeFromLeft (124));      footer.removeFromLeft (12);
    midiTile.setBounds (footer);

    // ---- zwei Panels: Pattern links, Sound rechts --------------------------------------
    auto panels = area.removeFromBottom (222);
    area.removeFromBottom (8);
    legendArea = area.removeFromBottom (20);
    area.removeFromBottom (8);
    pattern.setBounds (area);

    patternPanel = panels.removeFromLeft (430);
    panels.removeFromLeft (12);
    soundPanel = panels;

    auto knobRow = [&] (juce::Rectangle<int> row, juce::Slider** knobs, const char* const* names, int count)
    {
        const int w = row.getWidth() / count;
        for (int i = 0; i < count; ++i)
        {
            auto col = row.removeFromLeft (w);
            captions.push_back ({ col.removeFromBottom (15), T8 (names[i]), centre });
            knobs[i]->setBounds (col.reduced (2, 2));
        }
    };

    // Pattern-Panel
    {
        auto inner = patternPanel.reduced (14, 10);
        captions.push_back ({ inner.removeFromTop (18), T8 ("PATTERN"), left });

        auto chips = inner.removeFromRight (124);
        inner.removeFromRight (10);
        {
            const int gap = 6, h = (chips.getHeight() - 4 * gap) / 5;
            for (auto* chip : { &stutterChip, &octaveChip, &tripletChip, &gapChip, &syncChip })
            {
                chip->setBounds (chips.removeFromTop (h));
                chips.removeFromTop (gap);
            }
        }

        const int rowH = inner.getHeight() / 2;
        juce::Slider* top[3] = { &densityKnob, &slidesKnob, &spiceKnob };
        const char* topNames[3] = { "DICHTE", "SLIDES", "SPICE" };
        juce::Slider* bottom[3] = { &moveKnob, &gateKnob, &swingKnob };
        const char* bottomNames[3] = { "BEWEGUNG", "LÄNGE", "SWING" };
        knobRow (inner.removeFromTop (rowH), top, topNames, 3);
        knobRow (inner, bottom, bottomNames, 3);
    }

    // Sound-Panel
    {
        auto inner = soundPanel.reduced (14, 10);
        auto title = inner.removeFromTop (18);
        captions.push_back ({ title, T8 ("808 AUSWÄHLEN"), left });

        auto gridCol = inner.removeFromLeft (372);
        inner.removeFromLeft (14);
        captions.push_back ({ title.withX (inner.getX()).withWidth (inner.getWidth()), T8 ("SOUND FORMEN"), left });

        auto octaveRow = gridCol.removeFromBottom (30);
        gridCol.removeFromBottom (8);
        kitGrid.setBounds (gridCol);
        captions.push_back ({ octaveRow.removeFromLeft (66), T8 ("OKTAVE"), left });
        octaveBox.setBounds (octaveRow.removeFromLeft (120));

        const int rowH = inner.getHeight() / 2;
        juce::Slider* top[4] = { &distKnob, &toneKnob, &punchKnob, &decayKnob };
        const char* topNames[4] = { "DISTORTION", "TONE", "PUNCH", "DECAY" };
        juce::Slider* bottom[4] = { &glideKnob, &subKnob, &reverbKnob, &levelKnob };
        const char* bottomNames[4] = { "GLIDE", "SUB", "HALL", "PEGEL" };
        knobRow (inner.removeFromTop (rowH), top, topNames, 4);
        knobRow (inner, bottom, bottomNames, 4);
    }
}

void Purple808Editor::paint (juce::Graphics& g)
{
    const float width = (float) getWidth();

    g.fillAll (theme::bg);
    g.setGradientFill (juce::ColourGradient (theme::bgHi, 0.0f, 0.0f, theme::bg, 0.0f, (float) getHeight() * 0.5f, false));
    g.fillRect (getLocalBounds());

    // Lila laeuft von der Oberkante: lange Tropfen in der freien Mitte, kurze ueber Titel und Auswahl
    {
        auto drips = theme::makeDrips (8.0f, 376.0f, 808u, 3.0f, 7.0f, 4.0f, 7.0f, 16.0f, 50.0f);
        const auto mid   = theme::makeDrips (388.0f, 486.0f, 1808u, 9.0f, 52.0f, 6.0f, 11.0f, 4.0f, 16.0f);
        const auto right = theme::makeDrips (500.0f, width - 8.0f, 2808u, 3.0f, 7.0f, 4.0f, 7.0f, 16.0f, 54.0f);
        drips.insert (drips.end(), mid.begin(), mid.end());
        drips.insert (drips.end(), right.begin(), right.end());
        theme::drawDrips (g, 0.0f, width, 0.0f, 6.0f, drips, theme::purple);

        theme::drawDroplet (g, 426.0f, 69.0f, 5.0f, theme::purple.withAlpha (0.8f));
        theme::drawDroplet (g, 462.0f, 60.0f, 3.6f, theme::purple.withAlpha (0.55f));
    }

    // Schriftzug in Chrom-Lila, darunter eine tropfende Linie
    {
        const auto textBounds = theme::drawChromeText (g, "Purple 808", 20.0f, 55.0f, 46.0f);
        const float l = textBounds.getX() + 2.0f, r = textBounds.getRight() - 6.0f, y = 62.0f;
        const auto drips = theme::makeDrips (l + 10.0f, r - 6.0f, 8080u, 6.0f, 15.0f, 4.0f, 6.5f, 18.0f, 60.0f);
        theme::drawDrips (g, l, r, y, y + 3.0f, drips, theme::purpleHi);
        theme::drawSparkle (g, { textBounds.getRight() - 4.0f, textBounds.getY() - 9.0f, 18.0f, 18.0f }, juce::Colours::white);

        g.setColour (theme::textDim);
        g.setFont (theme::font (11.5f, true));
        g.drawText ("808 GENERATOR", juce::Rectangle<float> (textBounds.getRight() + 22.0f, 36.0f, 130.0f, 16.0f).toNearestInt(),
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

    // Panels
    for (const auto& panel : { patternPanel, soundPanel })
    {
        g.setColour (theme::panel);
        g.fillRoundedRectangle (panel.toFloat(), 16.0f);
        g.setColour (theme::line.withAlpha (0.6f));
        g.drawRoundedRectangle (panel.toFloat().reduced (0.5f), 16.0f, 1.0f);
    }

    // Beschriftungen
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.0f, true));
    for (const auto& c : captions)
        g.drawText (c.text, c.area, c.just, false);

    // Legende, Hinweis und Zahlen
    {
        auto row = legendArea;
        const char* names[4] = { "Note", "Slide", "Stotter", "Ghost" };
        const int kinds[4] = { p808::kNote, p808::kSlide, p808::kStutter, p808::kGhost };
        const int widths[4] = { 54, 56, 68, 58 };

        g.setFont (theme::font (12.0f));
        for (int i = 0; i < 4; ++i)
        {
            auto cell = row.removeFromLeft (widths[i]);
            g.setColour (theme::kindColour (kinds[i]));
            g.fillEllipse ((float) cell.getX(), (float) cell.getCentreY() - 4.5f, 9.0f, 9.0f);
            g.setColour (theme::textDim);
            g.drawText (names[i], cell.withTrimmedLeft (14), juce::Justification::centredLeft, false);
        }

        g.setColour (theme::text.withAlpha (0.9f));
        g.setFont (theme::font (12.0f, true));
        g.drawText (statsText, row.removeFromRight (190), juce::Justification::centredRight, false);

        g.setColour (theme::purpleHi);
        g.setFont (theme::font (12.0f));
        g.drawFittedText (hint, row.reduced (10, 0), juce::Justification::centred, 1, 0.8f);
    }
}
