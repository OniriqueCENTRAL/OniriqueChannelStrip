#include "ParametricEQModule.h"
#include "../Parameters.h"

namespace onirique
{

namespace
{
    struct BandDefault { const char* name; float freq; int type; };

    // Disposition par defaut : coupe-bas utilitaire, shelf bas, 4 bandes
    // cloche au centre, shelf haut, coupe-haut utilitaire -- une base
    // polyvalente typique d'un EQ moderne 8 bandes.
    constexpr BandDefault bandDefaults[ParametricEQModule::numBands] = {
        { "Band 1",  80.0f,    ParametricEQModule::lowCut   },
        { "Band 2",  150.0f,   ParametricEQModule::lowShelf },
        { "Band 3",  400.0f,   ParametricEQModule::bell     },
        { "Band 4",  900.0f,   ParametricEQModule::bell     },
        { "Band 5",  2500.0f,  ParametricEQModule::bell     },
        { "Band 6",  5000.0f,  ParametricEQModule::bell     },
        { "Band 7",  9000.0f,  ParametricEQModule::highShelf},
        { "Band 8",  14000.0f, ParametricEQModule::highCut  },
    };
}

ParametricEQModule::ParametricEQModule (juce::AudioProcessorValueTreeState& state, const juce::String& moduleID)
    : ProcessorModule (state, moduleID)
{
}

void ParametricEQModule::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                                        const juce::String& id)
{
    addCommonParameters (layout, id, "EQ", numModules, 7);

    for (int b = 0; b < numBands; ++b)
    {
        const auto prefix = id + "_band" + juce::String (b + 1);
        const auto def = bandDefaults[(size_t) b];

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (prefix + "_freq", 1), juce::String (def.name) + " Freq",
            freqRange (20.0f, 20000.0f), def.freq, hzAttrib()));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (prefix + "_gain", 1), juce::String (def.name) + " Gain",
            juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, dbAttrib()));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (prefix + "_q", 1), juce::String (def.name) + " Q",
            juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.5f), 0.707f));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (prefix + "_type", 1), juce::String (def.name) + " Type",
            juce::StringArray { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch" },
            def.type));
    }
}

void ParametricEQModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    for (auto& band : bands)
        band.prepare (spec);

    analyzer.prepare (spec.sampleRate);

    // Chauffe le cache de parametres (32 suffixes : 8 bandes x 4), hors du
    // thread audio.
    for (int b = 0; b < numBands; ++b)
    {
        const auto prefix = "band" + juce::String (b + 1) + "_";
        warmUpParam (prefix + "freq");
        warmUpParam (prefix + "gain");
        warmUpParam (prefix + "q");
        warmUpParam (prefix + "type");
    }

    reset();
}

void ParametricEQModule::reset()
{
    for (auto& band : bands)
        band.reset();

    for (auto& c : caches)
        c = FilterParamCache();

    analyzer.reset();
}

void ParametricEQModule::updateBandIfNeeded (int b)
{
    const auto prefix = "band" + juce::String (b + 1) + "_";

    const auto freq = safeFilterFreq (getParam (prefix + "freq"), sampleRate);
    const auto gain = getParam (prefix + "gain");
    const auto q    = safeQ (getParam (prefix + "q"));
    const auto type = (float) (int) getParam (prefix + "type");

    if (! caches[(size_t) b].update (freq, gain, q, type))
        return;   // rien n'a change : pas de recalcul de coefficients

    const auto gainFactor = juce::Decibels::decibelsToGain (gain);
    auto& state = bands[(size_t) b].state;

    switch ((int) type)
    {
        case lowShelf:  *state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sampleRate, freq, q, gainFactor); break;
        case highShelf: *state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, freq, q, gainFactor); break;
        case lowCut:    *state = *juce::dsp::IIR::Coefficients<float>::makeHighPass  (sampleRate, freq, q); break;
        case highCut:   *state = *juce::dsp::IIR::Coefficients<float>::makeLowPass   (sampleRate, freq, q); break;
        case notch:     *state = *juce::dsp::IIR::Coefficients<float>::makeNotch     (sampleRate, freq, q); break;
        default:        *state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, freq, q, gainFactor); break;
    }
}

void ParametricEQModule::process (juce::dsp::AudioBlock<float>& block)
{
    juce::dsp::ProcessContextReplacing<float> context (block);

    for (int b = 0; b < numBands; ++b)
    {
        updateBandIfNeeded (b);
        bands[(size_t) b].process (context);
    }

    sanitizeBlock (block);

    // Alimente l'analyseur de spectre avec le signal post-EQ, reduit a un
    // seul canal (moyenne) : suffisant pour une vue spectrale globale.
    const auto numCh  = block.getNumChannels();
    const auto numSmp = block.getNumSamples();

    for (size_t s = 0; s < numSmp; ++s)
    {
        float sum = 0.0f;
        for (size_t ch = 0; ch < numCh; ++ch)
            sum += block.getChannelPointer (ch)[s];

        analyzer.pushSample (numCh > 0 ? sum / (float) numCh : 0.0f);
    }
}

float ParametricEQModule::getMagnitudeResponseDb (double frequencyHz) const noexcept
{
    double totalMagnitude = 1.0;

    for (auto& band : bands)
        if (band.state != nullptr)
            totalMagnitude *= band.state->getMagnitudeForFrequency (frequencyHz, sampleRate > 0.0 ? sampleRate : 44100.0);

    return (float) juce::Decibels::gainToDecibels (totalMagnitude, -100.0);
}

} // namespace onirique
