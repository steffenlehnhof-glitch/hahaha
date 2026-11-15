#include "PluginEditor.h"

using theme::T8;

//==============================================================================
PurrgressionEditor::PurrgressionEditor (PurrgressionProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    // ---- Auswahlboxen ----------------------------------------------------------
    setupCombo (keyBox, { "C", "C# / Db", "D", "D# / Eb", "E", "F", "F# / Gb", "G", "G# / Ab", "A", "A# / Bb", "B" });

    juce::StringArray scales, styles, voicings, patterns;
    for (int i = 0; i < purr::kNumScales; ++i)    scales.add (T8 (purr::scaleDef (i).name));
    for (int i = 0; i < purr::kNumStyles; ++i)    styles.add (T8 (purr::styleName (i)));
    for (int i = 0; i < purr::kNumVoicings; ++i)  voicings.add (T8 (purr::voicingName (i)));
    for (int i = 0; i < purr::kNumPatterns; ++i)  patterns.add (T8 (purr::patternName (i)));

    setupCombo (scaleBox, scales);
    setupCombo (styleBox, styles);
    setupCombo (countBox, { "2", "3", "4", "5", "6", "7", "8" });
    setupCombo (durationBox, { "1/2 Takt", "1 Takt", "2 Takte" });
    setupCombo (voicingBox, voicings);
    setupCombo (patternBox, patterns);
    setupCombo (soundBox, { "Aus", "Keys", "Pad", "Pluck" });
    setupCombo (octaveBox, { "-1", "0", "+1" });

    // ---- Regler + Schalter -----------------------------------------------------
    setupKnob (colourKnob, 0.35);
    setupKnob (spiceKnob, 0.3);
    setupKnob (humanKnob, 0.2);
    setupKnob (swingKnob, 0.0);

    bassChip.setText ("Bass");
    leadChip.setText ("Voice Leading");
    syncChip.setText ("Host-Sync");
    for (auto* chip : { &bassChip, &leadChip, &syncChip })
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
        cat.excite();
        sayForStyle();
        refresh();
    };

    varyButton.setText ("Variation");
    varyButton.setIcon (PillButton::Icon::sparkle);
    varyButton.onClick = [this]
    {
        proc.vary();
        say ({ "Gleiche Idee, neues Fell.", "Hab einen Akkord umgeschubst.", "Kleine Krallenspur hinterlassen.",
               "Nur ein bisschen anders. Hörst du's?" });
        refresh();
    };

    prevButton.setIcon (PillButton::Icon::prev);
    prevButton.onClick = [this]
    {
        if (proc.stepHistory (-1)) { say ({ "Zurückgespult.", "Die davor war auch nicht schlecht." }); refresh(); }
    };

    nextButton.setIcon (PillButton::Icon::next);
    nextButton.onClick = [this]
    {
        if (proc.stepHistory (1)) { say ({ "Und wieder vor." }); refresh(); }
    };

    playButton.setText ("Play");
    playButton.setIcon (PillButton::Icon::play);
    playButton.onClick = [this]
    {
        const bool start = ! proc.isPlaying();
        proc.setPlaying (start);
        if (start) say ({ "Lautstärke hoch!", "Schnurr ... läuft.", "Kopfhörer sitzen." });
        else       say ({ "Pause. Kurz Pfoten putzen.", "Stille. Auch schön." });
    };

    for (auto* b : { &generateButton, &varyButton, &prevButton, &nextButton, &playButton })
        addAndMakeVisible (*b);

    // ---- Akkordkarten ------------------------------------------------------------
    for (int i = 0; i < purr::kMaxChords; ++i)
    {
        auto& card = cards[(size_t) i];
        addChildComponent (card);

        card.onSelect = [this, i]
        {
            selected = i;
            proc.audition (i);
            for (int k = 0; k < purr::kMaxChords; ++k)
                cards[(size_t) k].setSelected (k == selected);
            updateKeyboard();
        };

        card.onLock = [this, i]
        {
            if (proc.toggleLock (i)) say ({ "Der bleibt. Versprochen.", "Festgekrallt." });
            else                     say ({ "Okay, der darf wieder wandern." });
            refresh();
        };

        card.onReroll = [this, i]
        {
            if (i < (int) views.size() && views[(size_t) i].locked)
            {
                say ({ "Der ist abgeschlossen. Erst das Schloss öffnen." });
                return;
            }
            proc.rerollChord (i);
            selected = i;
            say ({ "Neuer Wurf!", "Wie wär's damit?", "Den hab ich ausgetauscht." });
            refresh();
        };
    }

    addAndMakeVisible (cat);
    addAndMakeVisible (keyboard);

    dragTile.fileProvider = [this] { return proc.writeTempMidi(); };
    dragTile.onHint       = [this] { say ({ "Festhalten und in die Piano Roll oder Playlist ziehen. Doppelklick zeigt die Datei." }); };
    dragTile.onDragged    = [this] { say ({ "Abgeliefert. Viel Spaß damit!", "MIDI ist unterwegs." }); };
    addAndMakeVisible (dragTile);

    cat.say (T8 ("Miau, ich bin Moll! Drück auf Generieren und ich schnurr dir eine Akkordfolge vor."));

    setResizable (false, false);
    setSize (980, 620);
    refresh();
    startTimerHz (30);
}

