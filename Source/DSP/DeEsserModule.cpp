#include "DeEsserModule.h"
#include "DSPHelpers.h"
#include "../Parameters.h"
#include <cmath>

namespace onirique
{

namespace
{
    constexpr float detectionQ    = 3.0f;    // selectivite des filtres de detection
    constexpr float dynamicBellQ  = 2.5f;    // selectivite de la reduction (bande principale)
    constexpr float companionQ    = 2.0f;    // selectivite de la bande compagnon (plus large)
    constexpr float companionRatioFreq = 1.6f; // position de la bande compagnon par rapport a "frequency"
    constexpr float companionDepth = 0.55f;  // reduction appliquee sur la bande compagnon (fraction de la principale)
}

DeEsserModule::DeEsserModule (juce::AudioProcessorValueTreeState& state, const juce::String& moduleID,
                              const juce::String& displayName, int defaultOrder)
    : ProcessorModule (state, moduleID)
{
    juce::ignoreUnused (displayName, defaultOrder);   // utilises par addParameters()
}

void DeEsserModule::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                                   const juce::String& id, const juce::String& displayName,
                                   int defaultOrder)
{
    addCommonParameters (layout, id, displayName, numModules, defaultOrder);

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_frequency", 1), displayName + " Freq",
        freqRange (2000.0f, 16000.0f), 6000.0f, hzAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_threshold", 1), displayName + " Threshold",
        juce::NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -20.0f, dbAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_range", 1), displayName + " Range",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 12.0f, dbAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_attack", 1), displayName + " Attack",
        juce::NormalisableRange<float> (0.1f, 50.0f, 0.01f, 0.4f), 2.0f, msAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_release", 1), displayName + " Release",
        juce::NormalisableRange<float> (5.0f, 500.0f, 1.0f, 0.4f), 100.0f, msAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_mix", 1), displayName + " Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pctAttrib()));

    // Mode LISTEN (style Waves) : isole ce que le de-esser retire, pour
    // l'entendre seul -- n'affecte le signal de sortie que lorsqu'il est
    // actif (voir process()), sans toucher au comportement normal.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID (id + "_listen", 1), displayName + " Listen", false));
}

void DeEsserModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    detectorMain.prepare (spec);
    detectorCompanion.prepare (spec);
    bellMain.prepare (spec);
    bellCompanion.prepare (spec);
    mixer.prepare (spec);

    // tranches d'1 ms (voir process()) : ces deux tampons n'ont jamais besoin d'etre plus longs
    const int slice = juce::jmax (16, juce::roundToInt (sampleRate * 0.001));
    detectorBuffer.setSize ((int) spec.numChannels, slice);
    companionBuffer.setSize ((int) spec.numChannels, slice);
    dryListenBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);   // LISTEN : rien a allouer en jeu
    wetListenBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);

    // Chauffe le cache de parametres, hors du thread audio.
    for (const char* suffix : { "frequency", "threshold", "range", "attack", "release", "mix", "listen" })
        warmUpParam (suffix);

    reset();
}

void DeEsserModule::reset()
{
    detectorMain.reset();
    detectorCompanion.reset();
    bellMain.reset();
    bellCompanion.reset();
    mixer.reset();
    lastFreq = -1.0;
    smoothedLevelDb = -100.0f;
    mainMeanSquare = companionMeanSquare = 0.0f;
    gainReductionDbMeter.store (0.0f);
    detectorLevelDbMeter.store (-100.0f);
}

