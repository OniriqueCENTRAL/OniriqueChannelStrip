#pragma once

#include "ProcessorModule.h"
#include "DSPHelpers.h"
#include "SpectrumAnalyzer.h"
#include <array>

namespace onirique
{

//==============================================================================
/** EQ parametrique moderne a 8 bandes independantes. Chaque bande a sa
    frequence, son gain, son Q et son type (Bell / Low Shelf / High Shelf /
    Low Cut / High Cut / Notch), tous automatisables. Coefficients recalcules
    uniquement quand une bande change reellement (FilterParamCache).

    Integre un analyseur de spectre (voir SpectrumAnalyzer) qui observe le
    signal en sortie d'EQ : getSpectrumAnalyzer() donne acces en lecture pour
    une future interface graphique. */
class ParametricEQModule : public ProcessorModule
{
public:
    static constexpr int numBands = 8;

    enum BandType { bell = 0, lowShelf = 1, highShelf = 2, lowCut = 3, highCut = 4, notch = 5 };

    ParametricEQModule (juce::AudioProcessorValueTreeState& state, const juce::String& moduleID);

    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                               const juce::String& id);

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void process (juce::dsp::AudioBlock<float>& block) override;
    void reset() override;

    SpectrumAnalyzer& getSpectrumAnalyzer() noexcept { return analyzer; }
    const SpectrumAnalyzer& getSpectrumAnalyzer() const noexcept { return analyzer; }

    /** Reponse en magnitude (dB) de la somme des 8 bandes a une frequence
        donnee, en lisant les memes coefficients IIR que ceux appliques au
        signal. Non utilise par l'interface actuelle (style "MAAG EQ4" a
        potards, sans courbe a l'ecran), conserve pour un usage futur ou un
        hote qui interrogerait la reponse du plugin. Thread-safe pour un
        appel depuis le thread de l'interface : ne lit que des coefficients
        deja calcules (jamais de recalcul, jamais d'ecriture). */
    float getMagnitudeResponseDb (double frequencyHz) const noexcept;

private:
    using Band = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                juce::dsp::IIR::Coefficients<float>>;

    void updateBandIfNeeded (int bandIndex);

    std::array<Band, (size_t) numBands> bands;
    std::array<FilterParamCache, (size_t) numBands> caches;

    SpectrumAnalyzer analyzer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametricEQModule)
};

} // namespace onirique
