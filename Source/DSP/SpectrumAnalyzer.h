#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace onirique
{

//==============================================================================
/** Analyseur de spectre temps reel (backend, sans affichage). Alimente
    echantillon par echantillon depuis le thread audio (pushSample), il
    calcule une FFT fenetree des que 2048 echantillons se sont accumules et
    publie le resultat de maniere thread-safe pour une future interface
    graphique (getMagnitudesInDb, a appeler depuis un Timer cote UI).

    Choix volontairement simple et peu couteux : une seule FFT par bloc de
    2048 echantillons (~46 ms a 44.1 kHz, soit ~21 rafraichissements/s), pas
    de recouvrement entre blocs. Suffisant pour une visualisation fluide sans
    charger le thread audio. */
class SpectrumAnalyzer
{
public:
    static constexpr int fftOrder = 11;              // 2^11 = 2048 points
    static constexpr int fftSize  = 1 << fftOrder;
    static constexpr int numBins  = fftSize / 2;      // bins utiles (hors Nyquist/miroir)

    using MagnitudeArray = std::array<float, (size_t) numBins>;

    SpectrumAnalyzer() = default;

    void prepare (double newSampleRate);
    void reset();

    /** A appeler une fois par echantillon (signal deja reduit a un seul
        canal, ex: moyenne des canaux) depuis le thread audio. */
    void pushSample (float sample) noexcept;

    /** Copie le dernier spectre pret (magnitudes en dB) dans dest. Renvoie
        false si aucune donnee n'est encore disponible. Peut etre appele
        depuis le thread de l'interface pendant que l'audio tourne. */
    bool getMagnitudesInDb (MagnitudeArray& dest) const;

    double getSampleRate() const noexcept { return sampleRate; }

private:
    void computeFrame();

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize,
                                                 juce::dsp::WindowingFunction<float>::hann };

    std::array<float, (size_t) fftSize> fifo {};
    std::array<float, (size_t) fftSize * 2> fftWorkBuffer {};
    int fifoIndex = 0;

    MagnitudeArray readyMagnitudesDb {};
    std::atomic<bool> hasData { false };
    mutable juce::SpinLock lock;

    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};

} // namespace onirique
