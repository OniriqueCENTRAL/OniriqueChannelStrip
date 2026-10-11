#pragma once

#include "ParamControls.h"
#include "OrderBadge.h"
#include "ModuleOrderManager.h"
#include "UIStyle.h"
#include <vector>

namespace onirique::ui
{

//==============================================================================
/** Specification declarative d'une ligne de controle, en coordonnees
    ABSOLUES de la maquette 1000x640, exprimees relativement a l'origine du
    PANNEAU (c.a.d. deja soustraites de x/y du panneau -- voir ui-spec.json
    -> panels/knobs). x/y/w/h decrivent toujours le rectangle englobant du
    controle ; les usines knob()/toggle()/... prennent le centre pour les
    potards (comme dans ui-spec.json) et convertissent. */
struct RowSpec
{
    enum class Type { knob, toggle, segmented, combo, section };

    Type type;
    juce::String paramSuffix;
    juce::String label;
    float x = 0, y = 0, w = 0, h = 0;
    bool showMid = false;

    static RowSpec knob (juce::String suffix, juce::String label, float cx, float cy, float sizeScale, bool showMid = false)
    {
        // Diametre visuel du cadran (arc compris), plus la zone de texte
        // sous le potard (nom / pastille de valeur, et valeur par defaut au
        // dessus si demandee) : le composant couvre tout cela, voir
        // ParamKnob::resized().
        const float dialD = 23.0f * 2.0f * sizeScale;
        const float w = dialD + 10.0f;
        const float h = dialD + (showMid ? 33.0f : 23.0f);
        return { Type::knob, std::move (suffix), std::move (label), cx - w * 0.5f, cy - dialD * 0.5f, w, h, showMid };
    }

    static RowSpec toggle (juce::String suffix, juce::String label, float x, float y, float w, float h)
    {
        return { Type::toggle, std::move (suffix), std::move (label), x, y, w, h, false };
    }

    static RowSpec segmented (juce::String suffix, float x, float y, float w, float h)
    {
        return { Type::segmented, std::move (suffix), {}, x, y, w, h, false };
    }

    static RowSpec combo (juce::String suffix, float x, float y, float w, float h)
    {
        return { Type::combo, std::move (suffix), {}, x, y, w, h, false };
    }

    /** withRule (reutilise le champ showMid, sans rapport avec les potards
        pour ce type) ajoute un filet apres le texte -- titre de GROUPE
        (ex: "EQ", "COMPRESSOR") plutot que sous-titre de bande (LF/LMF/...)
        qui n'en a pas besoin. Voir SectionLabel. */
    static RowSpec section (juce::String label, float x, float y, float w, float h, bool withRule = false)
    {
        return { Type::section, {}, std::move (label), x, y, w, h, withRule };
    }
};

//==============================================================================
/** Panneau d'un module dans la DA ONIRIQUE : carte en verre sombre, titre +
    point braise, bouton power (bypass), badge d'ordre, et les controles
    decrits par `rows`, tous positionnes aux coordonnees EXACTES de la
    maquette (mises a l'echelle au redimensionnement, ratio conserve par
    ComponentBoundsConstrainer cote editeur). Module coupe = voile nuit en
    fondu sur le contenu (titre et power restent nets). Couvre tous les
    modules sauf le panneau De-Esser double (voir DualDeEsserPanel). */
class ModulePanel : public juce::Component,
                    private juce::Timer
{
public:
    ModulePanel (juce::AudioProcessorValueTreeState& apvts, ModuleOrderManager& orderManager,
                const juce::String& moduleID, juce::String displayName, const ModulePalette& palette,
                float refWidth, float refHeight, const std::vector<RowSpec>& rows);
    ~ModulePanel() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    /** Ajoute un composant supplementaire (meter, courbe de transfert...)
        positionne aux memes coordonnees-maquette absolues que les RowSpec
        (relatives au panneau, mises a l'echelle identiquement). Renvoie une
        reference pour configuration ulterieure. */
    template <typename ComponentType>
    ComponentType& addExtra (std::unique_ptr<ComponentType> comp, float x, float y, float w, float h)
    {
        auto& ref = *comp;
        addAndMakeVisible (*comp);
        extras.push_back ({ std::move (comp), x, y, w, h });
        resized();
        bypassLed->toFront (false);
        orderBadge->toFront (false);
        return ref;
    }

    float getScaleFactor() const noexcept { return scaleFactor(); }
    const ModulePalette& getPalette() const noexcept { return modulePalette; }

protected:
    float scaleFactor() const noexcept { return getWidth() / juce::jmax (1.0f, refW); }

    ModulePalette modulePalette;
    float refW, refH;

private:
    void timerCallback() override;

    juce::AudioProcessorValueTreeState& state;
    juce::String bypassParamID;
    juce::String titleText;

    struct RowComp { std::unique_ptr<juce::Component> comp; RowSpec spec; };
    std::vector<RowComp> rowComponents;

    struct ExtraComp { std::unique_ptr<juce::Component> comp; float x, y, w, h; };
    std::vector<ExtraComp> extras;

    std::unique_ptr<PowerSwitch> bypassLed;
    std::unique_ptr<juce::ButtonParameterAttachment> bypassAttachment;
    std::unique_ptr<OrderBadge> orderBadge;
    std::atomic<float>* bypassRaw = nullptr;
    float veil = -1.0f;   // 0 = module actif, 1 = module coupe (voile anime)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulePanel)
};

} // namespace onirique::ui
