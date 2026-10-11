#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

namespace onirique
{

//==============================================================================
/** Un preset d'usine : un nom + une liste eparse de (identifiant de
    parametre complet, valeur reelle). Un parametre absent de la liste garde
    sa valeur par defaut -- appliquer un preset reinitialise donc d'abord
    tout aux valeurs par defaut, puis applique uniquement ces overrides (voir
    PluginProcessor::setCurrentProgram). Les valeurs sont exprimees dans
    l'unite reelle du parametre (dB, Hz, ms, %, index de choix...), pas
    normalisees. */
struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;
};

/** Table des presets d'usine. Systeme extensible par construction : ajouter
    un preset = ajouter une entree dans Presets.cpp, rien d'autre a modifier
    (PluginProcessor expose getNumPrograms/getProgramName/setCurrentProgram
    directement a partir de cette liste, donc visible dans le selecteur de
    presets standard de n'importe quel DAW). */
const std::vector<FactoryPreset>& getFactoryPresets();

} // namespace onirique
