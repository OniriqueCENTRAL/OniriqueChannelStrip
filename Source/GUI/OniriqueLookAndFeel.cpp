#include "OniriqueLookAndFeel.h"
#include "OniriqueKnobLogo.h"
#include "UIStyle.h"

namespace onirique::ui
{

OniriqueLookAndFeel::OniriqueLookAndFeel()
{
    onirique::look::load();   // la couleur de l'interface choisie (avant de copier les couleurs)
    applyColours();
}

// les couleurs des menus, champs et boutons : recopiées ici, et de nouveau quand la couleur de l'interface change
void OniriqueLookAndFeel::applyColours()
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

    const auto& props = s.getProperties();
    const float shown = (float) props.getWithDefault ("shown", pos);
    const float glow = (float) props.getWithDefault ("glow", 0.0f);
    const bool bipolar = (bool) props.getWithDefault ("bipolar", false);
    const bool hot = s.isMouseOverOrDragging() || glow > 0.5f;

    // les potards du logo (OniriqueKnobLogo.h) : « Constellation » pour la dynamique, « Sceau » pour le timbre
    if (props["model"].toString() == "constellation")
        drawKnobConstellation (g, b, shown, a0, a1, ember, bipolar, hot);
    else
        drawKnobSceau (g, b, shown, a0, a1, ember, bipolar, hot);
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
