#include "TransferCurveDisplay.h"
#include "UIStyle.h"
#include "../DSP/SaturationModule.h"

namespace onirique::ui
{

TransferCurveDisplay::TransferCurveDisplay (juce::AudioProcessorValueTreeState& apvts, const juce::String& moduleID,
                                            juce::Colour accentColour, juce::Colour accentBrightColour)
    : accent (accentColour), accentBright (accentBrightColour)
{
    driveParam = apvts.getRawParameterValue (moduleID + "_drive");
    typeParam  = apvts.getRawParameterValue (moduleID + "_type");
    toneParam  = apvts.getRawParameterValue (moduleID + "_tone");

    startTimerHz (12);
}

TransferCurveDisplay::~TransferCurveDisplay() { stopTimer(); }

void TransferCurveDisplay::timerCallback()
{
    const float drive = driveParam != nullptr ? driveParam->load() : 0.0f;
    const float type  = typeParam  != nullptr ? typeParam->load()  : 0.0f;
    const float tone  = toneParam  != nullptr ? toneParam->load()  : 0.0f;

    if (! juce::approximatelyEqual (drive, lastDrive) || ! juce::approximatelyEqual (type, lastType)
        || ! juce::approximatelyEqual (tone, lastTone))
    {
        lastDrive = drive; lastType = type; lastTone = tone;
        repaint();
    }
}

void TransferCurveDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float u = bounds.getHeight() / 150.0f;
    const float rad = 8.0f * u;

    // --- Ecran (meme rendu que l'afficheur RESONANCE du VocalBox) ------------
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), 0.0f, bounds.getY(),
                                             night.withAlpha (0.85f), 0.0f, bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, rad);
    g.setColour (line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), rad, std::max (1.0f, 1.0f * u));

    const auto plot = bounds.reduced (10.0f * u);

    // --- Grille discrete + diagonale pointillee de reference -----------------
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    for (int i = 1; i < 4; ++i)
    {
        const float t = (float) i / 4.0f;
        g.fillRect (juce::Rectangle<float> (plot.getX() + t * plot.getWidth(), plot.getY(), 1.0f, plot.getHeight()));
        g.fillRect (juce::Rectangle<float> (plot.getX(), plot.getY() + t * plot.getHeight(), plot.getWidth(), 1.0f));
    }

    {
        juce::Path diag;
        diag.startNewSubPath (plot.getX(), plot.getBottom());
        diag.lineTo (plot.getRight(), plot.getY());
        const float dashes[] = { 4.0f * u, 4.0f * u };
        juce::Path dashed;
        juce::PathStrokeType (std::max (1.0f, 1.0f * u)).createDashedStroke (dashed, diag, dashes, 2);
        g.setColour (haze.withAlpha (0.35f));
        g.fillPath (dashed);
    }

    // --- Courbe de transfert reelle (meme fonction que le DSP) ---------------
    const int type = typeParam != nullptr ? (int) typeParam->load() : 2;
    const float driveDb = driveParam != nullptr ? driveParam->load() : 0.0f;
    const float driveGain = juce::Decibels::decibelsToGain (driveDb);

    juce::Path curve, area;
    const int numPoints = 96;
    area.startNewSubPath (plot.getX(), plot.getCentreY());
    for (int i = 0; i <= numPoints; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / (float) numPoints;
        const float y = juce::jlimit (-1.0f, 1.0f, SaturationModule::previewShape (x * driveGain, type));

        const float px = plot.getX() + (x * 0.5f + 0.5f) * plot.getWidth();
        const float py = plot.getBottom() - (y * 0.5f + 0.5f) * plot.getHeight();

        if (i == 0)
            curve.startNewSubPath (px, py);
        else
            curve.lineTo (px, py);
        area.lineTo (px, py);
    }
    area.lineTo (plot.getRight(), plot.getCentreY());
    area.closeSubPath();

    g.setGradientFill (juce::ColourGradient (ember.withAlpha (0.20f), 0.0f, plot.getY(),
                                             ember.withAlpha (0.02f), 0.0f, plot.getCentreY(), false));
    g.fillPath (area);

    const float w = std::max (1.5f, 3.0f * u);
    strokeGlow (g, curve, ember, w, 0.8f);
    g.setGradientFill (juce::ColourGradient (ember, plot.getX(), plot.getBottom(), bone, plot.getRight(), plot.getY(), false));
    g.strokePath (curve, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

} // namespace onirique::ui
