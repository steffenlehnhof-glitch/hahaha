#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "HatEngine.h"

#include <vector>

// Purple Spice: Farben, Schrift und Zeichen-Helfer fuer das Overlay ("Lila Drip")
namespace theme
{
const juce::Colour bg         { 0xff06040b };
const juce::Colour bgHi       { 0xff150b29 };
const juce::Colour panel      { 0xff0e0a19 };
const juce::Colour panelHi    { 0xff19122e };
const juce::Colour line       { 0xff32245c };
const juce::Colour text       { 0xfff3ecff };
const juce::Colour textDim    { 0xff9285bd };
const juce::Colour ink        { 0xff0a0613 };

const juce::Colour purple     { 0xffa855f7 };
const juce::Colour purpleHi   { 0xffc79bff };
const juce::Colour purpleDeep { 0xff6d28d9 };
const juce::Colour purpleDark { 0xff35186e };
const juce::Colour magenta    { 0xffe879f9 };
const juce::Colour gold       { 0xffffd166 };



// Farben der Schlaege in der Pattern-Ansicht
inline juce::Colour kindColour (int kind)
{
    switch (kind)
    {
        case spice::kRoll:  return juce::Colour (0xfff0abfc);
        case spice::kOpen:  return gold;
        case spice::kGhost: return juce::Colour (0xff7c5cc4);
        case spice::kFlam:  return juce::Colour (0xff8b9cff);
        default:            return purple;
    }
}

inline juce::Font font (float height, bool bold = false, bool italic = false)
{
    const int style = (bold ? juce::Font::bold : juce::Font::plain) | (italic ? juce::Font::italic : juce::Font::plain);
   #if JUCE_MAJOR_VERSION >= 8
    return juce::Font (juce::FontOptions (height, style));
   #else
    return juce::Font (height, style);
   #endif
}

// Schriftzug in Chrom-Lila: Text als Pfad, gefuellt mit einem Chrom-Verlauf (heller Himmel,
// dunkle Horizontlinie, heller Boden), dazu dunkle Kontur, Lichtkante und Schein
inline juce::Rectangle<float> drawChromeText (juce::Graphics& g, const juce::String& label, float x, float baseline, float height)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (font (height, true, true), label, x, baseline);
    juce::Path p;
    glyphs.createPath (p);
    const auto b = p.getBounds();

