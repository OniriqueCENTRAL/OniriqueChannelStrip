#pragma once

#include "ProcessorModule.h"

namespace onirique
{

//==============================================================================
/** Compresseur FET rapide, inspire fonctionnellement du comportement general
    d'un compresseur a diode tres rapide (type 1176) : attaque/relache quasi
    instantanes, ratios fixes selectionnables, punch et densite. Auto gain
    optionnel (compensation automatique de la reduction de gain mesuree).
    Implementation originale, aucun code tiers. */
class FETCompressorModule : public ProcessorModule
{
public:
    FETCompressorModule (juce::AudioProcessorValueTreeState& state, const juce::String& moduleID);

    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                               const juce::String& id);

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void process (juce::dsp::AudioBlock<float>& block) override;
    void reset() override;
    void resetMeters() noexcept override { gainReductionDb.store (0.0f); }

    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }

private:
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Gain<float> inputGain, outputGain;
    juce::dsp::DryWetMixer<float> mixer { 512 };

    juce::AudioBuffer<float> dryLevelBuffer;   // copie pre-compression, pour mesurer la reduction de gain

    // Compensation auto-gain lissee (evite les sauts de niveau quand elle
    // s'engage/se desengage ou quand la reduction varie brusquement).
    float smoothedAutoGainDb = 0.0f;

    std::atomic<float> gainReductionDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FETCompressorModule)
};

} // namespace onirique
