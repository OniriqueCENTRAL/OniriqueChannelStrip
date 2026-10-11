#pragma once

#include "ProcessorModule.h"

namespace onirique
{

//==============================================================================
/** Noise gate vocal avec hold et hysteresis, pense pour ne pas couper
    brutalement les fins de mots. Detection liee (un seul suivi de niveau
    pour tous les canaux, applique au meme gain partout : pas de derive
    stereo). Implementation originale, machine a etats + suivi d'enveloppe,
    pas de filtre IIR a reconstruire -> cout CPU minimal. */
class NoiseGateModule : public ProcessorModule
{
public:
    NoiseGateModule (juce::AudioProcessorValueTreeState& state, const juce::String& moduleID);

    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                               const juce::String& id);

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void process (juce::dsp::AudioBlock<float>& block) override;
    void reset() override;
    void resetMeters() noexcept override { gainReductionDb.store (0.0f); }

    /** Reduction de gain appliquee (dB, >= 0) -- interface uniquement
        (style NS1 : meter sous le potard de threshold unique). */
    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }

private:
    double sr = 44100.0;

    // Suivi de niveau (detection rapide et stable, non audible directement)
    float detectorEnvelope = 0.0f;

    // Gain applique (ce que l'on entend), lisse par attack/release utilisateur
    float currentGainLin = 0.0f;
    bool  lastTargetOpen = false;
    int   holdCounter = 0;

    std::atomic<float> gainReductionDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseGateModule)
};

} // namespace onirique
