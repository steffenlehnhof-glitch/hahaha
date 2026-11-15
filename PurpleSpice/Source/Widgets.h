#pragma once

#include "Theme.h"

#include <functional>
#include <vector>

// =============================================================================
//  Look & Feel fuer Drehregler, Auswahlboxen und Popup-Menues
// =============================================================================
class SpiceLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SpiceLookAndFeel()
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
//  Feld zum Herausziehen der MIDI-Datei in den Host
// =============================================================================
class DragTile : public juce::Component
{
public:
    std::function<juce::File()> fileProvider;
    std::function<void()> onHint, onDragged;

    DragTile()   { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }

    void setTexts (const juce::String& newTitle, const juce::String& newSubtitle)
    {
        if (title != newTitle || subtitle != newSubtitle)
        {
            title = newTitle;
            subtitle = newSubtitle;
            repaint();
        }
    }

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
        g.drawFittedText (title, textArea.removeFromTop (b.getHeight() * 0.56f).withTrimmedTop (9.0f).toNearestInt(),
                          juce::Justification::centredLeft, 1, 0.8f);
        g.setColour (theme::textDim);
        g.setFont (theme::font (12.0f));
        g.drawFittedText (subtitle, textArea.withTrimmedBottom (8.0f).toNearestInt(),
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
    juce::String title { "MIDI ziehen" }, subtitle;
    bool over = false, dragging = false;
};

// =============================================================================
//  Pattern-Ansicht: zeigt jeden Schlag (Hoehe = Lautstaerke, oben = Tonhoehe)
// =============================================================================
class PatternView : public juce::Component
{
public:
    std::function<void (int bar)> onLock, onReroll;

    void setPattern (const std::vector<spice::NoteEvent>& newEvents, const std::vector<bool>& lockedBars)
    {
        events = newEvents;
        locked = lockedBars;
        repaint();
    }

