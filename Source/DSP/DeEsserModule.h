#pragma once

#include "ProcessorModule.h"

namespace onirique
{

//==============================================================================
/** De-esser a bande dynamique double detection : une bande centree sur
    "frequency" (sifflantes "S", aigues) et une bande compagnon a ~1.6x cette
    frequence (couvre "SH"/"CH"/"T"/"Z", plus graves) sont surveillees
    ensemble ; la plus forte des deux pilote la reduction de gain, appliquee
    via deux filtres cloche dynamiques (reduction pleine sur la bande
    principale, attenuee sur la bande compagnon pour rester musical).

    Meme classe utilisee pour DE-ESSER 1 et DE-ESSER 2 (deux instances
    independantes, deux jeux de parametres, reglables separement). */
class DeEsserModule : public ProcessorModule
{
public:
    DeEsserModule (juce::AudioProcessorValueTreeState& state, const juce::String& moduleID,
                   const juce::String& displayName, int defaultOrder);

    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                               const juce::String& id, const juce::String& displayName,
                               int defaultOrder);

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void process (juce::dsp::AudioBlock<float>& block) override;
    void reset() override;
    void resetMeters() noexcept override { gainReductionDbMeter.store (0.0f); detectorLevelDbMeter.store (-100.0f); }

    /** Reduction de gain appliquee (dB, >= 0). */
    float getGainReductionDb() const noexcept { return gainReductionDbMeter.load(); }
    /** Niveau detecte (dB, enveloppe attack/release deja appliquee) --
        alimente le meter de niveau/threshold du De-Esser. */
    float getDetectorLevelDb() const noexcept { return detectorLevelDbMeter.load(); }

private:
    using DetectorFilter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                          juce::dsp::IIR::Coefficients<float>>;
    using DynamicBell    = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                          juce::dsp::IIR::Coefficients<float>>;

    DetectorFilter detectorMain, detectorCompanion;   // mesure de l'energie de sibilance
    DynamicBell    bellMain, bellCompanion;            // reduction appliquee au signal reel
    juce::dsp::DryWetMixer<float> mixer { 512 };

    juce::AudioBuffer<float> detectorBuffer;

    // Buffers du mode LISTEN (style Waves) : n'allouent/copient rien quand
    // le bouton n'est pas actif (voir process()). dryListenBuffer = signal
    // avant tout traitement, wetListenBuffer = signal apres les bandes
    // cloche dynamiques (avant mix dry/wet) -- la difference des deux isole
    // exactement ce que le de-esser retire, pour l'ecouter seul.
    juce::AudioBuffer<float> dryListenBuffer, wetListenBuffer;

    double lastFreq = -1.0;
    float  smoothedLevelDb = -100.0f;

    std::atomic<float> gainReductionDbMeter { 0.0f };
    std::atomic<float> detectorLevelDbMeter { -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeEsserModule)
};

} // namespace onirique
