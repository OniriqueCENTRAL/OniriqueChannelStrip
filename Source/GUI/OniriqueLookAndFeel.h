#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace onirique::ui
{

//==============================================================================
/** LookAndFeel unique pour tout le plugin, dans la DA ONIRIQUE (VocalBox) :
    potard = arc de valeur braise lumineux (depuis le centre si bipolaire) +
    corps spherique sombre teinte sang + repere clair ; menus et listes
    deroulantes en pastilles sombres a lisere fin. Tout est dessine en
    vectoriel (juce::Path / ColourGradient / DropShadow), aucune image.

    Le potard lit dans les proprietes du Slider : "shown" (position animee),
    "glow" (survol / reglage), "bipolar", "minLabel" / "maxLabel" -- fixees
    par ParamKnob. */
class OniriqueLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OniriqueLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

} // namespace onirique::ui
