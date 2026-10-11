#include "SSLChannelModule.h"
#include "../Parameters.h"
#include <cmath>

namespace onirique
{

namespace
{
    constexpr float gainRampSeconds = 0.015f;

    inline float msToCoeff (float ms, double sampleRate) noexcept
    {
        ms = juce::jmax (0.01f, ms);
        return std::exp (-1.0f / ((float) sampleRate * (ms * 0.001f)));
    }
}

SSLChannelModule::SSLChannelModule (juce::AudioProcessorValueTreeState& state, const juce::String& moduleID)
    : ProcessorModule (state, moduleID)
{
}

void SSLChannelModule::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                                      const juce::String& id)
{
    addCommonParameters (layout, id, "SSL Channel", numModules, 2);

    // --- Input / Output -------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_input", 1), "Input Trim",
        juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, dbAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_output", 1), "Output",
        juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, dbAttrib()));

    // --- Filtre coupe-bas --------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_hpf", 1), "HPF",
        freqRange (20.0f, 500.0f), 20.0f, hzAttrib()));

    // --- EQ 4 bandes ---------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_lowFreq", 1), "Low Shelf Freq",
        freqRange (30.0f, 450.0f), 100.0f, hzAttrib()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_lowGain", 1), "Low Shelf Gain",
        juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f, dbAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_lowMidFreq", 1), "Low-Mid Freq",
        freqRange (200.0f, 2500.0f), 600.0f, hzAttrib()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_lowMidGain", 1), "Low-Mid Gain",
        juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f, dbAttrib()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_lowMidQ", 1), "Low-Mid Q",
        juce::NormalisableRange<float> (0.3f, 5.0f, 0.01f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_highMidFreq", 1), "High-Mid Freq",
        freqRange (600.0f, 7000.0f), 3000.0f, hzAttrib()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_highMidGain", 1), "High-Mid Gain",
        juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f, dbAttrib()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_highMidQ", 1), "High-Mid Q",
        juce::NormalisableRange<float> (0.3f, 5.0f, 0.01f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_highFreq", 1), "High Shelf Freq",
        freqRange (1500.0f, 20000.0f), 8000.0f, hzAttrib()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_highGain", 1), "High Shelf Gain",
        juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f, dbAttrib()));

    // --- Compresseur bus-style -------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_threshold", 1), "Comp Threshold",
        juce::NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -10.0f, dbAttrib()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_ratio", 1), "Comp Ratio",
        juce::NormalisableRange<float> (1.0f, 10.0f, 0.01f, 0.5f), 4.0f,
        juce::AudioParameterFloatAttributes().withLabel (":1")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_attack", 1), "Comp Attack",
        juce::NormalisableRange<float> (0.1f, 100.0f, 0.01f, 0.4f), 10.0f, msAttrib()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_release", 1), "Comp Release",
        juce::NormalisableRange<float> (10.0f, 1000.0f, 1.0f, 0.3f), 100.0f, msAttrib()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_makeup", 1), "Comp Makeup",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dbAttrib()));

    // --- Gate / expander -----------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_gateThreshold", 1), "Gate Threshold",
        juce::NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -80.0f, dbAttrib()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_gateRange", 1), "Gate Range",
        juce::NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -20.0f, dbAttrib()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_gateAttack", 1), "Gate Attack",
        juce::NormalisableRange<float> (0.1f, 50.0f, 0.01f, 0.4f), 1.0f, msAttrib()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_gateRelease", 1), "Gate Release",
        juce::NormalisableRange<float> (5.0f, 500.0f, 1.0f, 0.4f), 100.0f, msAttrib()));
}

void SSLChannelModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    hpf.prepare (spec);
    lowShelf.prepare (spec);
    lowMid.prepare (spec);
    highMid.prepare (spec);
    highShelf.prepare (spec);
    compressor.prepare (spec);

    trimGain.prepare (spec);
    makeupGain.prepare (spec);
    outputGain.prepare (spec);
    trimGain.setRampDurationSeconds   (gainRampSeconds);
    makeupGain.setRampDurationSeconds (gainRampSeconds);
    outputGain.setRampDurationSeconds (gainRampSeconds);

    // Chauffe le cache de parametres (voir ProcessorModule::getParam), hors
    // du thread audio : aucune allocation/recherche par nom dans process().
    for (const char* suffix : { "input", "output", "hpf",
                                "lowFreq", "lowGain", "lowMidFreq", "lowMidGain", "lowMidQ",
                                "highMidFreq", "highMidGain", "highMidQ", "highFreq", "highGain",
                                "threshold", "ratio", "attack", "release", "makeup",
                                "gateThreshold", "gateRange", "gateAttack", "gateRelease" })
        warmUpParam (suffix);

    reset();
}

void SSLChannelModule::reset()
{
    hpf.reset(); lowShelf.reset(); lowMid.reset(); highMid.reset(); highShelf.reset();
    compressor.reset();
    trimGain.reset(); makeupGain.reset(); outputGain.reset();

    hpfCache = lowCache = lowMidCache = highMidCache = highCache = FilterParamCache();
    gateEnvelope = 0.0f;
    gateGainLin  = 1.0f;
    gainReductionDb.store (0.0f);
}

