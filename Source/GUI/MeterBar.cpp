#include "MeterBar.h"
#include "UIStyle.h"
#include <cmath>

namespace onirique::ui
{

namespace
{
    constexpr int timerHz = 30;
    constexpr float releaseTauSeconds = 0.3f;
    constexpr int peakHoldTicks = (500 * timerHz) / 1000;
    constexpr float repaintEpsilonDb = 0.1f;
}

MeterBar::MeterBar (Orientation orientation, float dispMinDb, float dispMaxDb, bool fillFromNearEnd,
                    std::vector<std::pair<float, juce::String>> ticks, juce::Colour fillColour)
    : orientationKind (orientation), dispMin (dispMinDb), dispMax (dispMaxDb),
      fillFromNear (fillFromNearEnd), tickList (std::move (ticks)), fillColourValue (fillColour)
{
    setPaintingIsUnclipped (true);   // les graduations se dessinent legerement hors des bounds du tube
    setValueDb (dispMin);
    currentDb = peakDb = dispMin;
    startTimerHz (timerHz);
}

MeterBar::~MeterBar() { stopTimer(); }

void MeterBar::setValueDb (float newDb) noexcept
{
    targetDb = juce::jlimit (juce::jmin (dispMin, dispMax), juce::jmax (dispMin, dispMax), newDb);
}

void MeterBar::enableThresholdMarker (std::function<void (float)> callback)
{
    hasThreshold = true;
    onThresholdChanged = std::move (callback);
}

void MeterBar::setThresholdDb (float db) noexcept
{
    thresholdDbValue = db;
    repaint();
}

float MeterBar::dbToFraction (float db) const noexcept
{
    if (juce::approximatelyEqual (dispMax, dispMin))
        return 0.0f;
    return juce::jlimit (0.0f, 1.0f, (db - dispMin) / (dispMax - dispMin));
}

float MeterBar::fractionToDb (float frac) const noexcept
{
    return dispMin + juce::jlimit (0.0f, 1.0f, frac) * (dispMax - dispMin);
}

float MeterBar::positionToFraction (juce::Point<float> p) const noexcept
{
    const auto b = getLocalBounds().toFloat();
    if (orientationKind == Orientation::vertical)
        return (p.y - b.getY()) / juce::jmax (1.0f, b.getHeight());
    return (p.x - b.getX()) / juce::jmax (1.0f, b.getWidth());
}

void MeterBar::mouseDown (const juce::MouseEvent& e)
{
    if (! hasThreshold)
        return;

    thresholdDbValue = fractionToDb (positionToFraction (e.position));
    if (onThresholdChanged != nullptr)
        onThresholdChanged (thresholdDbValue);
    repaint();
}

void MeterBar::mouseDrag (const juce::MouseEvent& e)
{
    mouseDown (e);
}

void MeterBar::timerCallback()
{
    constexpr float dt = 1.0f / (float) timerHz;

    if (targetDb > currentDb)
    {
        currentDb = targetDb;
    }
    else
    {
        const float coeff = std::exp (-dt / releaseTauSeconds);
        currentDb = targetDb + coeff * (currentDb - targetDb);
    }

    if (targetDb >= peakDb)
    {
        peakDb = targetDb;
        peakHoldCounter = peakHoldTicks;
    }
    else if (peakHoldCounter > 0)
    {
        --peakHoldCounter;
    }
    else
    {
        peakDb = juce::jmax (currentDb, peakDb - 0.4f);
    }

    if (std::abs (currentDb - lastPaintedDb) > repaintEpsilonDb)
    {
        lastPaintedDb = currentDb;
        repaint();
    }
}

void MeterBar::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const bool vertical = orientationKind == Orientation::vertical;
    // unite = epaisseur du tube / 14 (le tube fait 14 unites maquette d'epaisseur partout)
    const float u = (vertical ? bounds.getWidth() : bounds.getHeight()) / 14.0f;
    const float rad = (vertical ? bounds.getWidth() : bounds.getHeight()) * 0.5f;

