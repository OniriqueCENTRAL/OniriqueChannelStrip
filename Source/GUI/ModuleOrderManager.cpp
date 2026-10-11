#include "ModuleOrderManager.h"
#include "../Parameters.h"
#include <algorithm>
#include <vector>

namespace onirique::ui
{

ModuleOrderManager::ModuleOrderManager (juce::AudioProcessorValueTreeState& stateToUse)
    : apvts (stateToUse)
{
    using namespace onirique;
    moduleIDs = { ModuleIDs::noiseGate, ModuleIDs::sslChannel, ModuleIDs::deEsser1, ModuleIDs::deEsser2,
                 ModuleIDs::saturation, ModuleIDs::fet1176, ModuleIDs::fairchild670, ModuleIDs::eq };
}

juce::RangedAudioParameter* ModuleOrderManager::getOrderParam (const juce::String& moduleID) const
{
    return apvts.getParameter (moduleID + "_order");
}

int ModuleOrderManager::getOrder (const juce::String& moduleID) const
{
    if (auto* raw = apvts.getRawParameterValue (moduleID + "_order"))
        return (int) raw->load();

    jassertfalse;
    return 1;
}

void ModuleOrderManager::applyPermutation (const std::vector<juce::String>& orderedIDs)
{
    for (int i = 0; i < (int) orderedIDs.size(); ++i)
    {
        const auto& thisID = orderedIDs[(size_t) i];
        auto* param = getOrderParam (thisID);
        if (param == nullptr)
            continue;

        const int newValue = i + 1;
        if (getOrder (thisID) == newValue)
            continue;   // deja a la bonne place : ne pas generer d'ecriture host inutile

        const float normalised = param->convertTo0to1 ((float) newValue);
        param->beginChangeGesture();
        param->setValueNotifyingHost (normalised);
        param->endChangeGesture();
    }
}

void ModuleOrderManager::setOrder (const juce::String& moduleID, int requestedPosition)
{
    const int n = getNumModules();
    requestedPosition = juce::jlimit (1, n, requestedPosition);

    // 1) Ordre actuel de tous les modules (tri stable : a egalite, on garde
    //    l'ordre de declaration dans moduleIDs, situation qui ne devrait
    //    normalement pas arriver puisqu'on maintient une permutation propre,
    //    mais reste sure si l'etat charge est inattendu).
    std::vector<juce::String> current (moduleIDs.begin(), moduleIDs.end());
    std::stable_sort (current.begin(), current.end(),
                      [this] (const juce::String& a, const juce::String& b)
                      {
                          return getOrder (a) < getOrder (b);
                      });

    // 2) Retire le module qu'on deplace, puis le reinsere a la position demandee.
    current.erase (std::remove (current.begin(), current.end(), moduleID), current.end());

    const int insertIndex = juce::jlimit (0, (int) current.size(), requestedPosition - 1);
    current.insert (current.begin() + insertIndex, moduleID);

    // 3) Reassigne 1..N dans ce nouvel ordre : plus aucun doublon ni trou.
    applyPermutation (current);
}

void ModuleOrderManager::nudgeOrder (const juce::String& moduleID, int delta)
{
    setOrder (moduleID, getOrder (moduleID) + delta);
}

} // namespace onirique::ui