void SSLChannelModule::updateFiltersIfNeeded()
{
    const auto sr = sampleRate;

    const auto hpfFreq = safeFilterFreq (getParam ("hpf"), sr);
    if (hpfCache.update (hpfFreq, 0.0f, 0.707f))
        *hpf.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, hpfFreq, 0.707f);

    const auto lowFreq = safeFilterFreq (getParam ("lowFreq"), sr);
    const auto lowGain = getParam ("lowGain");
    if (lowCache.update (lowFreq, lowGain, 0.707f))
        *lowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sr, lowFreq, 0.707f, juce::Decibels::decibelsToGain (lowGain));

    const auto lowMidFreq = safeFilterFreq (getParam ("lowMidFreq"), sr);
    const auto lowMidGain = getParam ("lowMidGain");
    const auto lowMidQ    = safeQ (getParam ("lowMidQ"));
    if (lowMidCache.update (lowMidFreq, lowMidGain, lowMidQ))
        *lowMid.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sr, lowMidFreq, lowMidQ, juce::Decibels::decibelsToGain (lowMidGain));

    const auto highMidFreq = safeFilterFreq (getParam ("highMidFreq"), sr);
    const auto highMidGain = getParam ("highMidGain");
    const auto highMidQ    = safeQ (getParam ("highMidQ"));
    if (highMidCache.update (highMidFreq, highMidGain, highMidQ))
        *highMid.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sr, highMidFreq, highMidQ, juce::Decibels::decibelsToGain (highMidGain));

    const auto highFreq = safeFilterFreq (getParam ("highFreq"), sr);
    const auto highGain = getParam ("highGain");
    if (highCache.update (highFreq, highGain, 0.707f))
        *highShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sr, highFreq, 0.707f, juce::Decibels::decibelsToGain (highGain));
}

void SSLChannelModule::processGateExpander (juce::dsp::AudioBlock<float>& block)
{
    const float thresholdDb = getParam ("gateThreshold");
    const float rangeDb     = getParam ("gateRange");
    const float attackCoeff  = msToCoeff (getParam ("gateAttack"),  sampleRate);
    const float releaseCoeff = msToCoeff (getParam ("gateRelease"), sampleRate);

    const auto numCh  = block.getNumChannels();
    const auto numSmp = block.getNumSamples();

    for (size_t s = 0; s < numSmp; ++s)
    {
        float peak = 0.0f;
        for (size_t ch = 0; ch < numCh; ++ch)
            peak = juce::jmax (peak, std::abs (block.getChannelPointer (ch)[s]));

        // Suivi de niveau rapide et stable (meme principe que le noise gate).
        const float detCoeff = (peak > gateEnvelope) ? 0.3f : 0.9995f;
        gateEnvelope = peak + detCoeff * (gateEnvelope - peak);

        const float levelDb = juce::Decibels::gainToDecibels (gateEnvelope, -120.0f);

        // Expansion douce sous le seuil (pas de coupure franche comme un gate dur).
        const float overshoot   = juce::jmax (0.0f, thresholdDb - levelDb);
        const float reductionDb = juce::jlimit (0.0f, -rangeDb, overshoot);
        const float targetGain  = juce::Decibels::decibelsToGain (-reductionDb);

        const float gainCoeff = (targetGain > gateGainLin) ? attackCoeff : releaseCoeff;
        gateGainLin = targetGain + gainCoeff * (gateGainLin - targetGain);

        for (size_t ch = 0; ch < numCh; ++ch)
        {
            auto* samples = block.getChannelPointer (ch);
            samples[s] = sanitizeSample (samples[s] * gateGainLin);
        }
    }
}

void SSLChannelModule::process (juce::dsp::AudioBlock<float>& block)
{
    updateFiltersIfNeeded();

    juce::dsp::ProcessContextReplacing<float> context (block);

    trimGain.setGainDecibels (getParam ("input"));
    trimGain.process (context);

    hpf.process (context);
    lowShelf.process (context);
    lowMid.process (context);
    highMid.process (context);
    highShelf.process (context);

    compressor.setThreshold (getParam ("threshold"));
    compressor.setRatio      (getParam ("ratio"));
    compressor.setAttack     (getParam ("attack"));
    compressor.setRelease    (getParam ("release"));

    // Mesure de la reduction de gain (interface uniquement, n'affecte pas le
    // traitement) : RMS avant/apres le compresseur.
    const float preCompDb = juce::Decibels::gainToDecibels (blockRmsLevel (block), -100.0f);
    compressor.process (context);
    const float postCompDb = juce::Decibels::gainToDecibels (blockRmsLevel (block), -100.0f);
    gainReductionDb.store (juce::jmax (0.0f, preCompDb - postCompDb));

    makeupGain.setGainDecibels (getParam ("makeup"));
    makeupGain.process (context);

    processGateExpander (block);

    outputGain.setGainDecibels (getParam ("output"));
    outputGain.process (context);
}

} // namespace onirique
