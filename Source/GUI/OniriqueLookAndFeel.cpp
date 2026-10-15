#include "OniriqueLookAndFeel.h"
#include "OniriqueKnob.h"
#include "UIStyle.h"
#include <cmath>

namespace onirique::ui
{

namespace
{
    juce::Point<float> onCircle (juce::Point<float> c, float r, float ang)
    {
        return { c.x + std::sin (ang) * r, c.y - std::cos (ang) * r };
    }
}

OniriqueLookAndFeel::OniriqueLookAndFeel()
{
    setDefaultSansSerifTypefaceName ("Segoe UI");
    setColour (juce::ResizableWindow::backgroundColourId, night);
    setColour (juce::ComboBox::textColourId, mist);
    setColour (juce::ComboBox::backgroundColourId, night);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::PopupMenu::backgroundColourId, deep2);
    setColour (juce::PopupMenu::textColourId, mist);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ember.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, bone);
    setColour (juce::TooltipWindow::backgroundColourId, deep2);
    setColour (juce::TooltipWindow::textColourId, mist);
    setColour (juce::TooltipWindow::outlineColourId, line);
}

//==============================================================================
void OniriqueLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                            float a0, float a1, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (1.0f);
    const float D = juce::jmin (b.getWidth(), b.getHeight());
    if (D < 8.0f) return;

    const auto c = b.getCentre();
    const float R = D * 0.5f, u = D / 150.0f;
    const auto& props = s.getProperties();
    const float shown = (float) props.getWithDefault ("shown", pos);
    const float glow = (float) props.getWithDefault ("glow", 0.0f);
    const bool bipolar = (bool) props.getWithDefault ("bipolar", false);

    // ---- le potard ONIRIQUE, le même dans tous les plugins de la marque (OniriqueKnob.h) : graduations allumées
    //      jusqu'à la valeur (depuis le centre si bipolaire), corps biseauté, aiguille à l'étoile
    onirique::ui::drawKnob (g, b, shown, a0, a1, ember, bipolar, s.isMouseOverOrDragging() || glow > 0.5f);

    // ---- bornes de l'echelle (seulement si le potard est assez grand pour rester lisible)
    const auto minL = props["minLabel"].toString(), maxL = props["maxLabel"].toString();
    if ((minL.isNotEmpty() || maxL.isNotEmpty()) && D >= 70.0f)
    {
        g.setColour (haze.withAlpha (0.8f));
        for (int side = 0; side < 2; ++side)
        {
            const auto& txt = side == 0 ? minL : maxL;
            const auto f = fitFont (Face::mono, std::max (7.0f, 13.0f * u), 0.30f * D, txt);
            const float tw = textWidth (f, txt) + 2.0f;
            const auto pt = onCircle (c, R * 0.9f, side == 0 ? a0 : a1);
            const float cx = juce::jlimit (b.getX() + tw * 0.5f, b.getRight() - tw * 0.5f, pt.x);
            g.setFont (f);
            g.drawText (txt, juce::Rectangle<float> (tw, f.getHeight() * 1.3f).withCentre ({ cx, pt.y + 0.17f * R }),
                        juce::Justification::centred, false);
        }
    }
}

//==============================================================================
void OniriqueLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                        int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
    const float rad = r.getHeight() * 0.5f, u = r.getHeight() / 26.0f;
    const float hl = (isButtonDown || box.isMouseOver (true)) ? 1.0f : 0.0f;

    g.setColour (juce::Colours::black.withAlpha (0.34f));
    g.fillRoundedRectangle (r, rad);
    g.setColour (line.interpolatedWith (ember, 0.7f * hl));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, std::max (1.0f, 1.1f * u));

    // chevron
    const float ax = r.getRight() - rad, ay = r.getCentreY(), aw = 4.0f * u;
    juce::Path ch;
    ch.startNewSubPath (ax - aw, ay - aw * 0.5f);
    ch.lineTo (ax, ay + aw * 0.5f);
    ch.lineTo (ax + aw, ay - aw * 0.5f);
    g.setColour (haze.interpolatedWith (ember, hl));
    g.strokePath (ch, juce::PathStrokeType (std::max (1.2f, 1.8f * u), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void OniriqueLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    const int h = box.getHeight();
    label.setBounds (h / 2, 1, box.getWidth() - h - h / 2, h - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void OniriqueLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (deep2);
    g.setColour (line);
    g.drawRect (0, 0, width, height, 1);
}

//==============================================================================
juce::Font OniriqueLookAndFeel::getLabelFont (juce::Label& l)      { return font (Face::body, std::max (9.0f, (float) l.getHeight() * 0.6f), true, 0.1f); }
juce::Font OniriqueLookAndFeel::getComboBoxFont (juce::ComboBox& b) { return font (Face::body, std::max (9.0f, (float) b.getHeight() * 0.46f), true, 0.08f); }
juce::Font OniriqueLookAndFeel::getPopupMenuFont()                  { return font (Face::body, 14.0f, true, 0.06f); }

} // namespace onirique::ui