void DeEsserModule::process (juce::dsp::AudioBlock<float>& block)
{
    const auto freq      = getParam ("frequency");
    const auto thresh    = getParam ("threshold");
    const auto rangeDb   = getParam ("range");
    const auto attackMs  = getParam ("attack");
    const auto releaseMs = getParam ("release");
    const auto mixPct    = getParam ("mix");
    const bool listen    = getParam ("listen") > 0.5f;

    const auto numCh  = (int) block.getNumChannels();
    const auto numSmp = (int) block.getNumSamples();

    mixer.setWetMixProportion (mixPct / 100.0f);
    mixer.pushDrySamples (block);

    // Snapshot du signal d'entree pour le mode LISTEN (voir plus bas) --
    // copie seulement si le bouton est actif, cout nul sinon.
    if (listen)
    {
        dryListenBuffer.setSize (numCh, numSmp, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
            dryListenBuffer.copyFrom (ch, 0, block.getChannelPointer ((size_t) ch), numSmp);
    }

    // 1) Filtres de detection sur les deux bandes surveillees (sifflantes aigues + consonnes plus graves).
    //    Coefficients en tableau : aucune allocation sur le thread audio.
    const float companionFreq = safeFilterFreq (freq * companionRatioFreq, sampleRate);
    const float safeFreq      = safeFilterFreq (freq, sampleRate);

    if (std::abs ((double) safeFreq - lastFreq) > 0.5)
    {
        lastFreq = safeFreq;
        *detectorMain.state      = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass (sampleRate, safeFreq, detectionQ);
        *detectorCompanion.state = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass (sampleRate, companionFreq, detectionQ);
    }

    // 2) Suivi par tranches d'1 ms, quelle que soit la taille de bloc du DAW : la reduction suit chaque sifflante
    //    au lieu de changer une fois par bloc (11 ms a 512, 43 ms a 2048). L'energie de chaque bande est integree
    //    echantillon par echantillon (~5 ms, l'equivalent de l'ancienne mesure sur un bloc de 512), on garde la
    //    plus forte, puis l'enveloppe attack/release fait le reste, avec les memes constantes de temps.
    const int slice = juce::jmax (16, juce::roundToInt (sampleRate * 0.001));
    const float msCoeff = (float) std::exp (-1.0 / (0.0053 * sampleRate));
    float maxReductionDb = 0.0f;

    for (int start = 0; start < numSmp; start += slice)
    {
        const int len = juce::jmin (slice, numSmp - start);
        auto sub = block.getSubBlock ((size_t) start, (size_t) len);

        for (int ch = 0; ch < numCh; ++ch)
        {
            detectorBuffer.copyFrom (ch, 0, sub.getChannelPointer ((size_t) ch), len);
            companionBuffer.copyFrom (ch, 0, sub.getChannelPointer ((size_t) ch), len);
        }
        auto mainBlock = juce::dsp::AudioBlock<float> (detectorBuffer).getSubBlock (0, (size_t) len);
        auto compBlock = juce::dsp::AudioBlock<float> (companionBuffer).getSubBlock (0, (size_t) len);
        detectorMain.process (juce::dsp::ProcessContextReplacing<float> (mainBlock));
        detectorCompanion.process (juce::dsp::ProcessContextReplacing<float> (compBlock));

        for (int i = 0; i < len; ++i)
        {
            float m = 0.0f, c = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float a = detectorBuffer.getSample (ch, i), b = companionBuffer.getSample (ch, i);
                m += a * a; c += b * b;
            }
            const float inv = 1.0f / (float) juce::jmax (1, numCh);
            mainMeanSquare      = m * inv + msCoeff * (mainMeanSquare - m * inv);
            companionMeanSquare = c * inv + msCoeff * (companionMeanSquare - c * inv);
        }

        const float levelDb = juce::Decibels::gainToDecibels (std::sqrt (juce::jmax (mainMeanSquare, companionMeanSquare)), -100.0f);
        const double timeConstant = (levelDb > smoothedLevelDb) ? (double) attackMs * 0.001 : (double) releaseMs * 0.001;
        const float coeff = (float) std::exp (-((double) len / sampleRate) / juce::jmax (0.0001, timeConstant));
        smoothedLevelDb = levelDb + coeff * (smoothedLevelDb - levelDb);

        const float reductionDb = juce::jlimit (0.0f, rangeDb, smoothedLevelDb - thresh);
        maxReductionDb = juce::jmax (maxReductionDb, reductionDb);

        // 3) Bandes cloche dynamiques appliquees au signal reel.
        *bellMain.state = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (
            sampleRate, safeFreq, dynamicBellQ, juce::Decibels::decibelsToGain (-reductionDb));
        *bellCompanion.state = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (
            sampleRate, companionFreq, companionQ, juce::Decibels::decibelsToGain (-reductionDb * companionDepth));

        juce::dsp::ProcessContextReplacing<float> context (sub);
        bellMain.process (context);
        bellCompanion.process (context);
    }

    detectorLevelDbMeter.store (smoothedLevelDb);
    gainReductionDbMeter.store (maxReductionDb);

    if (listen)
    {
        wetListenBuffer.setSize (numCh, numSmp, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
            wetListenBuffer.copyFrom (ch, 0, block.getChannelPointer ((size_t) ch), numSmp);
    }

    // mixWetSamples() toujours appele (meme en LISTEN) pour garder la FIFO
    // interne du DryWetMixer equilibree -- son resultat est simplement
    // ignore/ecrase ci-dessous quand LISTEN est actif.
    mixer.mixWetSamples (block);

    if (listen)
    {
        // Isole exactement ce que le de-esser retire (style Waves "Listen") :
        // difference entre le signal avant traitement et le signal issu des
        // bandes cloche dynamiques (avant mix dry/wet), sans tenir compte du
        // reglage Mix -- on veut entendre la reduction elle-meme, pas un
        // melange dry/wet.
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* dst      = block.getChannelPointer ((size_t) ch);
            const auto* dry = dryListenBuffer.getReadPointer (ch);
            const auto* wet = wetListenBuffer.getReadPointer (ch);

            for (int s = 0; s < numSmp; ++s)
                dst[s] = dry[s] - wet[s];
        }
    }

    sanitizeBlock (block);
}

} // namespace onirique
