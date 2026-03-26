#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**  Clean rotary knob — standard 270° arc, 7:30 → 12:00 → 4:30.
 *
 *   Two angle conventions exist and must NOT be mixed:
 *
 *   JUCE addArc   θ = 0 → 12 o'clock, increases clockwise.
 *                 Point: x = cx + r·sin(θ),  y = cy − r·cos(θ)
 *
 *   C++ cos/sin   θ = 0 → 3 o'clock, increases clockwise (Y-down screen).
 *                 Point: x = cx + r·cos(θ),  y = cy + r·sin(θ)
 *
 *   Relationship: juce_θ = ptr_θ + π/2
 *
 *   Position     JUCE addArc   cos/sin (ptr)
 *   ----------   -----------   -------------
 *   7:30 (min)   5π/4          3π/4
 *   12:00 (mid)  0  (= 2π)     3π/2
 *   4:30 (max)   11π/4(=3π/4+2π) 9π/4(=π/4+2π)
 *
 *   So min-at-7:30, center-at-12, max-at-4:30.
 *   For a range symmetric around 0 (e.g. ±12) value 0 is at 12 o'clock.
 *   For a unipolar range (0–1, 0–100) the midpoint lands at 12 o'clock.
 */
class FancyKnob : public juce::Slider
{
public:
    FancyKnob() : juce::Slider()
    {
        setSliderStyle (RotaryVerticalDrag);
        setTextBoxStyle (NoTextBox, false, 0, 0);
        setPopupDisplayEnabled (true, false, nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        const auto  bounds = getLocalBounds().toFloat().reduced (3.0f);
        const float cx     = bounds.getCentreX();
        const float cy     = bounds.getCentreY();
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;

        // ── Arc geometry ────────────────────────────────────────────────────
        //  270° sweep, 7:30 (min) → clockwise via 9 → 12 → 3 → 4:30 (max)
        constexpr float kTotal    = juce::MathConstants<float>::pi * 1.5f;    // 270°

        // JUCE addArc angles (0 = 12 o'clock, CW)
        constexpr float kArcStart = juce::MathConstants<float>::pi * 1.25f;   // 7:30

        // cos/sin pointer angles (0 = 3 o'clock, CW in Y-down screen)
        constexpr float kPtrStart = juce::MathConstants<float>::pi * 0.75f;   // 7:30

        // Normalised value [0 … 1]
        const double mn = getMinimum(), mx = getMaximum();
        const float  t  = (mx > mn) ? (float)((getValue() - mn) / (mx - mn)) : 0.0f;

        // Current angle in each convention
        const float arcAngle = kArcStart + t * kTotal;   // for addArc
        const float ptrAngle = kPtrStart + t * kTotal;   // for cos/sin

        const float arcR = radius * 0.80f;

        // ── Background track ─────────────────────────────────────────────────
        juce::Path track;
        track.addArc (cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f,
                      kArcStart, kArcStart + kTotal, true);
        g.setColour (juce::Colour::fromRGB (38, 38, 50));
        g.strokePath (track, juce::PathStrokeType (3.5f,
                      juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // ── Value arc (filled portion) — always drawn; zero-length at t=0 ────
        {
            juce::Path filled;
            filled.addArc (cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f,
                           kArcStart, arcAngle, true);

            // Cyan (top) → neon-green (bottom)
            juce::ColourGradient grad (juce::Colour::fromString ("0xff00d4ff"), cx, cy - arcR,
                                       juce::Colour::fromString ("0xff00ff88"), cx, cy + arcR, false);
            g.setGradientFill (grad);
            g.strokePath (filled, juce::PathStrokeType (3.5f,
                          juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // ── Knob body ─────────────────────────────────────────────────────────
        const float knobR = radius * 0.60f;

        // Rim
        g.setColour (juce::Colour::fromRGB (55, 55, 68));
        g.fillEllipse (cx - knobR, cy - knobR, knobR * 2.0f, knobR * 2.0f);

        // Face (top-lit gradient)
        juce::ColourGradient face (juce::Colour::fromRGB (48, 48, 60), cx, cy - knobR * 0.6f,
                                   juce::Colour::fromRGB (20, 20, 28), cx, cy + knobR, false);
        g.setGradientFill (face);
        g.fillEllipse (cx - knobR + 1.5f, cy - knobR + 1.5f,
                       knobR * 2.0f - 3.0f, knobR * 2.0f - 3.0f);

        // ── Pointer (cos/sin convention: 0 = 3 o'clock, CW in Y-down) ──────
        const float pInner = knobR * 0.22f;
        const float pOuter = knobR * 0.82f;
        const float px1 = cx + pInner * std::cos (ptrAngle);
        const float py1 = cy + pInner * std::sin (ptrAngle);
        const float px2 = cx + pOuter * std::cos (ptrAngle);
        const float py2 = cy + pOuter * std::sin (ptrAngle);

        // Shadow
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawLine (px1 + 0.8f, py1 + 0.8f, px2 + 0.8f, py2 + 0.8f, 2.0f);

        // Line
        g.setColour (juce::Colours::white.withAlpha (0.88f));
        g.drawLine (px1, py1, px2, py2, 2.0f);

        // Tip dot
        g.setColour (juce::Colour::fromString ("0xff00d4ff"));
        g.fillEllipse (px2 - 2.5f, py2 - 2.5f, 5.0f, 5.0f);
    }
};