    // --- Tube sombre a lisere fin ------------------------------------------
    g.setColour (juce::Colours::black.withAlpha (0.42f));
    g.fillRoundedRectangle (bounds, rad);
    g.setColour (line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), rad, std::max (1.0f, 0.8f * u));

    const auto track = bounds.reduced (2.5f * u);
    const float trackRad = (vertical ? track.getWidth() : track.getHeight()) * 0.5f;

    // --- Fraction remplie ---------------------------------------------------
    const float valueFrac = dbToFraction (currentDb);
    const float fillStartFrac = fillFromNear ? 0.0f : valueFrac;
    const float fillEndFrac   = fillFromNear ? valueFrac : 1.0f;

    juce::Rectangle<float> fillArea;
    if (vertical)
    {
        const float y0 = track.getY() + track.getHeight() * fillStartFrac;
        const float y1 = track.getY() + track.getHeight() * fillEndFrac;
        fillArea = { track.getX(), y0, track.getWidth(), juce::jmax (0.0f, y1 - y0) };
    }
    else
    {
        const float x0 = track.getX() + track.getWidth() * fillStartFrac;
        const float x1 = track.getX() + track.getWidth() * fillEndFrac;
        fillArea = { x0, track.getY(), juce::jmax (0.0f, x1 - x0), track.getHeight() };
    }

    if (fillArea.getWidth() > 0.1f && fillArea.getHeight() > 0.1f)
    {
        // degrade os (cote fixe) -> braise (cote valeur), avec halo braise
        juce::Point<float> from, to;
        if (vertical)
        {
            from = { fillArea.getCentreX(), fillFromNear ? fillArea.getY() : fillArea.getBottom() };
            to   = { fillArea.getCentreX(), fillFromNear ? fillArea.getBottom() : fillArea.getY() };
        }
        else
        {
            from = { fillFromNear ? fillArea.getX() : fillArea.getRight(), fillArea.getCentreY() };
            to   = { fillFromNear ? fillArea.getRight() : fillArea.getX(), fillArea.getCentreY() };
        }

        juce::Path fp;
        fp.addRoundedRectangle (fillArea, trackRad);
        g.setColour (ember.withAlpha (0.22f));
        g.fillRoundedRectangle (fillArea.expanded (1.5f * u), trackRad + 1.5f * u);
        g.setGradientFill (juce::ColourGradient (fillFromNear ? bone : ember.darker (0.2f), from,
                                                 fillFromNear ? ember : bone, to, false));
        g.fillPath (fp);
    }

    // --- Graduations ---------------------------------------------------------
    g.setFont (font (Face::mono, std::max (6.5f, 6.0f * u)));
    for (auto& tick : tickList)
    {
        const float frac = dbToFraction (tick.first);
        g.setColour (haze.withAlpha (0.55f));
        if (vertical)
        {
            const float yy = track.getY() + track.getHeight() * frac;
            g.fillRect (juce::Rectangle<float> (bounds.getRight() + 1.5f * u, yy - 0.5f, 2.5f * u, 1.0f));
            g.setColour (haze.withAlpha (0.85f));
            g.drawText (tick.second, juce::Rectangle<float> (bounds.getRight() + 5.0f * u, yy - 5.0f * u, 24.0f * u, 10.0f * u),
                        juce::Justification::centredLeft, false);
        }
        else
        {
            const float xx = track.getX() + track.getWidth() * frac;
            g.fillRect (juce::Rectangle<float> (xx - 0.5f, bounds.getBottom() + 1.5f * u, 1.0f, 2.5f * u));
            g.setColour (haze.withAlpha (0.85f));
            g.drawText (tick.second, juce::Rectangle<float> (xx - 12.0f * u, bounds.getBottom() + 4.5f * u, 24.0f * u, 10.0f * u),
                        juce::Justification::centred, false);
        }
    }

    // --- Marqueur de seuil deplacable : trait os avec lueur braise ---------
    if (hasThreshold)
    {
        const float frac = dbToFraction (thresholdDbValue);
        juce::Rectangle<float> mark, halo;
        if (vertical)
        {
            const float yy = track.getY() + track.getHeight() * frac;
            mark = { bounds.getX() + 1.0f, yy - 1.0f * u, bounds.getWidth() - 2.0f, 2.0f * u };
            halo = mark.expanded (0.0f, 2.5f * u);
        }
        else
        {
            const float xx = track.getX() + track.getWidth() * frac;
            mark = { xx - 1.0f * u, bounds.getY() + 1.0f, 2.0f * u, bounds.getHeight() - 2.0f };
            halo = mark.expanded (2.5f * u, 0.0f);
        }
        g.setColour (ember.withAlpha (0.45f));
        g.fillRoundedRectangle (halo, 2.5f * u);
        g.setColour (bone);
        g.fillRoundedRectangle (mark, 1.0f * u);
    }
}

} // namespace onirique::ui
