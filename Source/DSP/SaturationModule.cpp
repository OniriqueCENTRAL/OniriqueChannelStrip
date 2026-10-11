#include "SaturationModule.h"
#include "DSPHelpers.h"
#include "../Parameters.h"
#include <cmath>

namespace onirique
{

namespace
{
    constexpr float gainRampSeconds = 0.015f;
    constexpr float maxDensityBonusDb = 4.0f;   // sur-saturation ajoutee aux niveaux les plus forts
    constexpr float envelopeCoeff = 0.9990f;    // lissage du suivi de niveau (RMS par bloc, puis lissage inter-blocs)
}

SaturationModule::SaturationModule (juce::AudioProcessorValueTreeState& state, const juce::String& moduleID)
    : ProcessorModule (state, moduleID)
{
    driveParam = apvts.getRawParameterValue (id + "_drive");
    typeParam  = apvts.getRawParameterValue (id + "_type");
}

SaturationModule::~SaturationModule() = default;

void SaturationModule::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                                      const juce::String& id)
{
    addCommonParameters (layout, id, "Saturation", numModules, 4);

    // IN/OUT : trim de niveau pur, independant du Drive (qui pilote la
    // quantite de saturation/densite, pas seulement le niveau) -- demande
    // explicite pour gerer le gain d'entree/sortie comme sur les autres
    // modules (SSL, Fairchild, 1176).
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_input", 1), "Input",
        juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, dbAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_drive", 1), "Drive",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f, dbAttrib()));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID (id + "_type", 1), "Type",
        juce::StringArray { "Tape", "Tube", "Transformer" }, 2));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_tone", 1), "Tone",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_mix", 1), "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pctAttrib()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id + "_output", 1), "Output",
        juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, dbAttrib()));

    // OFF par defaut : le sur-echantillonnage a un cout CPU et une latence
    // reels, on ne l'active jamais tout seul.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID (id + "_oversampling", 1), "Oversampling",
        juce::StringArray { "OFF", "2X", "4X", "8X" }, 0));
}

float SaturationModule::previewShape (float x, int type) noexcept
{
    switch (type)
    {
        case 0: // Tape : saturation symetrique douce
            return std::tanh (x);

        case 1: // Tube : asymetrique, ajoute des harmoniques paires (chaleur)
            return x >= 0.0f ? std::tanh (x)
                             : 0.8f * std::tanh (1.4f * x);

        default: // Transformer : soft-clip cubique + legere asymetrie
        {
            const float c = juce::jlimit (-1.5f, 1.5f, x + 0.02f * x * x);
            return c - (c * c * c) / 3.0f;
        }
    }
}

float SaturationModule::shape (float x) const noexcept
{
    const int type = typeParam != nullptr ? (int) typeParam->load() : 2;
    return previewShape (x, type);
}

void SaturationModule::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    preparedNumChannels = (int) spec.numChannels;
    preparedBlockSize   = (int) spec.maximumBlockSize;

    inputGain.prepare (spec);
    preGain.prepare (spec);
    outputGain.prepare (spec);
    inputGain.setRampDurationSeconds (gainRampSeconds);
    preGain.setRampDurationSeconds   (gainRampSeconds);
    outputGain.setRampDurationSeconds (gainRampSeconds);

    waveShaper.prepare (spec);
    toneFilter.prepare (spec);
    dcBlocker.prepare (spec);
    mixer.prepare (spec);

    dcBlockerCache = FilterParamCache();   // force le recalcul ci-dessous
    toneCache = FilterParamCache();

    // Chauffe le cache de parametres (voir ProcessorModule::getParam), hors
    // du thread audio : aucune allocation/recherche par nom dans process().
    warmUpParam ("input");
    warmUpParam ("tone");
    warmUpParam ("mix");
    warmUpParam ("output");
    warmUpParam ("oversampling");

    // (Re)construit l'oversampler pour le reglage courant. prepare() tourne
    // toujours hors du thread audio (appele par l'hote avant tout
    // processBlock) : l'allocation ici est sans risque.
    oversamplingIndex = -1;   // force setOversamplingIndex a (re)construire meme si la valeur n'a pas change
    setOversamplingIndex ((int) getParam ("oversampling"));

    reset();
}

void SaturationModule::reset()
{
    inputGain.reset();
    preGain.reset();
    outputGain.reset();
    toneFilter.reset();
    dcBlocker.reset();
    mixer.reset();
    envelopeLevel = 0.0f;
    toneCache = FilterParamCache();
}

double SaturationModule::effectiveSampleRate() const noexcept
{
    return oversampler != nullptr ? sampleRate * (double) (1 << oversamplingIndex) : sampleRate;
}