    g.setColour (juce::Colour (0xffa855f7).withAlpha (0.28f));
    g.strokePath (p, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xff12052b));
    g.strokePath (p, juce::PathStrokeType (3.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::ColourGradient chrome (juce::Colour (0xfffbf7ff), 0.0f, b.getY(), juce::Colour (0xff4c1d95), 0.0f, b.getBottom(), false);
    chrome.addColour (0.28, juce::Colour (0xffdcc0ff));
    chrome.addColour (0.46, juce::Colour (0xff9a63f2));
    chrome.addColour (0.50, juce::Colour (0xff2a0d5e));
    chrome.addColour (0.57, juce::Colour (0xff8750ee));
    chrome.addColour (0.80, juce::Colour (0xfff4e9ff));
    g.setGradientFill (chrome);
    g.fillPath (p);

    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.strokePath (p, juce::PathStrokeType (0.6f));
    return b;
}

// Quelltext ist UTF-8; so kommen Umlaute sicher in juce::String an
inline juce::String T8 (const char* utf8) { return juce::String::fromUTF8 (utf8); }

inline juce::PathStrokeType roundStroke (float thickness)
{
    return juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
}

// ---- Drips -------------------------------------------------------------------
struct Drip { float x, width, length; };

// Verteilt Tropfen zwischen left und right. Gleicher seed = gleiches Bild.
inline std::vector<Drip> makeDrips (float left, float right, uint32_t seed, float minLen, float maxLen,
                                    float minWidth, float maxWidth, float minGap, float maxGap)
{
    auto next = [&seed]
    {
        seed = seed * 1664525u + 1013904223u;
        return (float) ((seed >> 8) & 0xffff) / 65535.0f;
    };

    std::vector<Drip> drips;
    float x = left + minGap * next();

    for (;;)
    {
        const float w = minWidth + (maxWidth - minWidth) * next();
        x += w * 1.6f;
        if (x + w * 1.6f > right)
            break;

        const float r = next();
        drips.push_back ({ x, w, minLen + (maxLen - minLen) * r * r });   // wenige lange, viele kurze
        x += w * 1.6f + minGap + (maxGap - minGap) * next();
    }
    return drips;
}

// Band von top bis baseY, aus dessen Unterkante die Tropfen herunterlaufen
inline juce::Path dripPath (float left, float right, float top, float baseY, const std::vector<Drip>& drips)
{
    juce::Path p;
    p.startNewSubPath (left, top);
    p.lineTo (right, top);
    p.lineTo (right, baseY);

    for (auto it = drips.rbegin(); it != drips.rend(); ++it)
    {
        const float hw = it->width * 0.5f;
        const float k = hw * 0.9f;
        const float cx = it->x;
        const float tip = baseY + juce::jmax (it->length, hw + k + 1.0f);

        p.lineTo (cx + hw + k, baseY);
        p.quadraticTo (cx + hw, baseY, cx + hw, baseY + k);
        p.lineTo (cx + hw, tip - hw);
        p.cubicTo (cx + hw * 1.35f, tip + hw * 0.95f, cx - hw * 1.35f, tip + hw * 0.95f, cx - hw, tip - hw);
        p.lineTo (cx - hw, baseY + k);
        p.quadraticTo (cx - hw, baseY, cx - hw - k, baseY);
    }

    p.lineTo (left, baseY);
    p.closeSubPath();
    return p;
}

// Zeichnet das Band samt Tropfen mit Farbverlauf und Glanzpunkten
inline void drawDrips (juce::Graphics& g, float left, float right, float top, float baseY,
                       const std::vector<Drip>& drips, juce::Colour colour, bool gloss = true)
{
    float longest = 0.0f;
    for (const auto& d : drips)
        longest = juce::jmax (longest, d.length);

    g.setGradientFill (juce::ColourGradient (colour.brighter (0.25f), 0.0f, top,
                                             colour.darker (0.55f), 0.0f, baseY + longest + 6.0f, false));
    g.fillPath (dripPath (left, right, top, baseY, drips));

    if (! gloss)
        return;

    g.setColour (juce::Colours::white.withAlpha (0.32f));
    for (const auto& d : drips)
    {
        if (d.width < 5.0f) continue;
        const float hw = d.width * 0.5f;
        const float tip = baseY + juce::jmax (d.length, hw * 1.9f + 1.0f);
        g.fillEllipse (d.x - hw * 0.55f, tip - hw * 1.1f, hw * 0.55f, hw * 0.8f);
    }
}

// einzelner fallender Tropfen
inline void drawDroplet (juce::Graphics& g, float cx, float cy, float size, juce::Colour colour)
{
    juce::Path p;
    p.startNewSubPath (cx, cy - size);
    p.cubicTo (cx + size * 0.2f, cy - size * 0.35f, cx + size * 0.62f, cy, cx + size * 0.62f, cy + size * 0.32f);
    p.cubicTo (cx + size * 0.62f, cy + size * 0.85f, cx - size * 0.62f, cy + size * 0.85f, cx - size * 0.62f, cy + size * 0.32f);
    p.cubicTo (cx - size * 0.62f, cy, cx - size * 0.2f, cy - size * 0.35f, cx, cy - size);
    p.closeSubPath();
    g.setColour (colour);
    g.fillPath (p);
}

// ---- kleine Vektor-Icons (alle in ein Quadrat "area" gezeichnet) -----------
inline void drawLock (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour c, bool closed)
{
    const float w = area.getWidth(), h = area.getHeight();
    const auto body = juce::Rectangle<float> (area.getX() + w * 0.18f, area.getY() + h * 0.46f, w * 0.64f, h * 0.46f);

    juce::Path shackle;
    const float sx = area.getCentreX(), r = w * 0.2f, top = area.getY() + h * 0.12f;
    shackle.startNewSubPath (sx - r, body.getY());
    shackle.lineTo (sx - r, top + r);
    shackle.addCentredArc (sx, top + r, r, r, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi);
    if (closed)
        shackle.lineTo (sx + r, body.getY());

    g.setColour (c);
    g.strokePath (shackle, roundStroke (w * 0.11f));
    g.fillRoundedRectangle (body, w * 0.12f);
}

inline void drawDice (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour c)
{
    const auto box = area.reduced (area.getWidth() * 0.1f);
    g.setColour (c);
    g.drawRoundedRectangle (box, box.getWidth() * 0.24f, juce::jmax (1.4f, box.getWidth() * 0.1f));

    const float d = box.getWidth() * 0.17f;
    auto pip = [&] (float fx, float fy)
    {
        g.fillEllipse (box.getX() + box.getWidth() * fx - d * 0.5f, box.getY() + box.getHeight() * fy - d * 0.5f, d, d);
    };
    pip (0.3f, 0.3f); pip (0.7f, 0.3f); pip (0.5f, 0.5f); pip (0.3f, 0.7f); pip (0.7f, 0.7f);
}

inline void drawSparkle (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour c)
{
    auto star = [] (float cx, float cy, float r)
    {
        juce::Path p;
        p.startNewSubPath (cx, cy - r);
        p.quadraticTo (cx, cy, cx + r, cy);
        p.quadraticTo (cx, cy, cx, cy + r);
        p.quadraticTo (cx, cy, cx - r, cy);
        p.quadraticTo (cx, cy, cx, cy - r);
        p.closeSubPath();
        return p;
    };

    const float w = area.getWidth();
    g.setColour (c);
    g.fillPath (star (area.getX() + w * 0.42f, area.getY() + w * 0.56f, w * 0.42f));
    g.fillPath (star (area.getX() + w * 0.8f,  area.getY() + w * 0.22f, w * 0.2f));
}

inline void drawPlay (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour c)
{
    const auto a = area.reduced (area.getWidth() * 0.16f);
    juce::Path p;
    p.addTriangle (a.getX() + a.getWidth() * 0.1f, a.getY(), a.getRight(), a.getCentreY(), a.getX() + a.getWidth() * 0.1f, a.getBottom());
    g.setColour (c);
    g.fillPath (p);
    g.strokePath (p, roundStroke (area.getWidth() * 0.12f));
}

inline void drawStop (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour c)
{
    g.setColour (c);
    g.fillRoundedRectangle (area.reduced (area.getWidth() * 0.18f), area.getWidth() * 0.12f);
}

inline void drawChevron (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour c, bool pointsLeft)
{
    const auto a = area.reduced (area.getWidth() * 0.26f);
    const float x0 = pointsLeft ? a.getRight() - a.getWidth() * 0.25f : a.getX() + a.getWidth() * 0.25f;
    const float x1 = pointsLeft ? a.getX() + a.getWidth() * 0.15f     : a.getRight() - a.getWidth() * 0.15f;

    juce::Path p;
    p.startNewSubPath (x0, a.getY());
    p.lineTo (x1, a.getCentreY());
    p.lineTo (x0, a.getBottom());
    g.setColour (c);
    g.strokePath (p, roundStroke (area.getWidth() * 0.14f));
}

// Achtelnote
inline void drawNote (juce::Graphics& g, float x, float y, float size, juce::Colour c)
{
    juce::Path head;
    head.addEllipse (-0.34f * size, -0.22f * size, 0.68f * size, 0.46f * size);
    head.applyTransform (juce::AffineTransform::rotation (-0.35f).translated (x, y));

    juce::Path stem;
    stem.startNewSubPath (x + 0.3f * size, y - 0.05f * size);
    stem.lineTo (x + 0.3f * size, y - 1.05f * size);
    stem.quadraticTo (x + 0.75f * size, y - 0.95f * size, x + 0.7f * size, y - 0.5f * size);

    g.setColour (c);
    g.fillPath (head);
    g.strokePath (stem, roundStroke (0.13f * size));
}

} // namespace theme
