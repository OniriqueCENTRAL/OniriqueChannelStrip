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

    constexpr float autoGainSmoothCoeff = 0.90f;   // lissage inter-blocs de la compensation
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
    mixer.prepare (spec);

    dryLevelBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);

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
    gainReductionDb.store (0.0f);
}

void FETCompressorModule::process (juce::dsp::AudioBlock<float>& block)
{
    const auto numCh  = (int) block.getNumChannels();
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

    // Niveau d'entree (post-trim, pre-compression) : reference pour mesurer
    // la reduction de gain reellement appliquee par le compresseur.
    const bool autoGainOn = getParam ("autoGain") > 0.5f;
    float inputRmsDb = -100.0f;

    if (autoGainOn)
    {
        dryLevelBuffer.setSize (numCh, numSmp, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
            dryLevelBuffer.copyFrom (ch, 0, block.getChannelPointer ((size_t) ch), numSmp);

        float sumSq = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float r = dryLevelBuffer.getRMSLevel (ch, 0, numSmp);
            sumSq += r * r;
        }
        inputRmsDb = juce::Decibels::gainToDecibels (
            std::sqrt (sumSq / (float) juce::jmax (1, numCh)), -100.0f);
    }

    // Mesure de la reduction de gain (interface uniquement) : RMS avant/
    // apres le compresseur, independamment de l'auto-gain.
    const float preCompDb = juce::Decibels::gainToDecibels (blockRmsLevel (block), -100.0f);
    compressor.process (context);
    const float postCompDbForMeter = juce::Decibels::gainToDecibels (blockRmsLevel (block), -100.0f);
    gainReductionDb.store (juce::jmax (0.0f, preCompDb - postCompDbForMeter));

    if (autoGainOn)
    {
        const float outputRmsDb = juce::Decibels::gainToDecibels (blockRmsLevel (block), -100.0f);

        // Reduction mesuree = combien le niveau a baisse pendant ce bloc ;
        // on ne compense qu'une partie (0.9) pour rester naturel, et on
        // lisse pour eviter le pompage si la reduction varie vite.
        const float measuredReductionDb = juce::jmax (0.0f, inputRmsDb - outputRmsDb);
        const float targetCompDb = juce::jlimit (0.0f, autoGainMaxDb, measuredReductionDb * 0.9f);

        smoothedAutoGainDb = targetCompDb + autoGainSmoothCoeff * (smoothedAutoGainDb - targetCompDb);
    }
    else
    {
        smoothedAutoGainDb = 0.0f;
    }

    outputGain.setGainDecibels (getParam ("output") + smoothedAutoGainDb);
    outputGain.process (context);

    mixer.mixWetSamples (block);
    sanitizeBlock (block);
}

} // namespace onirique