int SaturationModule::getLatencySamples() const noexcept
{
    return oversampler != nullptr ? (int) std::ceil (oversampler->getLatencyInSamples()) : 0;
}

void SaturationModule::setOversamplingIndex (int newIndex)
{
    newIndex = juce::jlimit (0, 3, newIndex);
    if (newIndex == oversamplingIndex)
        return;   // deja dans cet etat : rien a refaire (pas d'allocation inutile)

    oversamplingIndex = newIndex;

    if (newIndex <= 0)
    {
        oversampler.reset();   // OFF : aucun cout, le chemin normal (sans oversampler) est utilise
    }
    else
    {
        // Filtre IIR demi-bande : latence et cout CPU nettement plus bas
        // qu'un FIR equiripple, pour un filtrage anti-repliement tres
        // largement suffisant sur une saturation (pas de phase lineaire
        // necessaire ici). isMaximumQuality=true utilise une cascade IIR
        // d'ordre plus eleve pour une meilleure attenuation.
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) juce::jmax (1, preparedNumChannels), (size_t) newIndex,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true, false);
        oversampler->initProcessing ((size_t) juce::jmax (1, preparedBlockSize));
        oversampler->reset();
    }

    // Le sample rate "vu" par dcBlocker/toneFilter change avec le facteur de
    // sur-echantillonnage : force leur recalcul au prochain bloc.
    dcBlockerCache = FilterParamCache();
    toneCache = FilterParamCache();

    // Le chemin "wet" a maintenant un retard (latence de l'oversampler) :
    // sans compensation, le melange dry/wet du DryWetMixer created des
    // annulations de phase. setWetLatency retarde le chemin sec en
    // consequence pour que les deux restent alignes en phase.
    mixer.setWetLatency ((float) getLatencySamples());
}

void SaturationModule::processCore (juce::dsp::AudioBlock<float>& blk)
{
    // --- Trim d'entree (pur gain, avant tout traitement) -------------------
    juce::dsp::ProcessContextReplacing<float> inputContext (blk);
    inputGain.setGainDecibels (getParam ("input"));
    inputGain.process (inputContext);

    // --- Suivi de niveau d'entree (coloration progressive avec le niveau) ---
    const float currentBlockRms = blockRmsLevel (blk);
    envelopeLevel = currentBlockRms + envelopeCoeff * (envelopeLevel - currentBlockRms);

    // RMS ~0.3 (-10 dBFS) consideree comme "chaude" : au-dela, on ajoute
    // progressivement de la densite harmonique, jusqu'a +4 dB equivalents.
    const float densityAmount = juce::jlimit (0.0f, 1.0f, (envelopeLevel - 0.1f) / 0.6f);
    const float driveDb = (driveParam != nullptr ? driveParam->load() : 0.0f)
                         + densityAmount * maxDensityBonusDb;

    preGain.setGainDecibels (driveDb);

    juce::dsp::ProcessContextReplacing<float> context (blk);
    preGain.process (context);
    waveShaper.process (context);

    const double effRate = effectiveSampleRate();

    if (dcBlockerCache.update (5.0f, 0.0f, 0.707f))
        *dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (effRate, 5.0);
    dcBlocker.process (context);   // retire le decalage continu introduit par les formes asymetriques

    const float tone = getParam ("tone");
    if (toneCache.update (4000.0f, tone, 0.707f))
        *toneFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            effRate, 4000.0, 0.707f, juce::Decibels::decibelsToGain (tone * 6.0f));
    toneFilter.process (context);

    // Compensation automatique partielle du drive (densite naturelle sans
    // explosion de niveau), puis gain de sortie manuel.
    outputGain.setGainDecibels (getParam ("output") - driveDb * 0.25f);
    outputGain.process (context);
}

void SaturationModule::process (juce::dsp::AudioBlock<float>& block)
{
    // Changement de reglage d'oversampling : rare et deliberement choisi par
    // l'utilisateur (pas un parametre automatisable a frequence audio), donc
    // reconstruit ici au besoin plutot que via une machinerie inter-thread
    // dediee -- voir le commentaire au-dessus de setOversamplingIndex() et
    // le rapport final pour le compromis assume.
    setOversamplingIndex ((int) getParam ("oversampling"));

    mixer.setWetMixProportion (getParam ("mix") / 100.0f);
    mixer.pushDrySamples (block);

    if (oversampler != nullptr)
    {
        auto upBlock = oversampler->processSamplesUp (block);
        processCore (upBlock);
        oversampler->processSamplesDown (block);
    }
    else
    {
        processCore (block);
    }

    mixer.mixWetSamples (block);
    sanitizeBlock (block);
}

} // namespace onirique