    void setPlayhead (double beats)
    {
        if (std::abs (beats - playhead) > 1.0e-4)
        {
            playhead = beats;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const int numBars = (int) locked.size();
        if (numBars == 0) return;

        for (int bar = 0; bar < numBars; ++bar)
        {
            const auto cell = barBounds (bar);
            const bool isLocked = locked[(size_t) bar];

            g.setColour (theme::panel);
            g.fillRoundedRectangle (cell, 12.0f);
            g.setColour (isLocked ? theme::gold.withAlpha (0.9f) : theme::line.withAlpha (0.7f));
            g.drawRoundedRectangle (cell.reduced (0.5f), 12.0f, isLocked ? 1.5f : 1.0f);

            // Kopf: Taktnummer, Schloss, Wuerfel
            g.setColour (theme::textDim);
            g.setFont (theme::font (11.0f, true));
            g.drawText ("TAKT " + juce::String (bar + 1), juce::Rectangle<float> (cell.getX() + 10.0f, cell.getY() + 3.0f, 80.0f, headerH - 2.0f).toNearestInt(),
                        juce::Justification::centredLeft, false);
            theme::drawLock (g, lockZone (bar).reduced (3.0f), isLocked ? theme::gold : theme::textDim.withAlpha (hoverBar == bar && hoverZone == 1 ? 1.0f : 0.5f), isLocked);
            theme::drawDice (g, diceZone (bar).reduced (3.0f), theme::textDim.withAlpha (isLocked ? 0.2f : (hoverBar == bar && hoverZone == 2 ? 1.0f : 0.55f)));

            // Spuren
            const auto lanes = cell.withTrimmedTop (headerH).reduced (8.0f, 0.0f).withTrimmedBottom (7.0f);
            const float pitchH = juce::jmin (26.0f, lanes.getHeight() * 0.28f);
            const auto pitchLane = lanes.withHeight (pitchH);
            const auto velLane = lanes.withTrimmedTop (pitchH + 3.0f);
            const float beatW = lanes.getWidth() / 4.0f;

            g.setColour (theme::ink.withAlpha (0.55f));
            g.fillRoundedRectangle (lanes, 6.0f);

            // Raster: Viertel kraeftig, Achtel und Sechzehntel zart
            for (int i = 0; i <= 16; ++i)
            {
                const float x = lanes.getX() + beatW * 0.25f * (float) i;
                const bool beat = i % 4 == 0, eighth = i % 2 == 0;
                if (! beat && ! eighth && beatW < 60.0f) continue;
                g.setColour (theme::line.withAlpha (beat ? 0.95f : (eighth ? 0.45f : 0.22f)));
                g.fillRect (x - 0.5f, lanes.getY() + (beat ? 0.0f : 3.0f), 1.0f, lanes.getHeight() - (beat ? 0.0f : 6.0f));
            }

            // Mittellinie der Tonhoehe
            g.setColour (theme::line.withAlpha (0.6f));
            g.fillRect (lanes.getX(), pitchLane.getCentreY() - 0.5f, lanes.getWidth(), 1.0f);

            const double barStart = 4.0 * bar;
            float lastDotX = 0.0f, lastDotY = 0.0f;
            bool lastWasRoll = false;

            for (const auto& e : events)
            {
                // kleine Toleranz: ein per Humanize vorgezogener Schlag auf der Eins gehoert zu seinem Takt
                const double rel = e.start - barStart;
                if (rel < -0.05 || rel >= 3.95) continue;

                const float x = lanes.getX() + (float) juce::jmax (0.0, rel) * beatW;
                const auto colour = theme::kindColour (e.kind);

                // Lautstaerke-Balken
                const float h = juce::jmax (3.0f, velLane.getHeight() * (float) e.velocity / 127.0f);
                float w = juce::jlimit (2.0f, 9.0f, (float) e.length * beatW - 1.0f);
                if (e.kind == spice::kOpen)
                    w = juce::jmax (4.0f, (float) e.length * beatW - 2.0f);

                const juce::Rectangle<float> barRect (x, velLane.getBottom() - h, w, h);
                if (e.kind == spice::kOpen)
                {
                    g.setColour (colour.withAlpha (0.28f));
                    g.fillRoundedRectangle (barRect, 2.5f);
                    g.setColour (colour);
                    g.fillRoundedRectangle (barRect.withWidth (3.5f), 1.5f);
                    g.fillRoundedRectangle (barRect.withHeight (2.5f), 1.2f);
                }
                else
                {
                    g.setGradientFill (juce::ColourGradient (colour.brighter (0.25f), 0.0f, barRect.getY(),
                                                             colour.darker (0.55f), 0.0f, barRect.getBottom(), false));
                    g.fillRoundedRectangle (barRect, juce::jmin (2.0f, w * 0.5f));
                }

                // Tonhoehe als Punkt, in Rolls zur Linie verbunden
                const float py = pitchLane.getCentreY() - (float) juce::jlimit (-12, 12, e.pitch) / 12.0f * (pitchLane.getHeight() * 0.5f - 2.5f);
                const float px = x + juce::jmin (w, 4.0f) * 0.5f;
                if (e.pitch != 0 || e.kind == spice::kRoll)
                {
                    if (lastWasRoll && e.kind == spice::kRoll && px - lastDotX < beatW * 0.6f)
                    {
                        g.setColour (colour.withAlpha (0.6f));
                        g.drawLine (lastDotX, lastDotY, px, py, 1.2f);
                    }
                    g.setColour (e.pitch != 0 ? juce::Colours::white : colour.withAlpha (0.7f));
                    g.fillEllipse (px - 1.8f, py - 1.8f, 3.6f, 3.6f);
                }
                lastWasRoll = e.kind == spice::kRoll;
                lastDotX = px;
                lastDotY = py;
            }

            // Abspielposition
            if (playhead >= barStart && playhead < barStart + 4.0)
            {
                const float x = lanes.getX() + (float) (playhead - barStart) * beatW;
                g.setColour (juce::Colours::white.withAlpha (0.16f));
                g.fillRect (x - 3.0f, lanes.getY(), 6.0f, lanes.getHeight());
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.fillRect (x - 0.75f, lanes.getY(), 1.5f, lanes.getHeight());
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        int bar = -1, zone = 0;
        hitTestZones (e.position, bar, zone);
        if (bar != hoverBar || zone != hoverZone)
        {
            hoverBar = bar;
            hoverZone = zone;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override    { hoverBar = -1; hoverZone = 0; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        int bar = -1, zone = 0;
        hitTestZones (e.position, bar, zone);
        if (bar < 0) return;
        if (zone == 1 && onLock != nullptr)        onLock (bar);
        else if (zone == 2 && onReroll != nullptr) onReroll (bar);
    }

private:
    static constexpr float headerH = 20.0f;

    juce::Rectangle<float> barBounds (int bar) const
    {
        const int numBars = (int) locked.size();
        const int perRow = numBars == 1 ? 1 : 2;
        const int rows = (numBars + perRow - 1) / perRow;
        const float gap = 8.0f;
        const auto b = getLocalBounds().toFloat();
        const float w = (b.getWidth() - gap * (float) (perRow - 1)) / (float) perRow;
        const float h = (b.getHeight() - gap * (float) (rows - 1)) / (float) rows;
        return { b.getX() + (float) (bar % perRow) * (w + gap), b.getY() + (float) (bar / perRow) * (h + gap), w, h };
    }

    juce::Rectangle<float> lockZone (int bar) const
    {
        const auto c = barBounds (bar);
        return { c.getRight() - 50.0f, c.getY() + 1.0f, 20.0f, 20.0f };
    }

    juce::Rectangle<float> diceZone (int bar) const
    {
        const auto c = barBounds (bar);
        return { c.getRight() - 28.0f, c.getY() + 1.0f, 20.0f, 20.0f };
    }

    void hitTestZones (juce::Point<float> p, int& bar, int& zone) const
    {
        bar = -1;
        zone = 0;
        for (int i = 0; i < (int) locked.size(); ++i)
        {
            if (! barBounds (i).contains (p)) continue;
            bar = i;
            if (lockZone (i).expanded (3.0f).contains (p))       zone = 1;
            else if (diceZone (i).expanded (3.0f).contains (p))  zone = 2;
            return;
        }
    }

    std::vector<spice::NoteEvent> events;
    std::vector<bool> locked;
    double playhead = -1.0;
    int hoverBar = -1, hoverZone = 0;
};
