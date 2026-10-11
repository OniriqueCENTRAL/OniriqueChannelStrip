#pragma once

#include "ProcessorModule.h"
#include "DSPHelpers.h"

namespace onirique
{

//==============================================================================
/** Channel strip style console (inspire du comportement d'un SSL Channel,
    sans aucun code ni coefficient proprietaire) :

      Trim -> HPF -> EQ 4 bandes -> Compresseur -> Gate/Expander -> Output

    Objectif sonore : voix presente, mediums propres, aigu precis, attaque
    nette, controle dynamique efficace. */
class SSLChannelModule : public ProcessorModule
{
public:
    SSLChannelModule (juce::AudioProcessorValueTreeState& state, const juce::String& moduleID);

    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                               const juce::String& id);

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void process (juce::dsp::AudioBlock<float>& block) override;
    void reset() override;
    void resetMeters() noexcept override { gainReductionDb.store (0.0f); }

    /** Reduction de gain (dB, >= 0) du compresseur bus-style, mesuree une
        fois par bloc (RMS avant/apres compressor.process()) -- voir le
        brief UI §6. Lecture sans lock depuis le thread de l'interface. */
    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }

private:
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;

    void updateFiltersIfNeeded();
    void processGateExpander (juce::dsp::AudioBlock<float>& block);

    Filter hpf, lowShelf, lowMid, highMid, highShelf;
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Gain<float> trimGain, makeupGain, outputGain;

    FilterParamCache hpfCache, lowCache, lowMidCache, highMidCache, highCache;

    // Gate/expander : implementation legere par echantillon (pas de filtre a
    // reconstruire, cout CPU negligeable).
    float gateEnvelope   = 0.0f;
    float gateGainLin    = 1.0f;

    std::atomic<float> gainReductionDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SSLChannelModule)
};

} // namespace onirique
