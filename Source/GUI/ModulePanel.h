#pragma once

#include "ParamControls.h"
#include "UIStyle.h"
#include <vector>

namespace onirique::ui
{

/** Un potard d'un module, en coordonnées de la façade (relatives au module) : centre horizontal, haut de la case
    (le nom), diamètre du potard. La valeur s'écrit dessous. */
struct RowSpec
{
    juce::String paramSuffix, label;
    float cx = 0, top = 0, dial = 38;

    static RowSpec knob (juce::String suffix, juce::String label, float cx, float top, float dial = 38.0f)
    {
        return { std::move (suffix), std::move (label), cx, top, dial };
    }

    float width() const noexcept  { return juce::jmax (dial * 1.4f, 56.0f); }
    float height() const noexcept { return dial + 34.0f; }
};

/** Un module sur la façade, à plat comme sur VERB : le bouton power et le nom, un filet jusqu'au bout du module,
    puis ses potards et ses compléments (meters, sélecteurs, courbe). Module coupé = voile nuit en fondu sur son
    contenu (le titre et le power restent nets). */
class ModulePanel : public juce::Component,
                    private juce::Timer
{
public:
    ModulePanel (juce::AudioProcessorValueTreeState& apvts, const juce::String& moduleID, juce::String displayName,
                 float refWidth, float refHeight, const std::vector<RowSpec>& rows);
    ~ModulePanel() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    /** Ajoute un composant (meter, courbe, sélecteur…) aux coordonnées de la façade, relatives au module. */
    template <typename ComponentType>
    ComponentType& addExtra (std::unique_ptr<ComponentType> comp, float x, float y, float w, float h)
    {
        auto& ref = *comp;
        addAndMakeVisible (*comp);
        extras.push_back ({ std::move (comp), x, y, w, h });
        resized();
        bypassLed->toFront (false);
        return ref;
    }

    static constexpr float headH = 18.0f;   // la ligne du titre

private:
    void timerCallback() override;
    float scaleFactor() const noexcept { return (float) getWidth() / juce::jmax (1.0f, refW); }

    float refW, refH;
    juce::String titleText;

    struct RowComp { std::unique_ptr<ParamKnob> knob; RowSpec spec; };
    std::vector<RowComp> knobs;

    struct ExtraComp { std::unique_ptr<juce::Component> comp; float x, y, w, h; };
    std::vector<ExtraComp> extras;

    std::unique_ptr<PowerSwitch> bypassLed;
    std::unique_ptr<juce::ButtonParameterAttachment> bypassAttachment;
    std::atomic<float>* bypassRaw = nullptr;
    float veil = -1.0f;   // 0 = module actif, 1 = module coupé (voile animé)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulePanel)
};

} // namespace onirique::ui