PurrgressionEditor::~PurrgressionEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PurrgressionEditor::setupCombo (juce::ComboBox& box, const juce::StringArray& items)
{
    box.addItemList (items, 1);
    box.onChange = [this] { pushSettings(); };
    addAndMakeVisible (box);
}

void PurrgressionEditor::setupKnob (juce::Slider& knob, double defaultValue)
{
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setRange (0.0, 1.0, 0.01);
    knob.setDoubleClickReturnValue (true, defaultValue);
    knob.onValueChange = [this] { pushSettings(); };
    addAndMakeVisible (knob);
}

//==============================================================================
void PurrgressionEditor::say (std::initializer_list<const char*> options)
{
    if (options.size() == 0)
        return;
    const auto* chosen = *(options.begin() + random.nextInt ((int) options.size()));
    cat.say (T8 (chosen));
}

void PurrgressionEditor::sayForStyle()
{
    switch (proc.getSettings().style)
    {
        case purr::kTrap:   say ({ "Düster. Gefällt mir.", "Das schreit nach einer 808.", "Mitternachts-Vibes." }); break;
        case purr::kHipHop: say ({ "Kopfnicker-Material.", "Staubig wie eine alte Platte. Nice.", "Da gehört ein Drum-Break drunter." }); break;
        case purr::kLoFi:   say ({ "Schön flauschig.", "Dazu ein Nickerchen auf der Fensterbank.", "Regen, Tee, diese Akkorde." }); break;
        case purr::kJazz:   say ({ "Uuuh, schicke Akkorde.", "Zwei-Fünf-Eins. Schnurr.", "Da wackeln die Schnurrhaare." }); break;
        default:            say ({ "Samtig. Wie mein Fell.", "Das hat Seele.", "Butterweich." }); break;
    }
}

//==============================================================================
void PurrgressionEditor::pushSettings()
{
    auto s = proc.getSettings();
    const float oldColour = s.colour, oldSpice = s.spice;

    s.key       = keyBox.getSelectedId() - 1;
    s.scale     = scaleBox.getSelectedId() - 1;
    s.style     = styleBox.getSelectedId() - 1;
    s.numChords = countBox.getSelectedId() + 1;
    s.duration  = durationBox.getSelectedId() - 1;
    s.voicing   = voicingBox.getSelectedId() - 1;
    s.pattern   = patternBox.getSelectedId() - 1;
    s.sound     = soundBox.getSelectedId() - 1;
    s.octave    = octaveBox.getSelectedId() - 2;
    s.colour    = (float) colourKnob.getValue();
    s.spice     = (float) spiceKnob.getValue();
    s.humanize  = (float) humanKnob.getValue();
    s.swing     = (float) swingKnob.getValue();
    s.bass      = bassChip.getState();
    s.voiceLead = leadChip.getState();
    s.hostSync  = syncChip.getState();

    using Result = PurrgressionProcessor::SettingsResult;
    switch (proc.updateSettings (s))
    {
        case Result::regenerated:
            cat.excite();
            sayForStyle();
            break;

        case Result::transposed:
            say ({ "Transponiert. Gleiche Folge, neue Tonart." });
            break;

        case Result::resized:
            say ({ "Platz angepasst.", "Länge geändert, der Rest bleibt." });
            break;

        case Result::revoiced:
        case Result::soundOnly:
            if (! hintedKnobs && (std::abs (s.colour - oldColour) > 1.0e-6f || std::abs (s.spice - oldSpice) > 1.0e-6f))
            {
                hintedKnobs = true;
                say ({ "Farbe und Würze wirken beim nächsten Generieren." });
            }
            break;
    }

    refresh();
}

