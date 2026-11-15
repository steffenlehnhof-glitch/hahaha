#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ChordEngine.h"

// Farben, Schrift und kleine Zeichen-Helfer fuer das Overlay
namespace theme
{
const juce::Colour bg       { 0xff110e1b };
const juce::Colour bgHi     { 0xff191530 };
const juce::Colour panel    { 0xff1b1730 };
const juce::Colour panelHi  { 0xff262043 };
const juce::Colour line     { 0xff3a3163 };
const juce::Colour text     { 0xfff3eeff };
const juce::Colour textDim  { 0xffa399c9 };
const juce::Colour ink      { 0xff1d1633 };

const juce::Colour pink     { 0xffff5fa2 };
const juce::Colour mint     { 0xff6ef2c4 };
const juce::Colour gold     { 0xffffd166 };
const juce::Colour violet   { 0xffa98bff };
const juce::Colour amber    { 0xffffa94d };
const juce::Colour coral    { 0xffff7b7b };
const juce::Colour sky      { 0xff6cc4ff };

inline juce::Colour styleAccent (int style)
{
    switch (style)
    {
        case purr::kTrap:   return juce::Colour (0xffb86bff);
        case purr::kHipHop: return amber;
        case purr::kLoFi:   return mint;
        case purr::kJazz:   return sky;
        default:            return pink;
    }
}

inline juce::Colour qualityColour (int quality)
{
    switch (quality)
    {
        case purr::kQMinor:    return violet;
        case purr::kQDominant: return amber;
        case purr::kQTense:    return coral;
        default:               return mint;
    }
}

inline juce::Font font (float height, bool bold = false)
{
   #if JUCE_MAJOR_VERSION >= 8
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
   #else
    return juce::Font (height, bold ? juce::Font::bold : juce::Font::plain);
   #endif
}

// Quelltext ist UTF-8; so kommen Umlaute sicher in juce::String an
inline juce::String T8 (const char* utf8) { return juce::String::fromUTF8 (utf8); }

inline juce::PathStrokeType roundStroke (float thickness)
{
    return juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
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
