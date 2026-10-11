#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include "License/LicenseGuard.h"
#include "Parameters.h"
#include "Presets.h"
#include "DSP/ModuleChain.h"

//==============================================================================
/** ONIRIQUE CHANNEL STRIP : channel strip vocal complet (8 modules), ordre de
    traitement audio entierement reconfigurable independamment de la
    disposition graphique (voir DSP/ModuleChain.h). */
class OniriqueChannelStripProcessor : public juce::AudioProcessor,
                               private juce::AudioProcessorValueTreeState::Listener,
                               private juce::AsyncUpdater
{
public:
    OniriqueChannelStripProcessor();
    ~OniriqueChannelStripProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ONIRIQUE CHANNEL STRIP"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // --- Presets (programs) : exposes au DAW via le mecanisme standard
    //     AudioProcessor, donc visibles dans le selecteur de presets natif
    //     de n'importe quel hote, sans code d'interface supplementaire. Voir
    //     Source/Presets.cpp pour la table, extensible par simple ajout. ---
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}   // presets d'usine : non renommables

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    /** Acces a la chaine de modules pour l'interface graphique (ex:
        recuperer l'analyseur de spectre de l'EQ via getModule(...)). Le
        moteur audio reste seul a appeler prepare()/process() dessus. */
    onirique::ModuleChain& getModuleChain() noexcept { return moduleChain; }

    /** Crete d'entree/sortie du plugin (dB), mesuree une fois par bloc dans
        processBlock() -- alimente les meters IN/OUT de la barre du haut. */
    float getInputPeakDb() const noexcept { return inputPeakDb.load(); }
    float getOutputPeakDb() const noexcept { return outputPeakDb.load(); }

    /** Licence ONIRIQUE (achete, essai, demo) : voir License/LicenseGuard.h. */
    onirique::license::Guard license { "based" };

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void applyPreset (int index);
    void resetAllParametersToDefault();

    onirique::ModuleChain moduleChain;
    int currentProgramIndex = 0;

    std::atomic<float> inputPeakDb { -100.0f };
    std::atomic<float> outputPeakDb { -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OniriqueChannelStripProcessor)
};
