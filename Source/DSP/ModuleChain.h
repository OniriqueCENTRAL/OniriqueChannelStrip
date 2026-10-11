#pragma once

#include "ProcessorModule.h"
#include <vector>
#include <memory>

namespace onirique
{

//==============================================================================
/** Conteneur des modules du channel strip.

    L'ORDRE GRAPHIQUE des modules (fixe, impose par l'interface) est totalement
    independant de l'ORDRE DE TRAITEMENT AUDIO (modifiable par l'utilisateur
    via le parametre "<id>_order" de chaque module).

    A chaque bloc audio, la chaine :
      1) retrie en place la liste des modules par getOrder() (tri par
         insertion manuel : 8 elements, zero allocation, contrairement a
         std::stable_sort dont le comportement d'allocation interne n'est pas
         garanti) ;
      2) traite le buffer sequentiellement a travers les modules non
         bypasses, dans cet ordre.

    Deux protections anti-glitch, actives uniquement le temps necessaire (cout
    nul en regime stable) :
      - BYPASS : chaque module a un gain lisse (dry/wet crossfade ~8 ms) qui
        rampe vers 0 ou 1 quand son bypass change, au lieu d'un interrupteur
        brutal. Une fois la rampe stabilisee, le module bypasse n'est plus du
        tout appele (cout CPU minimal) et le module actif est traite
        normalement (aucune copie supplementaire).
      - REORDONNANCEMENT : un changement d'ordre de traitement reconstruit un
        chemin de signal different d'un bloc a l'autre, ce qu'aucun crossfade
        wet/dry simple ne peut masquer proprement (ce n'est pas un bypass).
        Un court fondu de sortie (~6 ms) attenue la discontinuite au lieu de
        la laisser passer telle quelle.
*/
class ModuleChain
{
public:
    ModuleChain() = default;

    /** Ajoute un module a la chaine. L'ordre d'ajout ne determine PAS l'ordre
        de traitement (voir getOrder()), seulement le choix en cas d'egalite. */
    ProcessorModule& addModule (std::unique_ptr<ProcessorModule> module);

    /** Recupere un module par son identifiant (ex: "sslchannel"). */
    ProcessorModule* getModule (const juce::String& moduleID) const;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::dsp::AudioBlock<float>& block);
    void reset();

    int getNumModules() const noexcept { return (int) modules.size(); }

    /** Somme des latences (en echantillons) de tous les modules, quel que
        soit leur etat de bypass -- voir ProcessorModule::getLatencySamples()
        pour le choix assume de ne pas faire varier la latence declaree a
        l'hote au gre des bypass. */
    int getTotalLatencySamples() const noexcept;

private:
    struct Entry
    {
        ProcessorModule* module = nullptr;
        size_t rampIndex = 0;   // indice dans bypassRamps (parallele a "modules")
    };

    void rebuildProcessingOrderIfNeeded();

    std::vector<std::unique_ptr<ProcessorModule>> modules;

    // Liste de travail reconstruite/triee a chaque bloc (pas d'allocation en
    // dehors de prepare(), le vector garde sa capacite entre les appels).
    std::vector<Entry> sortedEntries;

    // Un gain lisse par module (voir la doc de classe ci-dessus), indexe
    // dans le meme ordre que "modules" (pas "sortedEntries").
    std::vector<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>> bypassRamps;

    juce::AudioBuffer<float> dryScratch;   // copie temporaire, utilisee seulement pendant une transition de bypass

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> orderChangeFade;
    std::vector<ProcessorModule*> lastOrderSignature;   // pour detecter un changement d'ordre reel

    double preparedSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleChain)
};

} // namespace onirique
