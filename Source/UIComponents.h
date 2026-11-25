#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "MeatGrinderEngine.h"
#include "MeatLookAndFeel.h"

namespace mg::ui
{

// ---------------------------------------------------------------------------
// Stereo segmented LED peak meter with latched clip indicator (click to clear).
// ---------------------------------------------------------------------------
class MeterComponent : public juce::Component
{
public:
    MeterComponent (std::atomic<float>& peakL, std::atomic<float>& peakR, std::atomic<bool>& clip, juce::String titleText)
        : src { &peakL, &peakR }, clipFlag (&clip), title (std::move (titleText))
    {
    }

    // Called by the editor's timer (~30 Hz): pulls the peaks written by the audio thread.
    void pull()
    {
        for (int c = 0; c < 2; ++c)
        {
            const float pk = src[(size_t) c]->exchange (0.0f);
            const float d = juce::Decibels::gainToDecibels (pk, -90.0f);
            levelDb[c] = d > levelDb[c] ? d : juce::jmax (d, levelDb[c] - 2.2f);   // ~66 dB/s fall

            if (d >= holdDb[c])        { holdDb[c] = d; holdFrames[c] = 40; }
            else if (holdFrames[c] > 0) --holdFrames[c];
            else                        holdDb[c] = juce::jmax (d, holdDb[c] - 0.8f);
        }
        if (clipFlag->exchange (false))
            clipLatched = true;
        repaint();
    }

