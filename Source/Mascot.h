#pragma once

#include "Theme.h"

// =============================================================================
//  Moll - die Katze mit Kopfhoerern, jetzt als Nachtkatze.
//  Komplett aus Vektorpfaden gezeichnet (keine Bilddateien noetig). Sie nickt
//  im Takt, blinzelt, wedelt mit dem Schwanz, von den Kopfhoerern tropft Lila.
// =============================================================================
class CatComponent : public juce::Component
{
public:
    void setAccent (juce::Colour c)                 { accent = c; }
    void setSubtitle (const juce::String& s)        { subtitle = s; }

    void say (const juce::String& s)
    {
        speech = s;
        pop = 1.0f;
        repaint();
    }

    void excite()                                   { excitement = 1.0f; }

    void advance (float dt, bool isPlaying, float beat)
    {
        time += dt;
        playing = isPlaying;
        bob = isPlaying ? (1.0f - beat) * (1.0f - beat) : 0.0f;
        excitement = juce::jmax (0.0f, excitement - dt * 2.4f);
        pop = juce::jmax (0.0f, pop - dt * 4.0f);

        nextBlink -= dt;
        blink = juce::jmax (0.0f, blink - dt);
        if (nextBlink <= 0.0f)
        {
            blink = 0.13f;
            nextBlink = 2.2f + 3.5f * random.nextFloat();
        }

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();

        g.setColour (theme::panel);
        g.fillRoundedRectangle (area, 16.0f);
        g.setColour (theme::line.withAlpha (0.6f));
        g.drawRoundedRectangle (area.reduced (0.5f), 16.0f, 1.0f);

        area = area.reduced (12.0f);

        // ---- Sprechblase -----------------------------------------------------
        auto bubbleArea = area.removeFromTop (96.0f);
        auto bubble = bubbleArea.withTrimmedBottom (12.0f);
        bubble = bubble.expanded (2.0f * pop, 1.5f * pop);

        juce::Path tail;
        const float tx = bubble.getCentreX() - 18.0f;
        tail.addTriangle (tx, bubble.getBottom() - 2.0f, tx + 20.0f, bubble.getBottom() - 2.0f, tx + 14.0f, bubble.getBottom() + 11.0f);

        g.setColour (theme::panelHi.brighter (0.05f + 0.12f * pop));
        g.fillRoundedRectangle (bubble, 14.0f);
        g.fillPath (tail);
        g.setColour (accent.withAlpha (0.45f + 0.4f * pop));
        g.drawRoundedRectangle (bubble, 14.0f, 1.2f);

        g.setColour (theme::text);
        g.setFont (theme::font (14.0f));
        g.drawFittedText (speech, bubble.reduced (12.0f, 6.0f).toNearestInt(), juce::Justification::centred, 4, 0.85f);

        // ---- Namensschild ----------------------------------------------------
        auto plate = area.removeFromBottom (40.0f);
        g.setColour (theme::text);
        g.setFont (theme::font (15.0f, true));
        g.drawText ("MOLL", plate.removeFromTop (20.0f).toNearestInt(), juce::Justification::centred, false);
        g.setColour (theme::textDim);
        g.setFont (theme::font (12.0f));
        g.drawText (subtitle, plate.toNearestInt(), juce::Justification::centred, false);

        // ---- Katze (Zeichenraum 200 x 216) ------------------------------------
        const float scale = juce::jmin (area.getWidth() / 200.0f, area.getHeight() / 216.0f);
        const float ox = area.getCentreX() - 100.0f * scale;
        const float oy = area.getCentreY() - 108.0f * scale;

        juce::Graphics::ScopedSaveState state (g);
        g.addTransform (juce::AffineTransform::scale (scale).translated (ox, oy));
        drawCat (g);
    }

private:
    void drawCat (juce::Graphics& g) const
    {
        const juce::Colour fur      (0xff3d2d68);
        const juce::Colour furLight (0xff5d489a);
        const juce::Colour furDark  (0xff22183f);
        const juce::Colour pale     (0xffe9d5ff);
        const juce::Colour dark     = theme::ink;
        const float pi = juce::MathConstants<float>::pi;

        const float hop  = std::sin (pi * excitement) * 9.0f;
        const float bobY = playing ? 5.0f * bob : 1.4f * std::sin (time * 1.6f);
        const float tilt = playing ? 0.05f * std::sin (time * 2.4f) : 0.015f * std::sin (time * 0.9f);

        // Leuchten hinter der Katze
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.42f), 100.0f, 112.0f,
                                                 accent.withAlpha (0.0f), 100.0f, 218.0f, true));
        g.fillEllipse (-6.0f, 6.0f, 212.0f, 212.0f);

        // Pfuetze aus Lila unter der Katze
        g.setColour (theme::purpleDark);
        g.fillEllipse (30.0f, 187.0f, 140.0f, 20.0f);
        g.setColour (theme::purpleDeep);
        g.fillEllipse (38.0f, 188.0f, 124.0f, 15.0f);
        g.setColour (theme::purpleHi.withAlpha (0.35f));
        g.fillEllipse (52.0f, 190.0f, 34.0f, 4.0f);

        g.addTransform (juce::AffineTransform::translation (0.0f, -hop));

        // Schwanz
        {
            const float sway = (playing ? 9.0f : 5.0f) * std::sin (time * (playing ? 4.2f : 1.5f));
            juce::Path tail;
            tail.startNewSubPath (134.0f, 178.0f);
            tail.cubicTo (176.0f, 186.0f, 190.0f, 152.0f, 172.0f + sway, 128.0f);
            g.setColour (furDark);
            g.strokePath (tail, theme::roundStroke (13.0f));
            g.setColour (fur);
            g.strokePath (tail, theme::roundStroke (9.0f));
        }

        // Koerper + Bauch
        g.setColour (fur);
        g.fillEllipse (58.0f, 112.0f, 84.0f, 84.0f);
        g.setColour (furLight.withAlpha (0.9f));
        g.fillEllipse (77.0f, 136.0f, 46.0f, 52.0f);

        // Pfoten (die linke tippt im Takt)
        const float tap = playing ? 4.0f * bob : 0.0f;
        g.setColour (furLight.brighter (0.25f));
        g.fillEllipse (68.0f, 182.0f - tap, 26.0f, 14.0f);
        g.fillEllipse (106.0f, 182.0f, 26.0f, 14.0f);

        // ---- Kopf (nickt und neigt sich) ----
        {
            juce::Graphics::ScopedSaveState headState (g);
            g.addTransform (juce::AffineTransform::rotation (tilt, 100.0f, 125.0f).translated (0.0f, bobY));

            for (int side = -1; side <= 1; side += 2)
            {
                auto X = [side] (float x) { return 100.0f - (float) side * (x - 100.0f); };
                juce::Path ear;
                ear.addTriangle (X (50.0f), 66.0f, X (58.0f), 20.0f, X (92.0f), 44.0f);
                g.setColour (fur);
                g.fillPath (ear);
                g.strokePath (ear, theme::roundStroke (6.0f));

                juce::Path inner;
                inner.addTriangle (X (60.0f), 56.0f, X (64.0f), 34.0f, X (80.0f), 46.0f);
                g.setColour (theme::magenta.withAlpha (0.55f));
                g.fillPath (inner);
            }

            g.setColour (fur);
            g.fillEllipse (42.0f, 38.0f, 116.0f, 92.0f);

            // Stirnstreifen
            juce::Path stripes;
            stripes.startNewSubPath (100.0f, 43.0f); stripes.lineTo (100.0f, 57.0f);
            stripes.startNewSubPath (88.0f, 45.0f);  stripes.lineTo (91.0f, 56.0f);
            stripes.startNewSubPath (112.0f, 45.0f); stripes.lineTo (109.0f, 56.0f);
            g.setColour (furDark);
            g.strokePath (stripes, theme::roundStroke (4.5f));

            // Kopfhoerer: Buegel
            juce::Path band;
            band.startNewSubPath (38.0f, 84.0f);
            band.cubicTo (36.0f, 6.0f, 164.0f, 6.0f, 162.0f, 84.0f);
            g.setColour (dark);
            g.strokePath (band, theme::roundStroke (10.0f));
            g.setColour (theme::purple);
            g.strokePath (band, theme::roundStroke (5.5f));

            // Kopfhoerer: Muscheln, von denen es tropft
            const float led = playing ? 0.55f + 0.45f * bob : 0.8f;
            for (int side = 0; side < 2; ++side)
            {
                const juce::Rectangle<float> cup (side == 0 ? 24.0f : 150.0f, 68.0f, 26.0f, 46.0f);

                // Tropfen an der Unterkante
                const std::vector<theme::Drip> drips = side == 0
                    ? std::vector<theme::Drip> { { cup.getX() + 8.0f, 6.0f, 15.0f }, { cup.getX() + 18.5f, 5.0f, 8.0f } }
                    : std::vector<theme::Drip> { { cup.getX() + 7.5f, 5.0f, 9.0f }, { cup.getX() + 18.0f, 6.5f, 19.0f } };
                theme::drawDrips (g, cup.getX() + 2.0f, cup.getRight() - 2.0f, cup.getBottom() - 8.0f, cup.getBottom() - 1.0f,
                                  drips, theme::purple, true);

                // einer faellt
                float fall = time * 0.55f + (float) side * 0.5f;
                fall -= std::floor (fall);
                const float dx = side == 0 ? cup.getX() + 8.0f : cup.getX() + 18.0f;
                theme::drawDroplet (g, dx, cup.getBottom() + 22.0f + 46.0f * fall * fall, 4.2f,
                                    theme::purple.withAlpha (1.0f - fall));

                g.setColour (dark);
                g.fillRoundedRectangle (cup.expanded (2.5f), 13.0f);
                g.setGradientFill (juce::ColourGradient (theme::purpleHi, cup.getX(), cup.getY(),
                                                         theme::purpleDeep, cup.getRight(), cup.getBottom(), false));
                g.fillRoundedRectangle (cup, 10.5f);
                g.setColour (theme::purpleDark);
                g.fillRoundedRectangle (cup.reduced (5.0f, 8.0f), 7.0f);
                g.setColour (pale.withAlpha (led));
                g.fillEllipse (cup.getCentreX() - 4.0f, cup.getCentreY() - 4.0f, 8.0f, 8.0f);
            }

            // Augen
            for (float cx : { 78.0f, 122.0f })
            {
                if (playing)
                {
                    juce::Path e;   // gluecklich zugekniffen
                    e.startNewSubPath (cx - 10.0f, 94.0f);
                    e.quadraticTo (cx, 80.0f, cx + 10.0f, 94.0f);
                    g.setColour (pale);
                    g.strokePath (e, theme::roundStroke (4.2f));
                }
                else if (blink > 0.0f)
                {
                    juce::Path e;
                    e.startNewSubPath (cx - 10.0f, 91.0f);
                    e.lineTo (cx + 10.0f, 91.0f);
                    g.setColour (pale);
                    g.strokePath (e, theme::roundStroke (4.0f));
                }
                else
                {
                    // leuchtende Augen mit Schlitzpupille
                    const float s = 1.0f + 0.18f * excitement;
                    g.setColour (accent.withAlpha (0.35f));
                    g.fillEllipse (cx - 14.0f * s, 90.0f - 16.0f * s, 28.0f * s, 32.0f * s);
                    g.setColour (pale);
                    g.fillEllipse (cx - 10.0f * s, 90.0f - 12.0f * s, 20.0f * s, 24.0f * s);
                    g.setColour (accent.brighter (0.2f).withAlpha (0.55f));
                    g.fillEllipse (cx - 7.5f * s, 90.0f - 9.5f * s, 15.0f * s, 19.0f * s);
                    g.setColour (dark);
                    g.fillEllipse (cx - 3.0f - 1.5f * excitement, 90.0f - 9.0f * s, 6.0f + 3.0f * excitement, 18.0f * s);
                    g.setColour (juce::Colours::white);
                    g.fillEllipse (cx - 6.5f, 82.0f, 5.5f, 5.5f);
                }
            }

            // Wangen
            g.setColour (theme::magenta.withAlpha (0.22f));
            g.fillEllipse (55.0f, 102.0f, 17.0f, 8.0f);
            g.fillEllipse (128.0f, 102.0f, 17.0f, 8.0f);

            // Nase + Mund + Eckzahn
            juce::Path nose;
            nose.addTriangle (95.5f, 104.0f, 104.5f, 104.0f, 100.0f, 109.5f);
            g.setColour (theme::magenta);
            g.fillPath (nose);
            g.strokePath (nose, theme::roundStroke (2.0f));

            juce::Path mouth;
            mouth.startNewSubPath (89.0f, 114.0f);
            mouth.quadraticTo (94.5f, 122.0f, 100.0f, 113.0f);
            mouth.quadraticTo (105.5f, 122.0f, 111.0f, 114.0f);
            g.setColour (pale);
            g.strokePath (mouth, theme::roundStroke (2.3f));

            juce::Path fang;
            fang.addTriangle (104.0f, 118.5f, 108.5f, 117.0f, 107.0f, 123.5f);
            g.setColour (juce::Colours::white);
            g.fillPath (fang);

            // Schnurrhaare
            juce::Path whiskers;
            for (int side = -1; side <= 1; side += 2)
            {
                auto X = [side] (float x) { return 100.0f - (float) side * (x - 100.0f); };
                whiskers.startNewSubPath (X (64.0f), 106.0f); whiskers.lineTo (X (45.0f), 102.0f);
                whiskers.startNewSubPath (X (64.0f), 110.0f); whiskers.lineTo (X (44.0f), 111.0f);
                whiskers.startNewSubPath (X (64.0f), 114.0f); whiskers.lineTo (X (46.0f), 120.0f);
            }
            g.setColour (pale.withAlpha (0.8f));
            g.strokePath (whiskers, theme::roundStroke (1.5f));
        }

        // Noten, die beim Abspielen aufsteigen
        if (playing)
        {
            for (int i = 0; i < 3; ++i)
            {
                float ph = time * 0.45f + (float) i * 0.37f;
                ph -= std::floor (ph);
                const float x = 160.0f + 10.0f * std::sin (ph * 6.0f + (float) i * 2.0f) + (float) i * 7.0f;
                const float y = 70.0f - 56.0f * ph;
                theme::drawNote (g, x, y, 10.0f + 2.0f * (float) i, theme::purpleHi.withAlpha (std::sin (pi * ph)));
            }
        }
    }

    juce::Colour accent = theme::purple;
    juce::String speech, subtitle;
    juce::Random random;

    float time = 0.0f, bob = 0.0f, excitement = 0.0f, pop = 0.0f;
    float blink = 0.0f, nextBlink = 2.0f;
    bool playing = false;
};
