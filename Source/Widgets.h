#pragma once

#include "Theme.h"

#include <functional>
#include <vector>

// =============================================================================
//  Look & Feel fuer Drehregler, Auswahlboxen und Popup-Menues
// =============================================================================
class PurrLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PurrLookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId, theme::panelHi);
        setColour (juce::ComboBox::outlineColourId, theme::line);
        setColour (juce::ComboBox::textColourId, theme::text);
        setColour (juce::ComboBox::arrowColourId, theme::textDim);
        setColour (juce::PopupMenu::backgroundColourId, theme::panelHi);
        setColour (juce::PopupMenu::textColourId, theme::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::purpleDeep);
        setColour (juce::PopupMenu::highlightedTextColourId, theme::text);
        setColour (juce::ScrollBar::thumbColourId, theme::line.brighter (0.35f));
        setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::rotarySliderFillColourId, theme::purple);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override   { return theme::font (14.5f); }
    juce::Font getPopupMenuFont() override                  { return theme::font (15.0f); }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto b = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.75f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId).brighter (box.isMouseOver (true) ? 0.1f : 0.0f));
        g.fillRoundedRectangle (b, 9.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (b, 9.0f, 1.2f);

        const float cx = (float) width - 16.0f, cy = (float) height * 0.5f;
        juce::Path arrow;
        arrow.startNewSubPath (cx - 4.5f, cy - 2.5f);
        arrow.lineTo (cx, cy + 2.5f);
        arrow.lineTo (cx + 4.5f, cy - 2.5f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.strokePath (arrow, theme::roundStroke (1.8f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
        const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto sq = bounds.withSizeKeepingCentre (size, size);
        const float cx = sq.getCentreX(), cy = sq.getCentreY(), r = size * 0.5f - 3.0f;
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

        juce::Path track;
        track.addCentredArc (cx, cy, r, r, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (theme::line);
        g.strokePath (track, theme::roundStroke (5.0f));

        if (sliderPos > 0.001f)
        {
            juce::Path value;
            value.addCentredArc (cx, cy, r, r, 0.0f, rotaryStartAngle, angle, true);
            g.setColour (accent);
            g.strokePath (value, theme::roundStroke (5.0f));
        }

        const float inner = r - 7.0f;
        g.setColour (theme::panelHi.brighter (slider.isMouseOverOrDragging() ? 0.16f : 0.04f));
        g.fillEllipse (cx - inner, cy - inner, inner * 2.0f, inner * 2.0f);

        const float pr = inner - 5.0f;
        g.setColour (accent);
        g.fillEllipse (cx + pr * std::sin (angle) - 2.5f, cy - pr * std::cos (angle) - 2.5f, 5.0f, 5.0f);

        g.setColour (theme::text);
        g.setFont (theme::font (juce::jmax (11.0f, size * 0.21f), true));
        g.drawText (juce::String (juce::roundToInt (slider.getValue() * 100.0)), sq.toNearestInt(),
                    juce::Justification::centred, false);
    }
};

// =============================================================================
//  Button mit Vektor-Icon
// =============================================================================
class PillButton : public juce::Component
{
public:
    enum class Icon { none, dice, sparkle, play, stop, prev, next };

    std::function<void()> onClick;

    void setText (const juce::String& t)            { if (text != t) { text = t; repaint(); } }
    void setIcon (Icon i)                           { if (icon != i) { icon = i; repaint(); } }
    void setPrimary (bool p)                        { primary = p; repaint(); }
    void setAccent (juce::Colour c)                 { if (accent != c) { accent = c; repaint(); } }
    void setFontSize (float s)                      { fontSize = s; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = juce::jmin (14.0f, b.getHeight() * 0.5f);
        const bool enabled = isEnabled();

        auto fill = primary ? accent : theme::panelHi;
        if (down)       fill = fill.darker (0.25f);
        else if (over)  fill = fill.brighter (primary ? 0.14f : 0.2f);

        if (primary)
        {
            g.setGradientFill (juce::ColourGradient (fill.brighter (0.2f), 0.0f, b.getY(), fill.darker (0.75f), 0.0f, b.getBottom(), false));
            g.fillRoundedRectangle (b, radius);

            // Lila laeuft von der Oberkante herunter
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (b, radius);
            g.reduceClipRegion (clip);
            const auto drips = theme::makeDrips (b.getX() + 8.0f, b.getRight() - 8.0f, 4242u, 4.0f, 17.0f, 4.0f, 8.0f, 8.0f, 30.0f);
            theme::drawDrips (g, b.getX(), b.getRight(), b.getY() - 2.0f, b.getY() + 4.0f, drips, juce::Colours::white.withAlpha (0.22f), false);
        }
        else
        {
            g.setColour (fill.withMultipliedAlpha (enabled ? 1.0f : 0.45f));
            g.fillRoundedRectangle (b, radius);
        }

        if (! primary)
        {
            g.setColour (theme::line.withMultipliedAlpha (enabled ? 1.0f : 0.5f));
            g.drawRoundedRectangle (b, radius, 1.2f);
        }

        const auto fg = (primary ? juce::Colours::white : theme::text).withMultipliedAlpha (enabled ? 1.0f : 0.4f);

        if (icon != Icon::none)
        {
            const float s = juce::jmin (22.0f, b.getHeight() * 0.48f);
            const auto ia = text.isEmpty() ? b.withSizeKeepingCentre (s, s)
                                           : juce::Rectangle<float> (b.getX() + 16.0f, b.getCentreY() - s * 0.5f, s, s);
            switch (icon)
            {
                case Icon::dice:    theme::drawDice (g, ia, fg); break;
                case Icon::sparkle: theme::drawSparkle (g, ia, fg); break;
                case Icon::play:    theme::drawPlay (g, ia, fg); break;
                case Icon::stop:    theme::drawStop (g, ia, fg); break;
                case Icon::prev:    theme::drawChevron (g, ia, fg, true); break;
                case Icon::next:    theme::drawChevron (g, ia, fg, false); break;
                case Icon::none:    break;
            }
        }

        if (text.isNotEmpty())
        {
            auto ta = b;
            if (icon != Icon::none)
                ta = ta.withTrimmedLeft (30.0f).withTrimmedRight (8.0f);
            g.setColour (fg);
            g.setFont (theme::font (fontSize, true));
            g.drawFittedText (text, ta.toNearestInt(), juce::Justification::centred, 1, 0.8f);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override   { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override    { over = false; down = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override    { down = true; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const bool hit = down && getLocalBounds().contains (e.getPosition());
        down = false;
        repaint();
        if (hit && isEnabled() && onClick != nullptr)
            onClick();
    }

private:
    juce::String text;
    Icon icon = Icon::none;
    bool primary = false, over = false, down = false;
    juce::Colour accent = theme::purple;
    float fontSize = 15.0f;
};

// =============================================================================
//  Schalter als Chip
// =============================================================================
class ToggleChip : public juce::Component
{
public:
    std::function<void()> onChange;

    void setText (const juce::String& t)      { text = t; }
    void setAccent (juce::Colour c)           { if (accent != c) { accent = c; repaint(); } }
    void setState (bool shouldBeOn)           { if (on != shouldBeOn) { on = shouldBeOn; repaint(); } }
    bool getState() const                     { return on; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = b.getHeight() * 0.5f;

        g.setColour (on ? accent.withAlpha (over ? 0.26f : 0.18f) : theme::panelHi.brighter (over ? 0.12f : 0.0f));
        g.fillRoundedRectangle (b, radius);
        g.setColour (on ? accent : theme::line);
        g.drawRoundedRectangle (b, radius, 1.3f);

        const float d = 10.0f;
        const juce::Rectangle<float> dot (b.getX() + 12.0f, b.getCentreY() - d * 0.5f, d, d);
        if (on) { g.setColour (accent); g.fillEllipse (dot); }
        else    { g.setColour (theme::textDim); g.drawEllipse (dot, 1.4f); }

        g.setColour (on ? theme::text : theme::textDim);
        g.setFont (theme::font (13.5f, true));
        g.drawFittedText (text, b.withTrimmedLeft (30.0f).withTrimmedRight (8.0f).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.8f);
    }

    void mouseEnter (const juce::MouseEvent&) override   { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override    { over = false; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()))
        {
            on = ! on;
            repaint();
            if (onChange != nullptr)
                onChange();
        }
    }

private:
    juce::String text;
    juce::Colour accent = theme::purple;
    bool on = false, over = false;
};

// =============================================================================
//  Akkordkarte
// =============================================================================
class ChordCard : public juce::Component
{
public:
    std::function<void()> onSelect, onLock, onReroll;

    void setData (const juce::String& chordName, const juce::String& romanNumeral, const juce::String& noteNames,
                  bool isLocked, int chordQuality, int slotIndex)
    {
        name = chordName; roman = romanNumeral; notes = noteNames;
        locked = isLocked; quality = chordQuality; index = slotIndex;
        repaint();
    }

    void setCurrent (bool c)     { if (current != c) { current = c; repaint(); } }
    void setSelected (bool s)    { if (selected != s) { selected = s; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        const auto full = getLocalBounds().toFloat();
        const auto b = full.reduced (3.0f);
        const bool compact = b.getHeight() < 130.0f;
        const auto qc = theme::qualityColour (quality);

        if (current)
        {
            g.setColour (qc.withAlpha (0.22f));
            g.fillRoundedRectangle (full, 16.0f);
        }

        g.setColour (current ? theme::panelHi.brighter (0.14f) : (over || selected ? theme::panelHi.brighter (0.06f) : theme::panelHi));
        g.fillRoundedRectangle (b, 13.0f);

        // Farbstreifen oben = Akkordart, mit herunterlaufender Farbe
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (b, 13.0f);
            g.reduceClipRegion (clip);
            const auto drips = theme::makeDrips (b.getX() + b.getWidth() * (compact ? 0.56f : 0.5f), b.getRight() - 38.0f, 77u + 31u * (uint32_t) index,
                                                 7.0f, compact ? 14.0f : 28.0f, 5.0f, 8.0f, 4.0f, 16.0f);
            theme::drawDrips (g, b.getX(), b.getRight(), b.getY(), b.getY() + 5.0f, drips, qc.withAlpha (current ? 1.0f : 0.88f), ! compact);
        }

        if (current)        { g.setColour (qc);                          g.drawRoundedRectangle (b, 13.0f, 2.0f); }
        else if (locked)    { g.setColour (theme::gold.withAlpha (0.9f)); g.drawRoundedRectangle (b, 13.0f, 1.5f); }
        else if (selected)  { g.setColour (theme::textDim);               g.drawRoundedRectangle (b, 13.0f, 1.2f); }
        else                { g.setColour (theme::line);                  g.drawRoundedRectangle (b, 13.0f, 1.0f); }

        // Stufe
        g.setColour (qc);
        g.setFont (theme::font (compact ? 12.5f : 14.0f, true));
        g.drawFittedText (roman, juce::Rectangle<float> (b.getX() + 12.0f, b.getY() + 10.0f, b.getWidth() - 46.0f, 20.0f).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.7f);

        // Akkordname
        const auto nameArea = b.reduced (8.0f, 0.0f).withTrimmedTop (compact ? 26.0f : 42.0f).withTrimmedBottom (compact ? 24.0f : 62.0f);
        g.setColour (theme::text);
        g.setFont (theme::font (compact ? 23.0f : 31.0f, true));
        g.drawFittedText (name, nameArea.toNearestInt(), juce::Justification::centred, 1, 0.5f);

        // Toene
        g.setColour (theme::textDim);
        g.setFont (theme::font (compact ? 11.0f : 12.5f));
        const auto notesArea = compact ? juce::Rectangle<float> (b.getX() + 26.0f, b.getBottom() - 24.0f, b.getWidth() - 56.0f, 18.0f)
                                       : juce::Rectangle<float> (b.getX() + 8.0f, b.getBottom() - 60.0f, b.getWidth() - 16.0f, 20.0f);
        g.drawFittedText (notes, notesArea.toNearestInt(), juce::Justification::centred, 1, 0.6f);

        // Slot-Nummer
        g.setColour (theme::textDim.withAlpha (0.75f));
        g.setFont (theme::font (11.5f, true));
        g.drawText (juce::String (index + 1), juce::Rectangle<float> (b.getX() + 11.0f, b.getBottom() - 25.0f, 20.0f, 18.0f).toNearestInt(),
                    juce::Justification::centredLeft, false);

        // Schloss + Wuerfel
        theme::drawLock (g, lockZone().reduced (5.0f), locked ? theme::gold : theme::textDim.withAlpha (hover == 1 ? 1.0f : 0.5f), locked);
        theme::drawDice (g, diceZone().reduced (5.0f), theme::textDim.withAlpha (locked ? 0.2f : (hover == 2 ? 1.0f : 0.55f)));
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = zoneAt (e.position);
        if (h != hover || ! over) { hover = h; over = true; repaint(); }
    }

    void mouseExit (const juce::MouseEvent&) override    { hover = 0; over = false; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition()))
            return;

        const int zone = zoneAt (e.position);
        if (zone == 1)                                  { if (onLock != nullptr) onLock(); }
        else if (zone == 2 || e.mods.isPopupMenu())     { if (onReroll != nullptr) onReroll(); }
        else                                            { if (onSelect != nullptr) onSelect(); }
    }

private:
    juce::Rectangle<float> lockZone() const
    {
        const auto b = getLocalBounds().toFloat().reduced (3.0f);
        return { b.getRight() - 34.0f, b.getY() + 6.0f, 28.0f, 28.0f };
    }

    juce::Rectangle<float> diceZone() const
    {
        const auto b = getLocalBounds().toFloat().reduced (3.0f);
        return { b.getRight() - 34.0f, b.getBottom() - 32.0f, 28.0f, 28.0f };
    }

    int zoneAt (juce::Point<float> p) const
    {
        if (lockZone().expanded (3.0f).contains (p)) return 1;
        if (diceZone().expanded (3.0f).contains (p)) return 2;
        return 0;
    }

    juce::String name, roman, notes;
    bool locked = false, current = false, selected = false, over = false;
    int quality = 0, index = 0, hover = 0;
};

// =============================================================================
//  Klaviatur-Anzeige (zeigt die Toene des aktuellen Akkords)
// =============================================================================
class KeyboardView : public juce::Component
{
public:
    void setNotes (const std::vector<int>& n, juce::Colour c)
    {
        if (n != notes || c != colour)
        {
            notes = n;
            colour = c;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const int lo = 36, hi = 96;                      // C2..C7 = 36 weisse Tasten
        const auto b = getLocalBounds().toFloat();
        const float ww = b.getWidth() / 36.0f;
        const float bw = ww * 0.62f, bh = b.getHeight() * 0.6f;

        auto isBlack = [] (int n) { const int p = n % 12; return p == 1 || p == 3 || p == 6 || p == 8 || p == 10; };
        auto isOn = [this] (int n) { return std::find (notes.begin(), notes.end(), n) != notes.end(); };

        int w = 0;
        for (int n = lo; n <= hi; ++n)
        {
            if (isBlack (n)) continue;
            const juce::Rectangle<float> r (b.getX() + (float) w * ww + 0.75f, b.getY(), ww - 1.5f, b.getHeight());
            g.setColour (isOn (n) ? colour : (n % 12 == 0 ? juce::Colour (0xff3a2d63) : juce::Colour (0xff2c2250)));
            g.fillRoundedRectangle (r, 2.5f);
            ++w;
        }

        w = 0;
        for (int n = lo; n <= hi; ++n)
        {
            if (! isBlack (n)) { ++w; continue; }
            const juce::Rectangle<float> r (b.getX() + (float) w * ww - bw * 0.5f, b.getY(), bw, bh);
            g.setColour (theme::bg);
            g.fillRoundedRectangle (r.expanded (0.8f), 2.5f);
            g.setColour (isOn (n) ? colour.darker (0.25f) : juce::Colour (0xff0c0817));
            g.fillRoundedRectangle (r, 2.0f);
        }
    }

private:
    std::vector<int> notes;
    juce::Colour colour = theme::purple;
};

// =============================================================================
//  Feld zum Herausziehen der MIDI-Datei in den Host
// =============================================================================
class DragTile : public juce::Component
{
public:
    std::function<juce::File()> fileProvider;
    std::function<void()> onHint, onDragged;

    DragTile()   { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (1.0f);

        g.setColour (theme::purple.withAlpha (over ? 0.2f : 0.1f));
        g.fillRoundedRectangle (b, 14.0f);

        juce::Path outline, dashed;
        outline.addRoundedRectangle (b.reduced (0.75f), 14.0f);
        const float dashes[2] = { 6.0f, 5.0f };
        juce::PathStrokeType (1.6f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (theme::purple);
        g.fillPath (dashed);

        // Griffpunkte
        g.setColour (theme::purple.withAlpha (0.8f));
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 2; ++col)
                g.fillEllipse (b.getX() + 15.0f + (float) col * 7.0f, b.getCentreY() - 9.0f + (float) row * 7.0f, 3.6f, 3.6f);

        theme::drawNote (g, b.getX() + 46.0f, b.getCentreY() + 9.0f, 15.0f, theme::purple);

        auto textArea = b.withTrimmedLeft (68.0f).withTrimmedRight (10.0f);
        g.setColour (theme::text);
        g.setFont (theme::font (15.0f, true));
        g.drawFittedText ("MIDI ziehen", textArea.removeFromTop (b.getHeight() * 0.56f).withTrimmedTop (9.0f).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.8f);
        g.setColour (theme::textDim);
        g.setFont (theme::font (12.0f));
        g.drawFittedText ("in Piano Roll oder Playlist", textArea.withTrimmedBottom (8.0f).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.7f);
    }

    void mouseEnter (const juce::MouseEvent&) override   { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override    { over = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override    { dragging = false; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6 || fileProvider == nullptr)
            return;

        dragging = true;
        const auto file = fileProvider();
        if (! file.existsAsFile())
            return;

        juce::StringArray files;
        files.add (file.getFullPathName());
        juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false, this);

        if (onDragged != nullptr)
            onDragged();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragging && onHint != nullptr)
            onHint();
        dragging = false;
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (fileProvider != nullptr)
        {
            const auto file = fileProvider();
            if (file.existsAsFile())
                file.revealToUser();
        }
    }

private:
    bool over = false, dragging = false;
};

// =============================================================================
//  Preset-Browser: bekannte Akkordfolgen zum Anklicken
// =============================================================================
class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel
{
public:
    std::function<void (int presetIndex)> onChoose;

    PresetBrowser()
    {
        categoryBox.addItem ("Alle Stile", 1);
        for (int c = 0; c < purr::kNumPresetCategories; ++c)
            categoryBox.addItem (theme::T8 (purr::presetCategoryName (c)), c + 2);
        categoryBox.setSelectedId (1, juce::dontSendNotification);
        categoryBox.onChange = [this] { rebuild(); };
        addAndMakeVisible (categoryBox);

        for (int i = 0; i < purr::numPresets(); ++i)
        {
            names.add (theme::T8 (purr::preset (i).name));
            romans.add (theme::T8 (purr::Engine::presetRomans (i).c_str()));
        }

        list.setModel (this);
        list.setRowHeight (46);
        list.setOutlineThickness (0);
        list.setMouseMoveSelectsRows (true);
        addAndMakeVisible (list);
        rebuild();
    }

    void setCurrent (int presetIndex)
    {
        if (current != presetIndex)
        {
            current = presetIndex;
            list.repaint();
        }
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced (12);
        a.removeFromTop (50);
        categoryBox.setBounds (a.removeFromTop (32));
        a.removeFromTop (8);
        list.setBounds (a);
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (theme::panel);
        g.fillRoundedRectangle (b, 16.0f);

        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (b, 16.0f);
            g.reduceClipRegion (clip);
            const auto drips = theme::makeDrips (b.getX() + 112.0f, b.getRight() - 8.0f, 9001u, 6.0f, 34.0f, 5.0f, 9.0f, 5.0f, 22.0f);
            theme::drawDrips (g, b.getX(), b.getRight(), b.getY(), b.getY() + 7.0f, drips, theme::purple);
        }

        g.setColour (theme::line.withAlpha (0.6f));
        g.drawRoundedRectangle (b.reduced (0.5f), 16.0f, 1.0f);

        g.setColour (theme::text);
        g.setFont (theme::font (16.0f, true));
        g.drawText ("PRESETS", 14, 16, 110, 22, juce::Justification::centredLeft, false);
        g.setColour (theme::textDim);
        g.setFont (theme::font (11.5f));
        g.drawText (juce::String (purr::numPresets()) + " bekannte Folgen", 14, 37, 160, 16, juce::Justification::centredLeft, false);
    }

private:
    int getNumRows() override   { return (int) rows.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (row < 0 || row >= (int) rows.size())
            return;

        const int idx = rows[(size_t) row];
        const bool isCurrent = idx == current;
        const auto r = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (1.0f, 2.0f);

        if (isCurrent)
        {
            g.setGradientFill (juce::ColourGradient (theme::purpleDeep, r.getX(), 0.0f, theme::purpleDark, r.getRight(), 0.0f, false));
            g.fillRoundedRectangle (r, 9.0f);
        }
        else if (rowIsSelected)
        {
            g.setColour (theme::panelHi.brighter (0.08f));
            g.fillRoundedRectangle (r, 9.0f);
        }

        g.setColour (isCurrent ? theme::purpleHi : theme::line.brighter (0.2f));
        g.fillRoundedRectangle (r.getX() + 5.0f, r.getY() + 8.0f, 3.0f, r.getHeight() - 16.0f, 1.5f);

        g.setColour (theme::text);
        g.setFont (theme::font (14.0f, true));
        g.drawFittedText (names[idx], 17, 5, width - 24, 18, juce::Justification::centredLeft, 1, 0.8f);

        g.setColour (isCurrent ? theme::text.withAlpha (0.85f) : theme::textDim);
        g.setFont (theme::font (11.5f));
        g.drawFittedText (romans[idx], 17, 23, width - 24, 16, juce::Justification::centredLeft, 1, 0.6f);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (row >= 0 && row < (int) rows.size() && onChoose != nullptr)
            onChoose (rows[(size_t) row]);
    }

    void rebuild()
    {
        const int category = categoryBox.getSelectedId() - 2;
        rows.clear();
        for (int i = 0; i < purr::numPresets(); ++i)
            if (category < 0 || purr::preset (i).category == category)
                rows.push_back (i);

        list.updateContent();
        list.deselectAllRows();
        list.repaint();
    }

    juce::ComboBox categoryBox;
    juce::ListBox list;
    juce::StringArray names, romans;
    std::vector<int> rows;
    int current = -1;
};
