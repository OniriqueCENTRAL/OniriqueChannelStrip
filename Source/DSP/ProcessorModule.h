#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <utility>

namespace onirique
{

//==============================================================================
/** Ajoute les deux parametres communs a tous les modules de la chaine :
      <id>_order   -> position du module dans la chaine audio (1..NUM_MODULES)
      <id>_bypass  -> bypass individuel du module

    A appeler depuis la fonction addParameters() de chaque module, avant
    d'ajouter ses parametres propres.
*/
inline void addCommonParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                                  const juce::String& id,
                                  const juce::String& displayName,
                                  int numModules,
                                  int defaultOrder)
{
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID (id + "_order", 1),
        displayName + " Order",
        1, numModules, defaultOrder));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID (id + "_bypass", 1),
        displayName + " Bypass",
        true));   // tout est bypasse par defaut a l'ouverture du plugin --
                  // activation module par module par l'utilisateur, demande
                  // explicite (ne change que la VALEUR PAR DEFAUT du
                  // parametre, pas sa plage ni son comportement -- le
                  // parametre existait deja et etait deja automatisable)
}

//==============================================================================
/** Interface commune a tous les modules de traitement du channel strip.

    Un module :
      - lit ses parametres dans l'AudioProcessorValueTreeState (automatisables
        et sauvegardes avec le projet, sans duplication d'etat) ;
      - expose sa position (getOrder()) et son etat de bypass (isBypassed()),
        tous deux pilotes par des parametres hote (donc automatisables) ;
      - est prepare/traite/reinitialise comme un maillon de dsp::ProcessorChain,
        mais la chaine elle-meme (ModuleChain) reordonne les maillons a chaque
        bloc en fonction de getOrder().
*/
class ProcessorModule
{
public:
    ProcessorModule (juce::AudioProcessorValueTreeState& stateToUse, juce::String moduleID)
        : apvts (stateToUse), id (std::move (moduleID))
    {
        orderParam  = apvts.getRawParameterValue (id + "_order");
        bypassParam = apvts.getRawParameterValue (id + "_bypass");

        jassert (orderParam != nullptr);
        jassert (bypassParam != nullptr);
    }

    virtual ~ProcessorModule() = default;

    /** Alloue les ressources DSP pour la configuration audio donnee. */
    virtual void prepare (const juce::dsp::ProcessSpec& spec) = 0;

    /** Traite le bloc en place. Le ModuleChain ne l'appelle que si le module
        n'est pas bypasse : les classes derivees n'ont donc pas a tester
        isBypassed() elles-memes. */
    virtual void process (juce::dsp::AudioBlock<float>& block) = 0;

    /** Reinitialise l'etat interne (enveloppes, filtres, buffers...). */
    virtual void reset() = 0;

    /** Latence (en echantillons, au sample rate de base) introduite par ce
        module, independamment de son etat de bypass -- 0 par defaut. Seule
        la Saturation (sur-echantillonnage optionnel) la redefinit. Utilisee
        par ModuleChain pour que le plugin declare une latence correcte au
        DAW (voir AudioProcessor::setLatencySamples). */
    virtual int getLatencySamples() const noexcept { return 0; }

    /** Remet a zero le(s) atomic(s) de meter du module (gain reduction,
        niveau detecte...), appele par ModuleChain quand le module est
        bypasse et stable : un module qu'on n'appelle plus ne doit pas
        laisser son meter affiche a sa derniere valeur active. Sans effet
        par defaut (modules sans meter). */
    virtual void resetMeters() noexcept {}

    //==========================================================================
    int getOrder() const noexcept
    {
        return orderParam != nullptr ? (int) orderParam->load() : 0;
    }

    bool isBypassed() const noexcept
    {
        return bypassParam != nullptr && bypassParam->load() > 0.5f;
    }

    const juce::String& getID() const noexcept { return id; }

protected:
    /** Lit un parametre propre au module (prefixe automatiquement par son
        id), ex: getParam ("threshold") lit "sslchannel_threshold".

        IMPORTANT (thread audio / zero-allocation) : la toute premiere lecture
        d'un suffixe donne construit la chaine complete du parametre et
        interroge l'apvts (recherche par nom + eventuelle allocation de
        String) -- ce chemin ne doit JAMAIS s'executer pendant processBlock().
        Chaque module doit donc "chauffer" tous les suffixes qu'il utilisera,
        en appelant warmUpParam() pour chacun depuis son prepare() (qui tourne
        hors du thread audio, avant le premier processBlock). Une fois chauffe,
        getParam() ne fait plus qu'un parcours lineaire d'un petit tableau
        deja alloue (quelques dizaines d'entrees max) et une lecture atomique :
        aucune allocation, aucune recherche par nom dans l'apvts. */
    float getParam (const juce::String& paramName) const noexcept
    {
        for (auto& entry : paramCache)
            if (entry.first == paramName)
                return entry.second->load (std::memory_order_relaxed);

        // Chemin lent (uniquement au "chauffage", jamais depuis l'audio) :
        // resout le parametre une fois pour toutes et le met en cache.
        auto* ptr = apvts.getRawParameterValue (id + "_" + paramName);
        jassert (ptr != nullptr);   // parametre introuvable : verifier addParameters()

        if (ptr != nullptr)
            paramCache.push_back ({ paramName, ptr });

        return ptr != nullptr ? ptr->load (std::memory_order_relaxed) : 0.0f;
    }

    /** A appeler depuis prepare() (jamais depuis process()) pour chaque
        suffixe de parametre utilise par le module : garantit qu'aucune
        allocation ni recherche par nom ne se produira plus tard dans
        process(). Idempotent (sans effet si deja en cache). */
    void warmUpParam (const juce::String& paramName) const
    {
        juce::ignoreUnused (getParam (paramName));
    }

    double getSampleRate() const noexcept { return sampleRate; }

    double sampleRate = 44100.0;

    juce::AudioProcessorValueTreeState& apvts;
    juce::String id;

private:
    std::atomic<float>* orderParam  = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    // mutable : getParam() reste logiquement const (simple lecture de
    // parametre) alors qu'il alimente ce cache au premier appel.
    mutable std::vector<std::pair<juce::String, std::atomic<float>*>> paramCache;
};

} // namespace onirique
