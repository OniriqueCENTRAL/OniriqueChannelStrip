#include "FETCompressorModule.h"
#include "DSPHelpers.h"
#include "../Parameters.h"
#include <cmath>

namespace onirique
{

namespace
{
    // Index du menu "ratio" -> valeur de ratio appliquee au compresseur.
    constexpr float ratioTable[4] = { 4.0f, 8.0f, 12.0f, 20.0f };

    constexpr double autoGainSeconds    = 0.10;    // lissage de la compensation (0,9 par bloc de 512 a 48 kHz, quelle que soit la taille de bloc)
    constexpr float gainRampSeconds     = 0.015f;  // IN, OUT et gain auto glissent au lieu de sauter d'un bloc a l'autre
    constexpr float autoGainMaxDb       = 18.0f;   // securite : compensation jamais demesuree
}

FETCompressorModule::FETCompressorModule (juce::AudioProcessorValueTreeState& state, const juce::String& moduleID)
    : ProcessorModule (state, moduleID)
{
}

void FETCompressorModule::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                                         const juce::String& id)
{
    addCommonParameters (layout, id, "1176 FET", numModules, 5);

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_input", 1), "Input",
        juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, dbAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_threshold", 1), "Threshold",
        juce::NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -18.0f, dbAttrib()));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID (id + "_ratio", 1), "Ratio",
        juce::StringArray { "4:1", "8:1", "12:1", "20:1" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_attack", 1), "Attack",
        juce::NormalisableRange<float> (0.02f, 0.8f, 0.001f, 0.5f), 0.2f, msAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_release", 1), "Release",
        juce::NormalisableRange<float> (50.0f, 1100.0f, 1.0f, 0.4f), 250.0f, msAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_output", 1), "Output",
        juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, dbAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_mix", 1), "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pctAttrib()));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID (id + "_autoGain", 1), "Auto Gain", false));
}

void FETCompressorModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    compressor.prepare (spec);
    inputGain.prepare (spec);
    outputGain.prepare (spec);
    inputGain.setRampDurationSeconds (gainRampSeconds);
    outputGain.setRampDurationSeconds (gainRampSeconds);
    mixer.prepare (spec);

    // Chauffe le cache de parametres, hors du thread audio.
    for (const char* suffix : { "input", "threshold", "ratio", "attack", "release",
                                "output", "mix", "autoGain" })
        warmUpParam (suffix);

    reset();
}

void FETCompressorModule::reset()
{
    compressor.reset();
    inputGain.reset();
    outputGain.reset();
    mixer.reset();
    smoothedAutoGainDb = 0.0f;
    inPower = outPower = 0.0f;
    gainReductionDb.store (0.0f);
}

void FETCompressorModule::process (juce::dsp::AudioBlock<float>& block)
{
    const auto numSmp = (int) block.getNumSamples();

    mixer.setWetMixProportion (getParam ("mix") / 100.0f);
    mixer.pushDrySamples (block);

    inputGain.setGainDecibels (getParam ("input"));

    const int ratioIndex = juce::jlimit (0, 3, (int) getParam ("ratio"));
    compressor.setRatio (ratioTable[(size_t) ratioIndex]);
    compressor.setThreshold (getParam ("threshold"));
    compressor.setAttack  (getParam ("attack"));
    compressor.setRelease (getParam ("release"));

    juce::dsp::ProcessContextReplacing<float> context (block);
    inputGain.process (context);

    // Niveaux avant / apres le compresseur : meter de reduction (bloc par bloc) et reference du gain auto.
    const float preRms  = blockRmsLevel (block);
    compressor.process (context);
    const float postRms = blockRmsLevel (block);
    gainReductionDb.store (juce::jmax (0.0f, juce::Decibels::gainToDecibels (preRms, -100.0f)
                                           - juce::Decibels::gainToDecibels (postRms, -100.0f)));

    if (getParam ("autoGain") > 0.5f)
    {
        // Reduction mesuree sur l'energie integree dans le temps (~100 ms) avant et apres le compresseur, donc la
        // meme quelle que soit la taille de bloc du DAW ; on n'en compense qu'une partie (0.9) pour rester naturel.
        const float smoothing = (float) std::exp (-(double) numSmp / (sampleRate * autoGainSeconds));
        inPower  = preRms * preRms + smoothing * (inPower - preRms * preRms);
        outPower = postRms * postRms + smoothing * (outPower - postRms * postRms);
        const float measuredReductionDb = juce::jmax (0.0f, 10.0f * std::log10 ((inPower + 1.0e-12f) / (outPower + 1.0e-12f)));
        smoothedAutoGainDb = juce::jlimit (0.0f, autoGainMaxDb, measuredReductionDb * 0.9f);
    }
    else
    {
        smoothedAutoGainDb = 0.0f;
        inPower = outPower = 0.0f;
    }

    outputGain.setGainDecibels (getParam ("output") + smoothedAutoGainDb);
    outputGain.process (context);

    mixer.mixWetSamples (block);
    sanitizeBlock (block);
}

} // namespace onirique
