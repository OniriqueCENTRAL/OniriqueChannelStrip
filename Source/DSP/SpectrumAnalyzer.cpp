#include "SpectrumAnalyzer.h"
#include <cmath>

namespace onirique
{

void SpectrumAnalyzer::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    reset();
}

void SpectrumAnalyzer::reset()
{
    fifo.fill (0.0f);
    fftWorkBuffer.fill (0.0f);
    fifoIndex = 0;
    hasData = false;

    const juce::SpinLock::ScopedLockType sl (lock);
    readyMagnitudesDb.fill (-100.0f);
}

void SpectrumAnalyzer::pushSample (float sample) noexcept
{
    if (fifoIndex == fftSize)
    {
        computeFrame();
        fifoIndex = 0;
    }

    fifo[(size_t) fifoIndex++] = std::isfinite (sample) ? sample : 0.0f;
}

void SpectrumAnalyzer::computeFrame()
{
    // Buffer de travail : signal fenetre dans la premiere moitie, le reste
    // sert de scratch pour la FFT (impose par l'API JUCE).
    std::copy (fifo.begin(), fifo.end(), fftWorkBuffer.begin());
    std::fill (fftWorkBuffer.begin() + fftSize, fftWorkBuffer.end(), 0.0f);

    window.multiplyWithWindowingTable (fftWorkBuffer.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftWorkBuffer.data());

    const juce::SpinLock::ScopedLockType sl (lock);

    for (int i = 0; i < numBins; ++i)
    {
        const float magnitude = fftWorkBuffer[(size_t) i] / (float) fftSize;
        readyMagnitudesDb[(size_t) i] = juce::Decibels::gainToDecibels (magnitude, -100.0f);
    }

    hasData = true;
}

bool SpectrumAnalyzer::getMagnitudesInDb (MagnitudeArray& dest) const
{
    if (! hasData.load())
        return false;

    const juce::SpinLock::ScopedLockType sl (lock);
    dest = readyMagnitudesDb;
    return true;
}

} // namespace onirique
