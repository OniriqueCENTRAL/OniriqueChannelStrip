#include "OrderBadge.h"
#include "UIStyle.h"

namespace onirique::ui
{

namespace
{
    constexpr int refreshHz = 12;   // suffisant pour paraitre instantane, cout negligeable
}

OrderBadge::OrderBadge (ModuleOrderManager& managerToUse, juce::String moduleIDToUse, juce::Colour accentColour)
    : orderManager (managerToUse), moduleID (std::move (moduleIDToUse)), accent (accentColour)
{
    setBufferedToImage (false);
    displayedValue = orderManager.getOrder (moduleID);
    startTimerHz (refreshHz);
}

OrderBadge::~OrderBadge() { stopTimer(); }

void OrderBadge::resized()
{
    auto b = getLocalBounds();
    const int arrowW = juce::jmin (juce::roundToInt ((float) b.getHeight() * 0.9f), b.getWidth() / 3);
    upZone    = b.removeFromRight (arrowW).removeFromTop (b.getHeight() / 2);
    downZone  = juce::Rectangle<int> (upZone.getX(), upZone.getBottom(), arrowW, b.getHeight() - upZone.getHeight());
    numberZone = b;
}

void OrderBadge::paint (juce::Graphics& g)
{
    // Pastille sombre a lisere fin (DA ONIRIQUE) : numero de passage dans la
    // chaine en mono clair, chevrons haut/bas qui s'allument en braise au survol.
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    const float u = r.getHeight() / 18.0f;
    const bool over = isMouseOver (true);

    g.setColour (juce::Colours::black.withAlpha (0.34f));
    g.fillRoundedRectangle (r, rad);
    g.setColour (accent.withAlpha (over ? 0.10f : 0.04f));
    g.fillRoundedRectangle (r, rad);
    g.setColour (line.interpolatedWith (ember.withAlpha (0.8f), over ? 0.7f : 0.25f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, std::max (1.0f, 1.0f * u));

    const auto num = numberZone.toFloat().withTrimmedLeft (rad * 0.4f);
    const juce::String txt (displayedValue);
    g.setColour (bone);
    g.setFont (fitFont (Face::mono, r.getHeight() * 0.68f, num.getWidth(), txt, true));
    g.drawText (txt, num, juce::Justification::centred, false);

    auto drawChevron = [&] (juce::Rectangle<int> zone, bool pointingUp)
    {
        const auto z = zone.toFloat();
        const float cx = z.getCentreX() - 1.5f * u;
        const float cy = z.getCentreY() + (pointingUp ? 1.0f : -1.0f) * u;
        const float aw = 2.8f * u, ah = (pointingUp ? -1.6f : 1.6f) * u;
        juce::Path ch;
        ch.startNewSubPath (cx - aw, cy - ah * 0.5f);
        ch.lineTo (cx, cy + ah * 0.5f);
        ch.lineTo (cx + aw, cy - ah * 0.5f);
        g.setColour (haze.interpolatedWith (ember, over ? 0.8f : 0.0f));
        g.strokePath (ch, juce::PathStrokeType (std::max (1.0f, 1.2f * u), juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    };

    drawChevron (upZone, true);
    drawChevron (downZone, false);
}

void OrderBadge::mouseEnter (const juce::MouseEvent&) { repaint(); }
void OrderBadge::mouseExit (const juce::MouseEvent&)  { repaint(); }

void OrderBadge::mouseDown (const juce::MouseEvent& e)
{
    if (upZone.contains (e.getPosition()))
        orderManager.nudgeOrder (moduleID, -1);   // vers une position plus tot dans la chaine
    else if (downZone.contains (e.getPosition()))
        orderManager.nudgeOrder (moduleID, +1);
    else
        return;

    displayedValue = orderManager.getOrder (moduleID);
    repaint();
}

void OrderBadge::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    orderManager.nudgeOrder (moduleID, wheel.deltaY > 0.0f ? -1 : 1);
    displayedValue = orderManager.getOrder (moduleID);
    repaint();
}

void OrderBadge::timerCallback()
{
    const int current = orderManager.getOrder (moduleID);
    if (current != displayedValue)
    {
        displayedValue = current;
        repaint();
    }
}

} // namespace onirique::ui
