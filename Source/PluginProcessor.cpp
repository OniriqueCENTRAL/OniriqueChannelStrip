#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "DSP/NoiseGateModule.h"
#include "DSP/SSLChannelModule.h"
#include "DSP/DeEsserModule.h"
#include "DSP/SaturationModule.h"
#include "DSP/FETCompressorModule.h"
#include "DSP/VariMuCompressorModule.h"
#include "DSP/ParametricEQModule.h"

using namespace juce;
using namespace onirique;

namespace
{
    // Instancie les 8 modules et les ajoute a la chaine. Point unique a
    // modifier pour ajouter/retirer un module du channel strip.
    void buildModuleChain (ModuleChain& chain, AudioProcessorValueTreeState& apvts)
    {
        chain.addModule (std::make_unique<NoiseGateModule>        (apvts, ModuleIDs::noiseGate));
        chain.addModule (std::make_unique<SSLChannelModule>       (apvts, ModuleIDs::sslChannel));
        chain.addModule (std::make_unique<DeEsserModule>          (apvts, ModuleIDs::deEsser1, "De-Esser 1", 3));
        chain.addModule (std::make_unique<DeEsserModule>          (apvts, ModuleIDs::deEsser2, "De-Esser 2", 8));
        chain.addModule (std::make_unique<SaturationModule>       (apvts, ModuleIDs::saturation));
        chain.addModule (std::make_unique<FETCompressorModule>    (apvts, ModuleIDs::fet1176));
        chain.addModule (std::make_unique<VariMuCompressorModule> (apvts, ModuleIDs::fairchild670));
        chain.addModule (std::make_unique<ParametricEQModule>     (apvts, ModuleIDs::eq));
    }

    // Seul parametre dont un changement modifie la latence du plugin (voir
    // SaturationModule) : c'est le seul que l'on ecoute pour declencher une
    // mise a jour de la latence declaree a l'hote.
    const juce::String& latencyAffectingParamID()
    {
        static const juce::String idStr = ModuleIDs::saturation + "_oversampling";
        return idStr;
    }
}

//==============================================================================
OniriqueChannelStripProcessor::OniriqueChannelStripProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    buildModuleChain (moduleChain, apvts);

    // La reconstruction de l'oversampler (voir SaturationModule) peut
    // changer la latence du plugin. On ne doit JAMAIS appeler
    // setLatencySamples() directement depuis le thread audio (cela peut
    // declencher une reaction synchrone, potentiellement couteuse, de
    // l'hote) : le listener se contente de demander une mise a jour
    // asynchrone, executee sur le thread message par handleAsyncUpdate().
    apvts.addParameterListener (latencyAffectingParamID(), this);
}

OniriqueChannelStripProcessor::~OniriqueChannelStripProcessor()
{
    apvts.removeParameterListener (latencyAffectingParamID(), this);
}

void OniriqueChannelStripProcessor::parameterChanged (const juce::String&, float)
{
    triggerAsyncUpdate();
}

void OniriqueChannelStripProcessor::handleAsyncUpdate()
{
    setLatencySamples (moduleChain.getTotalLatencySamples());
}

void OniriqueChannelStripProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Sample rate et taille de bloc sécurisés : un hote qui annoncerait une
    // valeur degenerée ne doit jamais produire de division par zero ou de
    // NaN plus loin dans la chaine (voir aussi DSPHelpers::safeFilterFreq).
    const double safeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const int safeBlockSize     = juce::jmax (1, samplesPerBlock);

    dsp::ProcessSpec spec;
    spec.sampleRate       = safeSampleRate;
    spec.maximumBlockSize = (uint32) safeBlockSize;
    spec.numChannels      = (uint32) juce::jmax (1, getTotalNumOutputChannels());

    moduleChain.prepare (spec);

    // Safe ici (prepareToPlay n'est jamais appele depuis le thread audio) :
    // contrairement a parameterChanged(), pas besoin de passer par
    // l'AsyncUpdater.
    setLatencySamples (moduleChain.getTotalLatencySamples());
}

void OniriqueChannelStripProcessor::releaseResources()
{
}

bool OniriqueChannelStripProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void OniriqueChannelStripProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto peakOf = [&buffer]
    {
        float peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            peak = jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
        return Decibels::gainToDecibels (peak, -100.0f);
    };

    inputPeakDb.store (peakOf());

    dsp::AudioBlock<float> block (buffer);
    moduleChain.process (block);

    outputPeakDb.store (peakOf());
}

//==============================================================================
AudioProcessorEditor* OniriqueChannelStripProcessor::createEditor()
{
    return new OniriqueChannelStripEditor (*this);
}

//==============================================================================
//  Presets (programs)
//==============================================================================
int OniriqueChannelStripProcessor::getNumPrograms()
{
    return (int) onirique::getFactoryPresets().size();
}

int OniriqueChannelStripProcessor::getCurrentProgram()
{
    return currentProgramIndex;
}

const juce::String OniriqueChannelStripProcessor::getProgramName (int index)
{
    const auto& presets = onirique::getFactoryPresets();
    if (index >= 0 && index < (int) presets.size())
        return presets[(size_t) index].name;

    return {};
}

void OniriqueChannelStripProcessor::resetAllParametersToDefault()
{
    // getDefaultValue() est deja normalise (0..1), utilisable tel quel.
    for (auto* param : getParameters())
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getDefaultValue());
        param->endChangeGesture();
    }
}

void OniriqueChannelStripProcessor::applyPreset (int index)
{
    const auto& presets = onirique::getFactoryPresets();
    if (index < 0 || index >= (int) presets.size())
    {
        jassertfalse;
        return;
    }

    // Un preset ne liste que ce qui le caracterise ; tout le reste doit
    // revenir a sa valeur par defaut pour un rappel reproductible.
    resetAllParametersToDefault();

    for (auto& kv : presets[(size_t) index].values)
    {
        if (auto* param = apvts.getParameter (kv.first))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (kv.second));
            param->endChangeGesture();
        }
        else
        {
            jassertfalse;   // identifiant de parametre invalide dans Presets.cpp : a corriger
        }
    }

    currentProgramIndex = index;
}

void OniriqueChannelStripProcessor::setCurrentProgram (int index)
{
    // Les hotes appellent setCurrentProgram() depuis le thread message, pas
    // le thread audio (ce n'est pas une operation temps-reel) : le cout de
    // resetAllParametersToDefault() + application du preset est sans risque
    // ici.
    applyPreset (index);
}

//==============================================================================
void OniriqueChannelStripProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        state.setProperty ("oniriqueCurrentProgram", currentProgramIndex, nullptr);

        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
    }
}

void OniriqueChannelStripProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto newState = ValueTree::fromXml (*xml);
            currentProgramIndex = newState.getProperty ("oniriqueCurrentProgram", 0);
            apvts.replaceState (newState);

            // replaceState() notifie l'hote des nouvelles valeurs de
            // parametres, mais la latence (dependante de l'oversampling
            // restaure) doit etre recalculee explicitement.
            triggerAsyncUpdate();
        }
    }
}

//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OniriqueChannelStripProcessor();
}
