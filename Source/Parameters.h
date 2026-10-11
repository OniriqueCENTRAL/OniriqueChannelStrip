#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace onirique
{

//==============================================================================
/** Identifiants stables des 8 modules du channel strip.
    Utilises comme prefixe de tous les parametres d'un module
    (ex: "sslchannel_threshold") et comme cle de recherche dans ModuleChain. */
namespace ModuleIDs
{
    inline const juce::String noiseGate    { "noisegate" };
    inline const juce::String sslChannel   { "sslchannel" };
    inline const juce::String deEsser1     { "deesser1" };
    inline const juce::String deEsser2     { "deesser2" };
    inline const juce::String saturation   { "saturation" };
    inline const juce::String fet1176      { "fet1176" };
    inline const juce::String fairchild670 { "fairchild670" };
    inline const juce::String eq           { "eq" };
}

/** Nombre total de modules de la chaine : borne haute du parametre "order"
    de chaque module (positions 1..numModules). */
constexpr int numModules = 8;

/** Construit la liste complete des parametres du plugin : parametres communs
    (ordre + bypass) et parametres propres de chacun des 8 modules. Appelee
    une seule fois, a la construction de l'AudioProcessorValueTreeState. */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace onirique