void PurrgressionEditor::refresh()
{
    const auto s = proc.getSettings();
    views = proc.getChordViews();
    seenUiVersion = proc.getUiVersion();

    const auto dont = juce::dontSendNotification;
    keyBox.setSelectedId (s.key + 1, dont);
    scaleBox.setSelectedId (s.scale + 1, dont);
    styleBox.setSelectedId (s.style + 1, dont);
    countBox.setSelectedId (s.numChords - 1, dont);
    durationBox.setSelectedId (s.duration + 1, dont);
    voicingBox.setSelectedId (s.voicing + 1, dont);
    patternBox.setSelectedId (s.pattern + 1, dont);
    soundBox.setSelectedId (s.sound + 1, dont);
    octaveBox.setSelectedId (s.octave + 2, dont);

    colourKnob.setValue (s.colour, dont);
    spiceKnob.setValue (s.spice, dont);
    humanKnob.setValue (s.humanize, dont);
    swingKnob.setValue (s.swing, dont);

    bassChip.setState (s.bass);
    leadChip.setState (s.voiceLead);
    syncChip.setState (s.hostSync);

    accent = theme::styleAccent (s.style);
    for (auto* k : { &colourKnob, &spiceKnob, &humanKnob, &swingKnob })
        k->setColour (juce::Slider::rotarySliderFillColourId, accent);
    for (auto* c : { &bassChip, &leadChip, &syncChip })
        c->setAccent (accent);
    generateButton.setAccent (accent);
    cat.setAccent (accent);
    cat.setSubtitle (proc.getKeyLabel() + T8 (" \xc2\xb7 ") + T8 (purr::styleName (s.style)));

    numCards = (int) views.size();
    selected = juce::jlimit (0, juce::jmax (0, numCards - 1), selected);

    for (int i = 0; i < numCards; ++i)
    {
        const auto& v = views[(size_t) i];
        cards[(size_t) i].setData (v.name, v.roman, v.notes, v.locked, v.quality, i);
        cards[(size_t) i].setSelected (i == selected);
    }

    layoutCards();
    prevButton.setEnabled (proc.canStepHistory (-1));
    nextButton.setEnabled (proc.canStepHistory (1));
    updateKeyboard();
    repaint();
}

void PurrgressionEditor::updateKeyboard()
{
    const int idx = shownCurrent >= 0 ? shownCurrent : selected;
    if (idx >= 0 && idx < (int) views.size())
        keyboard.setNotes (views[(size_t) idx].midi, theme::qualityColour (views[(size_t) idx].quality));
    else
        keyboard.setNotes ({}, accent);
}

void PurrgressionEditor::timerCallback()
{
    if (proc.getUiVersion() != seenUiVersion)
        refresh();

    const int cur = proc.getCurrentChord();
    if (cur != shownCurrent)
    {
        shownCurrent = cur;
        for (int i = 0; i < purr::kMaxChords; ++i)
            cards[(size_t) i].setCurrent (i == cur);
        updateKeyboard();
    }

    const bool playing = proc.isPlaying();
    if (playing != wasPlaying)
    {
        wasPlaying = playing;
        playButton.setIcon (playing ? PillButton::Icon::stop : PillButton::Icon::play);
        playButton.setText (playing ? "Stop" : "Play");
    }

    cat.advance (1.0f / 30.0f, proc.isSounding(), proc.getBeatPhase());
}

//==============================================================================
void PurrgressionEditor::layoutCards()
{
    const int n = numCards;
    const int gap = 10;
    const int rows = n > 4 ? 2 : 1;
    const int perRow = rows == 1 ? juce::jmax (1, n) : (n + 1) / 2;
    const int h = (cardsArea.getHeight() - (rows - 1) * gap) / rows;
    const int w = juce::jmin (230, (cardsArea.getWidth() - (perRow - 1) * gap) / perRow);

    for (int i = 0; i < purr::kMaxChords; ++i)
    {
        auto& card = cards[(size_t) i];
        card.setVisible (i < n);
        if (i < n)
            card.setBounds (cardsArea.getX() + (i % perRow) * (w + gap), cardsArea.getY() + (i / perRow) * (h + gap), w, h);
    }
}

