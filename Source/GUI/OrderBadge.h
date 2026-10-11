#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ModuleOrderManager.h"

namespace onirique::ui
{

//==============================================================================
/** Petit controle numerique tres visible (coin superieur droit de chaque
    module) indiquant la position du module dans la chaine audio. Cliquer
    sur les fleches ou utiliser la molette change la position ; le
    reordonnancement des autres modules est delegue a ModuleOrderManager.

    Pas d'attachment JUCE standard ici (ce n'est pas un simple parametre :
    le changer a des effets de bord sur les autres modules), donc un leger
    polling (faible frequence, repaint uniquement si la valeur affichee a
    change) maintient l'affichage synchronise si un autre badge, l'automation
    hote, ou le chargement d'un projet modifie cette position. */
class OrderBadge : public juce::Component,
                   private juce::Timer
{
public:
    OrderBadge (ModuleOrderManager& managerToUse, juce::String moduleIDToUse, juce::Colour accentColour);
    ~OrderBadge() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;

    ModuleOrderManager& orderManager;
    juce::String moduleID;
    juce::Colour accent;

    juce::Rectangle<int> upZone, downZone, numberZone;
    int displayedValue = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrderBadge)
};

} // namespace onirique::ui