    void mouseDown (const juce::MouseEvent&) override { clipLatched = false; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        constexpr float topPad = 16.0f, bottomPad = 22.0f;
        const float barTop = topPad, barH = b.getHeight() - topPad - bottomPad;
        const float barW = 12.0f;
        const float xL = 2.0f, xR = b.getWidth() - 2.0f - barW;
        constexpr int nSeg = 48;
        const float segH = barH / (float) nSeg;

        // clip lamp
        const auto lamp = juce::Rectangle<float> (2.0f, 0.0f, b.getWidth() - 4.0f, 11.0f);
        if (clipLatched)
        {
            g.setColour (colour::amber.withAlpha (0.35f));
            g.fillRoundedRectangle (lamp.expanded (2.0f), 4.0f);
            g.setColour (juce::Colour (0xffff3b30));
        }
        else
        {
            g.setColour (colour::groove.brighter (0.2f));
        }
        g.fillRoundedRectangle (lamp, 3.0f);

        auto segColour = [] (float segDb, float frac)
        {
            if (segDb > 0.0f)   return juce::Colour (0xffff3b30);
            if (segDb > -9.0f)  return colour::amber;
            return colour::pink.interpolatedWith (colour::crimson, juce::jlimit (0.0f, 1.0f, frac * 1.4f));
        };

        for (int c = 0; c < 2; ++c)
        {
            const float x = c == 0 ? xL : xR;
            const int holdSeg = juce::jlimit (0, nSeg - 1, (int) ((holdDb[c] + 60.0f) / 66.0f * (float) nSeg));
            for (int i = 0; i < nSeg; ++i)
            {
                const float segDb = -60.0f + ((float) i + 0.5f) * 66.0f / (float) nSeg;
                const float y = barTop + barH - (float) (i + 1) * segH;
                const bool lit = levelDb[c] >= segDb - 0.01f;
                g.setColour (lit ? segColour (segDb, (float) i / (float) nSeg) : colour::groove.brighter (0.2f));
                g.fillRect (x, y + 0.6f, barW, segH - 1.2f);
                if (i == holdSeg && holdDb[c] > -58.0f)
                {
                    g.setColour (colour::bone);
                    g.fillRect (x, y + 0.6f, barW, segH - 1.2f);
                }
            }
        }

        // dB scale between the bars
        g.setFont (Fonts::body (12.0f));
        g.setColour (colour::boneDim);
        for (int db : { 0, -6, -12, -24, -48 })
        {
            const float y = barTop + barH * (1.0f - ((float) db + 60.0f) / 66.0f);
            g.drawText (juce::String (db), juce::Rectangle<float> (xL + barW, y - 6.0f, xR - xL - barW, 12.0f),
                        juce::Justification::centred, false);
        }

        g.setColour (colour::bone);
        g.setFont (Fonts::body (17.0f));
        g.drawText (title, juce::Rectangle<float> (0.0f, b.getHeight() - bottomPad + 3.0f, b.getWidth(), 18.0f),
                    juce::Justification::centred, false);
    }

private:
    std::array<std::atomic<float>*, 2> src;
    std::atomic<bool>* clipFlag;
    float levelDb[2] { -90.0f, -90.0f }, holdDb[2] { -90.0f, -90.0f };
    int holdFrames[2] { 0, 0 };
    bool clipLatched = false;
    juce::String title;
};

// ---------------------------------------------------------------------------
// Draws the actual waveshaper transfer curve for the current FATNESS/COLOR/DRIVE.
// ---------------------------------------------------------------------------
class CurveView : public juce::Component
{
public:
    void setParams (float f, float c, float driveDb, bool hard)
    {
        if (! juce::exactlyEqual (f, fat) || ! juce::exactlyEqual (c, col) || ! juce::exactlyEqual (driveDb, drv) || hard != hardClip)
        {
            fat = f; col = c; drv = driveDb; hardClip = hard;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto box = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (colour::groove);
        g.fillRoundedRectangle (box, 5.0f);

        const float cx = box.getCentreX(), cy = box.getCentreY();
        g.setColour (colour::plate.brighter (0.15f));
        g.drawLine (box.getX(), cy, box.getRight(), cy, 1.0f);
        g.drawLine (cx, box.getY(), cx, box.getBottom(), 1.0f);

        // unity line
        g.setColour (colour::boneDim.withAlpha (0.35f));
        const float k = 1.0f / 1.15f;
        g.drawLine (box.getX(), cy + box.getHeight() * 0.5f * k, box.getRight(), cy - box.getHeight() * 0.5f * k, 1.0f);

        juce::Path p;
        constexpr int n = 96;
        for (int i = 0; i <= n; ++i)
        {
            const float x = -1.0f + 2.0f * (float) i / (float) n;
            const float y = juce::jlimit (-1.15f, 1.15f, mg::MeatGrinderEngine::transferCurve (x, fat, col, drv, hardClip));
            const float px = box.getX() + (x + 1.0f) * 0.5f * box.getWidth();
            const float py = cy - y / 1.15f * box.getHeight() * 0.5f;
            if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
        }
        g.setGradientFill (juce::ColourGradient (colour::crimson, box.getX(), box.getBottom(),
                                                 colour::pink, box.getRight(), box.getY(), false));
        g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (colour::groove.brighter (0.2f));
        g.drawRoundedRectangle (box, 5.0f, 1.5f);
    }

private:
    float fat = -1, col = -1, drv = -1;
    bool hardClip = false;
};

// ---------------------------------------------------------------------------
// Segmented selector bound to a choice/bool parameter.
// ---------------------------------------------------------------------------
class SegmentedChoice : public juce::Component
{
public:
    SegmentedChoice (juce::RangedAudioParameter& param, juce::StringArray labelsIn)
        : labels (std::move (labelsIn)),
          attachment (param, [this] (float v) { selected = (int) std::lround (v); repaint(); })
    {
        attachment.sendInitialUpdate();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int n = labels.size();
        const int idx = juce::jlimit (0, n - 1, (int) (e.position.x / ((float) getWidth() / (float) n)));
        attachment.setValueAsCompleteGesture ((float) idx);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        const int n = labels.size();
        const float segW = r.getWidth() / (float) n;

        g.setColour (colour::groove);
        g.fillRoundedRectangle (r, 5.0f);

        for (int i = 0; i < n; ++i)
        {
            const auto seg = juce::Rectangle<float> (r.getX() + segW * (float) i, r.getY(), segW, r.getHeight()).reduced (2.5f);
            const bool on = i == selected;
            if (on)
            {
                g.setGradientFill (juce::ColourGradient (colour::pink, seg.getX(), seg.getY(),
                                                         colour::crimson, seg.getX(), seg.getBottom(), false));
                g.fillRoundedRectangle (seg, 3.5f);
            }
            g.setColour (on ? colour::groove : colour::boneDim);
            g.setFont (Fonts::body (18.0f));
            g.drawText (labels[i], seg.toNearestInt(), juce::Justification::centred, false);
        }

        g.setColour (colour::groove.brighter (0.2f));
        g.drawRoundedRectangle (r, 5.0f, 1.5f);
    }

private:
    juce::StringArray labels;
    int selected = 0;
    juce::ParameterAttachment attachment;
};

} // namespace mg::ui
