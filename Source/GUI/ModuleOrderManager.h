#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace onirique::ui
{

//==============================================================================
/** Gere le reordonnancement "propre" des 8 modules : change la position d'un
    module et decale les autres pour qu'il n'y ait jamais deux modules sur la
    meme position (permutation 1..N toujours valide). Le graphisme de chaque
    module ne bouge jamais : seule la valeur du parametre "<id>_order" change,
    ce qui modifie l'ordre de traitement audio (voir DSP/ModuleChain). */
class ModuleOrderManager
{
public:
    explicit ModuleOrderManager (juce::AudioProcessorValueTreeState& stateToUse);

    /** Position actuelle (1..N) d'un module. */
    int getOrder (const juce::String& moduleID) const;

    /** Deplace moduleID a la position demandee (clampee a 1..N) et decale
        proprement tous les autres modules pour combler l'espace libere /
        faire de la place : aucune position en double, aucun trou. */
    void setOrder (const juce::String& moduleID, int requestedPosition);

    /** Decale la position d'un module de +1/-1 (pratique pour la molette ou
        les petites fleches des badges d'ordre). */
    void nudgeOrder (const juce::String& moduleID, int delta);

    int getNumModules() const noexcept { return (int) moduleIDs.size(); }

private:
    juce::RangedAudioParameter* getOrderParam (const juce::String& moduleID) const;
    void applyPermutation (const std::vector<juce::String>& orderedIDs);

    juce::AudioProcessorValueTreeState& apvts;
    std::array<juce::String, 8> moduleIDs;
};

} // namespace onirique::ui