void PurrgressionEditor::resized()
{
    captions.clear();
    auto area = getLocalBounds().reduced (14);
    const auto left = juce::Justification::centredLeft;
    const auto centre = juce::Justification::centred;

    // ---- Kopfzeile: Tonart / Skala / Stil --------------------------------------
    auto header = area.removeFromTop (54);
    {
        auto right = header.removeFromRight (470);
        auto place = [&] (juce::ComboBox& box, int width, const char* caption)
        {
            auto col = right.removeFromLeft (width);
            captions.push_back ({ col.removeFromTop (17), T8 (caption), left });
            box.setBounds (col.removeFromTop (32));
            right.removeFromLeft (10);
        };
        place (keyBox, 110, "TONART");
        place (scaleBox, 180, "SKALA");
        place (styleBox, 160, "STIL");
    }
    area.removeFromTop (10);

    // ---- Fusszeile: Aktionen -----------------------------------------------------
    auto footer = area.removeFromBottom (58);
    area.removeFromBottom (12);
    generateButton.setBounds (footer.removeFromLeft (250));  footer.removeFromLeft (12);
    varyButton.setBounds (footer.removeFromLeft (160));      footer.removeFromLeft (10);
    prevButton.setBounds (footer.removeFromLeft (48));       footer.removeFromLeft (6);
    nextButton.setBounds (footer.removeFromLeft (48));       footer.removeFromLeft (10);
    playButton.setBounds (footer.removeFromLeft (132));      footer.removeFromLeft (12);
    dragTile.setBounds (footer);

    // ---- Mitte: Katze links, Akkorde + Regler rechts ---------------------------
    cat.setBounds (area.removeFromLeft (250));
    area.removeFromLeft (12);

    cardsArea = area.removeFromTop (196);
    area.removeFromTop (12);

    auto keyRow = area.removeFromTop (56);
    area.removeFromTop (12);
    legendArea = keyRow.removeFromRight (176);
    keyRow.removeFromRight (14);
    keyboard.setBounds (keyRow);

    controlsPanel = area;
    auto inner = controlsPanel.reduced (14, 12);

    auto rowA = inner.removeFromTop (50);
    {
        juce::ComboBox* boxes[6] = { &countBox, &durationBox, &voicingBox, &patternBox, &soundBox, &octaveBox };
        const char* names[6] = { "AKKORDE", "DAUER", "VOICING", "PATTERN", "SOUND", "OKTAVE" };
        const int weights[6] = { 8, 11, 11, 14, 9, 8 };
        const int gap = 10;
        const int usable = rowA.getWidth() - 5 * gap;
        for (int i = 0; i < 6; ++i)
        {
            auto col = rowA.removeFromLeft (i == 5 ? rowA.getWidth() : usable * weights[i] / 61);
            captions.push_back ({ col.removeFromTop (17), T8 (names[i]), left });
            boxes[i]->setBounds (col.removeFromTop (32));
            rowA.removeFromLeft (gap);
        }
    }
    inner.removeFromTop (10);

    auto chips = inner.removeFromRight (176);
    inner.removeFromRight (14);
    {
        const int gap = 7;
        const int h = (chips.getHeight() - 2 * gap) / 3;
        bassChip.setBounds (chips.removeFromTop (h));  chips.removeFromTop (gap);
        leadChip.setBounds (chips.removeFromTop (h));  chips.removeFromTop (gap);
        syncChip.setBounds (chips.removeFromTop (h));
    }

    {
        juce::Slider* knobs[4] = { &colourKnob, &spiceKnob, &humanKnob, &swingKnob };
        const char* names[4] = { "FARBE", "WÜRZE", "HUMANIZE", "SWING" };
        const int w = inner.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto col = inner.removeFromLeft (w);
            captions.push_back ({ col.removeFromBottom (16), T8 (names[i]), centre });
            knobs[i]->setBounds (col.reduced (4, 0));
        }
    }

    layoutCards();
}

void PurrgressionEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
    g.setGradientFill (juce::ColourGradient (theme::bgHi, 0.0f, 0.0f, theme::bg, 0.0f, (float) getHeight() * 0.7f, false));
    g.fillRect (getLocalBounds());

    // Logo + Titel
    const juce::Rectangle<float> mark (16.0f, 17.0f, 34.0f, 34.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (mark, 10.0f);
    theme::drawNote (g, mark.getCentreX() - 3.0f, mark.getCentreY() + 8.0f, 15.0f, theme::ink);

    g.setColour (theme::text);
    g.setFont (theme::font (27.0f, true));
    g.drawText ("Purrgression", 60, 12, 300, 30, juce::Justification::centredLeft, false);
    g.setColour (theme::textDim);
    g.setFont (theme::font (12.5f));
    g.drawText ("Akkordfolgen-Generator", 61, 41, 300, 16, juce::Justification::centredLeft, false);

    // Regler-Panel
    g.setColour (theme::panel);
    g.fillRoundedRectangle (controlsPanel.toFloat(), 16.0f);

    // Beschriftungen
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.0f, true));
    for (const auto& c : captions)
        g.drawText (c.text, c.area, c.just, false);

    // Legende der Kartenfarben
    {
        const char* names[4] = { "Moll", "Dur", "Dominante", "Spannung" };
        const int quals[4] = { purr::kQMinor, purr::kQMajor, purr::kQDominant, purr::kQTense };
        const int cw = legendArea.getWidth() / 2, ch = legendArea.getHeight() / 2;

        g.setFont (theme::font (12.0f));
        for (int i = 0; i < 4; ++i)
        {
            const juce::Rectangle<int> cell (legendArea.getX() + (i % 2) * cw, legendArea.getY() + (i / 2) * ch, cw, ch);
            g.setColour (theme::qualityColour (quals[i]));
            g.fillEllipse ((float) cell.getX(), (float) cell.getCentreY() - 4.5f, 9.0f, 9.0f);
            g.setColour (theme::textDim);
            g.drawText (names[i], cell.withTrimmedLeft (16), juce::Justification::centredLeft, false);
        }
    }
}
