#pragma once

#include "ProcessorModule.h"
#include "DSPHelpers.h"

namespace onirique
{

//==============================================================================
/** Saturation / transformateur harmonique, inspiree fonctionnellement du
    comportement d'une saturation de transformateur analogique (type Kazrog
    True Iron) : generation harmonique, densite qui augmente avec le niveau
    d'entree, legere compression naturelle, chaleur. Implementation
    originale (waveshaper + enveloppe + filtre de tonalite), aucun code
    tiers.

    Sur-echantillonnage optionnel (OFF / 2X / 4X / 8X, OFF par defaut) : une
    fonction non-lineaire comme un waveshaper cree des harmoniques au-dela de
    Nyquist qui replient (aliasing) si on les laisse au sample rate de base --
    c'est le seul endroit de la chaine ou le sur-echantillonnage apporte un
    vrai benefice sonore, d'ou son usage limite a ce module. Desactive par
    defaut (cout CPU nul), activable pour les reglages de drive eleve ou le
    materiel sensible a l'aliasing. Le changement de facteur reconstruit
    l'objet d'oversampling (une allocation), mais seulement sur un
    changement reel et explicite de ce choix -- jamais pendant un traitement
    audio normal -- et la latence introduite est correctement declaree a
    l'hote (voir getLatencySamples() et PluginProcessor). */
class SaturationModule : public ProcessorModule
{
public:
    SaturationModule (juce::AudioProcessorValueTreeState& state, const juce::String& moduleID);
    ~SaturationModule() override;

    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                               const juce::String& id);

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void process (juce::dsp::AudioBlock<float>& block) override;
    void reset() override;

    int getLatencySamples() const noexcept override;

    /** Meme waveshaper que process(), expose pour l'interface (ecran de
        courbe de transfert) : prend le type explicitement plutot que de
        lire le parametre, pour pouvoir tracer n'importe quel type sans
        toucher au module reel. Aucun etat, aucune allocation : appelable
        librement depuis le thread de l'interface. */
    static float previewShape (float x, int type) noexcept;

private:
    float shape (float x) const noexcept;
    void processCore (juce::dsp::AudioBlock<float>& blockAtCurrentRate);
    void setOversamplingIndex (int newIndex);
    double effectiveSampleRate() const noexcept;

    juce::dsp::WaveShaper<float, std::function<float (float)>> waveShaper { [this] (float x) { return shape (x); } };

    using ToneFilter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                       juce::dsp::IIR::Coefficients<float>>;
    ToneFilter toneFilter;

    // Bloqueur DC : les formes d'onde asymetriques (Tube, Transformer)
    // introduisent un leger decalage continu. Un filtre passe-haut tres bas
    // (quelques Hz) l'elimine sans toucher au spectre audible.
    using DcBlocker = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                      juce::dsp::IIR::Coefficients<float>>;
    DcBlocker dcBlocker;

    juce::dsp::Gain<float> inputGain, preGain, outputGain;
    juce::dsp::DryWetMixer<float> mixer { 512 };

    FilterParamCache toneCache;

    // Pointeurs mis en cache (lus a chaque echantillon dans shape(), on evite
    // la recherche par nom a chaque appel).
    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* typeParam  = nullptr;

    // Suivi de niveau d'entree (RMS, lisse) pour la coloration progressive :
    // plus le signal est fort, plus la saturation densifie, comme un
    // transformateur qui approche sa zone de saturation magnetique.
    float envelopeLevel = 0.0f;

    // --- Sur-echantillonnage (OFF par defaut) ------------------------------
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int oversamplingIndex = 0;        // 0=OFF, 1=2X, 2=4X, 3=8X (= exposant, factor JUCE)
    int preparedNumChannels = 2;
    int preparedBlockSize   = 512;
    FilterParamCache dcBlockerCache;  // recalcule dcBlocker si le sample rate effectif change

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaturationModule)
};

} // namespace onirique
