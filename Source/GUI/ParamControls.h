#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "UIStyle.h"

namespace onirique::ui
{

/** Formate une valeur reelle de parametre pour affichage court (potard,
    valeur en cours de glissement). Jamais de decimales superflues, les
    frequences hautes passent en kHz ("2.0k"), les dB positifs portent un
    signe explicite ("+12"). `unitLabel` est le label JUCE du parametre
    (AudioProcessorParameter::getLabel() : "dB", "Hz", "ms", "%", ":1", ou
    vide). */
juce::String formatKnobValue (float value, const juce::String& unitLabel);

//==============================================================================
/** Potard rotatif ONIRIQUE (rendu dans OniriqueLookAndFeel::drawRotarySlider)
    + nom dessous. Au survol / pendant le reglage, le nom laisse place a une
    pastille de valeur lumineuse et l'arc braise s'illumine. A l'ouverture,
    les arcs se deploient jusqu'a leur valeur (comme le VocalBox).
    Bornes min/max derivees du NormalisableRange REEL du parametre. */
class ParamKnob : public juce::Component,
                  private juce::Timer
{
public:
    ParamKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
              const juce::String& displayLabel, juce::Colour accentColour, juce::Colour accentBrightColour,
              bool showMidValue = false);
    ~ParamKnob() override;

    /** Echelle maquette -> pixels du panneau parent (textes et marges en
        unites maquette, comme le reste de l'interface). */
    void setUiScale (float s) { uiScale = s; resized(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    juce::Slider& getSlider() noexcept { return slider; }

private:
    void timerCallback() override;
    void animate() { if (! isTimerRunning()) startTimerHz (60); }
    juce::String valueText() const;

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    std::unique_ptr<juce::SliderParameterAttachment> attachment;

    juce::String displayName, unitLabel, midText;
    bool hasMid = false, hovering = false, dragging = false;
    float uiScale = 1.0f, shown = 0.0f, glow = 0.0f;
    juce::Rectangle<float> labelArea, midArea;
    float dialD = 10.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamKnob)
};

//==============================================================================
/** Bouton power d'un module (style VocalBox) : anneau braise lumineux quand
    le module est actif. Relie au parametre <id>_bypass, donc INVERSE :
    toggle = true -> module bypasse -> anneau eteint. */
class PowerSwitch : public juce::ToggleButton,
                    private juce::Timer
{
public:
    PowerSwitch();
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    void timerCallback() override;
    float target() const { return getToggleState() ? 0.0f : 1.0f; }
    float on = -1.0f;
};

//==============================================================================
/** Interrupteur en pastille : LED braise + libelle (AUTO, LISTEN...). */
class PillButton : public juce::ToggleButton,
                   private juce::Timer
{
public:
    explicit PillButton (const juce::String& label);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    void timerCallback() override;
    float on = -1.0f;
};

//==============================================================================
/** Pastille LED + nom, pour les parametres bool (auto gain, listen...). */
class ParamToggle : public juce::Component
{
public:
    ParamToggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                const juce::String& displayLabel, juce::Colour accentColour, juce::Colour accentBrightColour);

    void resized() override;
    juce::ToggleButton& getButton() noexcept { return button; }

private:
    PillButton button;
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamToggle)
};

//==============================================================================
/** Selecteur segmente (ratio du 1176, type de saturation...) relie a un
    AudioParameterChoice : la pastille braise glisse vers le choix. */
class ParamSegmented : public juce::Component,
                       private juce::Timer
{
public:
    ParamSegmented (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                    juce::Colour accentColour, juce::Colour accentBrightColour);
    ~ParamSegmented() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    int segmentAt (float x) const noexcept;

    juce::StringArray labels;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int selected = 0, hover = -1;
    float slide = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamSegmented)
};

//==============================================================================
/** Menu deroulant (type de bande EQ) avec attachment standard. */
class ParamCombo : public juce::Component
{
public:
    ParamCombo (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
               juce::Colour accentColour);

    void resized() override;
    juce::ComboBox& getBox() noexcept { return box; }

private:
    juce::ComboBox box;
    std::unique_ptr<juce::ComboBoxParameterAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamCombo)
};

//==============================================================================
class SectionLabel : public juce::Component
{
public:
    /** withRule = true : titre de GROUPE (ex: "EQ"/"COMPRESSOR"), texte clair
        suivi d'un filet fin ; false : sous-titre de bande (LF/LMF/HMF/HF). */
    SectionLabel (const juce::String& text, juce::Colour accentColour, bool withRule = false);
    void paint (juce::Graphics&) override;

private:
    juce::String labelText;
    bool hasRule;
};

} // namespace onirique::ui
