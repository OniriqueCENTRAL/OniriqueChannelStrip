#pragma once

#include "ProcessorModule.h"

namespace onirique
{

//==============================================================================
/** Compresseur vari-mu lent et program-dependent, inspire fonctionnellement
    du comportement general d'un compresseur a tube (type Fairchild 670) :
    attaque/relache naturellement plus lents que le FET, ratio doux, legere
    saturation harmonique proportionnelle a la reduction de gain (effet
    "glue"). Nettement plus doux, epais et musical que le module FET.
    Implementation originale, aucun code tiers. */
class VariMuCompressorModule : public ProcessorModule
{
public:
    VariMuCompressorModule (juce::AudioProcessorValueTreeState& state, const juce::String& moduleID);

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

    std::atomic<float> gainReductionDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VariMuCompressorModule)
};

} // namespace onirique
