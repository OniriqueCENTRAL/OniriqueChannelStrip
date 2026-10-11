#include "NoiseGateModule.h"
#include "DSPHelpers.h"
#include "../Parameters.h"
#include <cmath>

namespace onirique
{

namespace
{
    // Ballistique fixe du detecteur de niveau : rapide mais pas instantanee,
    // pour eviter que le gate "voie" du bruit haute-frequence comme un pic
    // isole. Ce n'est pas le gain audible (controle par attack/release
    // utilisateur), juste la mesure qui decide si on est au-dessus du seuil.
    constexpr float detectorAttackMs  = 0.5f;
    constexpr float detectorReleaseMs = 8.0f;

    inline float msToCoeff (float ms, double sampleRate) noexcept
    {
        ms = juce::jmax (0.01f, ms);
        return std::exp (-1.0f / ((float) sampleRate * (ms * 0.001f)));
    }
}

NoiseGateModule::NoiseGateModule (juce::AudioProcessorValueTreeState& state, const juce::String& moduleID)
    : ProcessorModule (state, moduleID)
{
}

void NoiseGateModule::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                                     const juce::String& id)
{
    addCommonParameters (layout, id, "Noise Gate", numModules, 1);

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_threshold", 1), "Gate Threshold",
        juce::NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -45.0f, dbAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_attack", 1), "Gate Attack",
        juce::NormalisableRange<float> (0.1f, 50.0f, 0.01f, 0.4f), 2.0f, msAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_hold", 1), "Gate Hold",
        juce::NormalisableRange<float> (0.0f, 500.0f, 1.0f, 0.5f), 60.0f, msAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_release", 1), "Gate Release",
        juce::NormalisableRange<float> (5.0f, 2000.0f, 1.0f, 0.3f), 180.0f, msAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_range", 1), "Gate Range",
        juce::NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -60.0f, dbAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_hysteresis", 1), "Gate Hysteresis",
        juce::NormalisableRange<float> (0.0f, 20.0f, 0.1f), 3.0f, dbAttrib()));
}

void NoiseGateModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sr = spec.sampleRate;

    // Chauffe le cache de parametres (voir ProcessorModule::getParam) : ces
    // lectures, seules a pouvoir allouer/chercher par nom, se font ici, hors
    // du thread audio -- process() ne fera plus que des lectures atomiques.
    warmUpParam ("threshold");
    warmUpParam ("hysteresis");
    warmUpParam ("range");
    warmUpParam ("attack");
    warmUpParam ("release");
    warmUpParam ("hold");

    reset();
}

void NoiseGateModule::reset()
{
    detectorEnvelope = 0.0f;
    currentGainLin   = 0.0f;   // ferme au demarrage : pas de souffle ni de pop
    lastTargetOpen   = false;
    holdCounter      = 0;
    gainReductionDb.store (0.0f);
}

void NoiseGateModule::process (juce::dsp::AudioBlock<float>& block)
{
    const float thresholdDb  = getParam ("threshold");
    const float hysteresisDb = getParam ("hysteresis");
    const float closeDb      = thresholdDb - hysteresisDb;
    const float rangeDb      = getParam ("range");
    const float rangeGainLin = juce::Decibels::decibelsToGain (rangeDb);

    const float attackCoeff  = msToCoeff (getParam ("attack"),  sr);
    const float releaseCoeff = msToCoeff (getParam ("release"), sr);
    const float detAttackCoeff  = msToCoeff (detectorAttackMs,  sr);
    const float detReleaseCoeff = msToCoeff (detectorReleaseMs, sr);

    const int holdSamples = (int) ((getParam ("hold") * 0.001) * sr);

    const auto numCh  = block.getNumChannels();
    const auto numSmp = block.getNumSamples();

    for (size_t s = 0; s < numSmp; ++s)
    {
        // 1) Niveau instantane (pic, detection liee tous canaux confondus)
        float peak = 0.0f;
        for (size_t ch = 0; ch < numCh; ++ch)
            peak = juce::jmax (peak, std::abs (block.getChannelPointer (ch)[s]));

        const float detCoeff = (peak > detectorEnvelope) ? detAttackCoeff : detReleaseCoeff;
        detectorEnvelope = peak + detCoeff * (detectorEnvelope - peak);

        const float levelDb = juce::Decibels::gainToDecibels (detectorEnvelope, -120.0f);

        // 2) Machine a etats seuil + hysteresis + hold
        bool targetOpen = lastTargetOpen;

        if (levelDb >= thresholdDb)
        {
            targetOpen  = true;
            holdCounter = holdSamples;
        }
        else if (levelDb < closeDb)
        {
            if (holdCounter > 0)
                --holdCounter;
            else
                targetOpen = false;
        }
        // entre closeDb et thresholdDb : on garde l'etat precedent (hysteresis)

        lastTargetOpen = targetOpen;

        // 3) Gain lisse (attack/release utilisateur = vitesse audible du gate)
        const float targetGain = targetOpen ? 1.0f : rangeGainLin;
        const float gainCoeff  = (targetGain > currentGainLin) ? attackCoeff : releaseCoeff;
        currentGainLin = targetGain + gainCoeff * (currentGainLin - targetGain);

        for (size_t ch = 0; ch < numCh; ++ch)
        {
            auto* samples = block.getChannelPointer (ch);
            samples[s] = sanitizeSample (samples[s] * currentGainLin);
        }
    }

    gainReductionDb.store (juce::jmax (0.0f, -juce::Decibels::gainToDecibels (currentGainLin, -80.0f)));
}

} // namespace onirique
