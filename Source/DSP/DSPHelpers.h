#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace onirique
{

//==============================================================================
//  Parametres : ranges / labels
//==============================================================================

/** Range logarithmique perceptuelle pour un parametre de frequence
    (curseur centre sur la moyenne geometrique de la plage). */
inline juce::NormalisableRange<float> freqRange (float lo, float hi)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (std::sqrt (lo * hi));
    return r;
}

inline juce::AudioParameterFloatAttributes dbAttrib()  { return juce::AudioParameterFloatAttributes().withLabel ("dB"); }
inline juce::AudioParameterFloatAttributes hzAttrib()  { return juce::AudioParameterFloatAttributes().withLabel ("Hz"); }
inline juce::AudioParameterFloatAttributes msAttrib()  { return juce::AudioParameterFloatAttributes().withLabel ("ms"); }
inline juce::AudioParameterFloatAttributes pctAttrib() { return juce::AudioParameterFloatAttributes().withLabel ("%"); }

//==============================================================================
//  Securite numerique
//==============================================================================

/** Remplace NaN/Inf par 0 et ecrete a une valeur raisonnable. A utiliser en
    sortie de chaque etage un peu "risque" (saturation, compresseurs avec
    gains extremes...) et systematiquement en sortie du plugin. */
inline float sanitizeSample (float x) noexcept
{
    if (! std::isfinite (x))
        return 0.0f;

    return juce::jlimit (-8.0f, 8.0f, x);
}

inline void sanitizeBlock (juce::dsp::AudioBlock<float>& block) noexcept
{
    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
    {
        auto* samples = block.getChannelPointer (ch);
        for (size_t s = 0; s < block.getNumSamples(); ++s)
            samples[s] = sanitizeSample (samples[s]);
    }
}

/** Clamp une frequence de filtre a une plage sure vis-a-vis de la frequence
    d'echantillonnage courante (evite les coefficients instables pres de
    Nyquist, et les frequences nulles/negatives). */
inline float safeFilterFreq (float freq, double sampleRate) noexcept
{
    const float nyquistMargin = (float) sampleRate * 0.45f;
    return juce::jlimit (10.0f, juce::jmax (20.0f, nyquistMargin), freq);
}

inline float safeQ (float q) noexcept
{
    return juce::jlimit (0.05f, 25.0f, q);
}

/** RMS combine (tous canaux, tout le bloc) d'un juce::dsp::AudioBlock.
    NOTE : AudioBlock, contrairement a AudioBuffer, n'expose pas
    getRMSLevel() -- cette fonction comble ce manque pour les modules qui
    mesurent un niveau de bloc directement sur l'AudioBlock recu dans
    process(), sans passer par un AudioBuffer intermediaire. */
inline float blockRmsLevel (const juce::dsp::AudioBlock<float>& block) noexcept
{
    const auto numCh  = block.getNumChannels();
    const auto numSmp = block.getNumSamples();

    if (numCh == 0 || numSmp == 0)
        return 0.0f;

    double sumSquares = 0.0;
    for (size_t ch = 0; ch < numCh; ++ch)
    {
        const auto* data = block.getChannelPointer (ch);
        for (size_t s = 0; s < numSmp; ++s)
            sumSquares += (double) data[s] * (double) data[s];
    }

    return (float) std::sqrt (sumSquares / (double) (numCh * numSmp));
}

//==============================================================================
//  Recalcul des coefficients uniquement quand necessaire
//==============================================================================

/** Petit cache a 4 flottants (freq/gain/Q/type) utilise pour eviter de
    reconstruire des coefficients de filtre IIR a chaque bloc alors que les
    parametres n'ont pas bouge : operation couteuse (quelques divisions/
    trigonometrie) inutile la plupart du temps, puisque l'auditeur ne tripote
    qu'un parametre a la fois. */
struct FilterParamCache
{
    float freq = -1.0f, gain = -1.0f, q = -1.0f, type = -1.0f;

    /** Renvoie true si au moins une valeur a change de plus de l'epsilon,
        et met a jour le cache dans ce cas. */
    bool update (float newFreq, float newGain, float newQ, float newType = 0.0f) noexcept
    {
        constexpr float eps = 1.0e-4f;

        const bool changed = std::abs (newFreq - freq) > eps
                           || std::abs (newGain - gain) > eps
                           || std::abs (newQ - q) > eps
                           || std::abs (newType - type) > eps;

        if (changed)
        {
            freq = newFreq; gain = newGain; q = newQ; type = newType;
        }

        return changed;
    }
};

} // namespace onirique
