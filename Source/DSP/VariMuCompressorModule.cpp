#include "VariMuCompressorModule.h"
#include "DSPHelpers.h"
#include "../Parameters.h"
#include <cmath>

namespace onirique
{

namespace
{
    // Saturation tube douce : plus la reduction de gain est importante, plus
    // la coloration harmonique s'intensifie (comportement "glue" program-
    // dependent, caracteristique des compresseurs a tube).
    inline float tubeGlow (float x, float amount) noexcept
    {
        const float drive = 1.0f + amount * 0.5f;
        return std::tanh (x * drive) / drive;
    }
}

VariMuCompressorModule::VariMuCompressorModule (juce::AudioProcessorValueTreeState& state, const juce::String& moduleID)
    : ProcessorModule (state, moduleID)
{
}

void VariMuCompressorModule::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                                            const juce::String& id)
{
    addCommonParameters (layout, id, "Fairchild 670", numModules, 6);

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_input", 1), "Input",
        juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, dbAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_threshold", 1), "Threshold",
        juce::NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -18.0f, dbAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_attack", 1), "Attack",
        juce::NormalisableRange<float> (5.0f, 200.0f, 0.1f, 0.5f), 40.0f, msAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_release", 1), "Release",
        juce::NormalisableRange<float> (100.0f, 5000.0f, 1.0f, 0.4f), 800.0f, msAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_ratio", 1), "Ratio",
        juce::NormalisableRange<float> (1.0f, 8.0f, 0.01f, 0.6f), 3.0f,
        juce::AudioParameterFloatAttributes().withLabel (":1")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_output", 1), "Output",
        juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, dbAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_mix", 1), "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pctAttrib()));
}

void VariMuCompressorModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    compressor.prepare (spec);
    inputGain.prepare (spec);
    outputGain.prepare (spec);
    mixer.prepare (spec);

    // Chauffe le cache de parametres, hors du thread audio.
    for (const char* suffix : { "input", "threshold", "attack", "release", "ratio", "output", "mix" })
        warmUpParam (suffix);

    reset();
}

void VariMuCompressorModule::reset()
{
    compressor.reset();
    inputGain.reset();
    outputGain.reset();
    mixer.reset();
    gainReductionDb.store (0.0f);
}

void VariMuCompressorModule::process (juce::dsp::AudioBlock<float>& block)
{
    const auto numCh  = block.getNumChannels();
    const auto numSmp = block.getNumSamples();

    mixer.setWetMixProportion (getParam ("mix") / 100.0f);
    mixer.pushDrySamples (block);

    inputGain.setGainDecibels (getParam ("input"));

    compressor.setThreshold (getParam ("threshold"));
    compressor.setRatio      (juce::jmax (1.0f, getParam ("ratio")));
    compressor.setAttack     (getParam ("attack"));
    compressor.setRelease    (getParam ("release"));

    juce::dsp::ProcessContextReplacing<float> context (block);
    inputGain.process (context);

    const float preRmsDb = juce::Decibels::gainToDecibels (blockRmsLevel (block), -100.0f);

    compressor.process (context);

    const float postRmsDb = juce::Decibels::gainToDecibels (blockRmsLevel (block), -100.0f);

    const float reductionDb = juce::jmax (0.0f, preRmsDb - postRmsDb);
    gainReductionDb.store (reductionDb);
    const float glowAmount  = juce::jlimit (0.0f, 1.0f, reductionDb / 12.0f);

    for (size_t ch = 0; ch < numCh; ++ch)
    {
        auto* samples = block.getChannelPointer (ch);
        for (size_t s = 0; s < numSmp; ++s)
            samples[s] = tubeGlow (samples[s], glowAmount);
    }

    outputGain.setGainDecibels (getParam ("output"));
    outputGain.process (context);

    mixer.mixWetSamples (block);
    sanitizeBlock (block);
}

} // namespace onirique
