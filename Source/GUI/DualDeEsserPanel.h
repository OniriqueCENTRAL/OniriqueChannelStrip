#pragma once

#include "ParamControls.h"
#include "OrderBadge.h"
#include "ModuleOrderManager.h"
#include "MeterBar.h"
#include "UIStyle.h"
#include "../DSP/DeEsserModule.h"
#include <array>

namespace onirique::ui
{

//==============================================================================
/** Panneau DE-ESSER fusionne : deux instances independantes de DeEsserModule
    (deesser1 / deesser2) dans UN SEUL panneau graphique, chacune avec son
    propre bouton power, son propre badge d'ordre (3 et 8 par defaut), ses
    5 potards, son meter de niveau/threshold (trait blanc deplaçable) et son
    meter de reduction de gain. Pas une sous-classe de ModulePanel (qui ne
    gere qu'un seul badge/power par panneau) : composant autonome, meme
    carte ONIRIQUE (voir paintCard) redessinee directement. */
class DualDeEsserPanel : public juce::Component,
                         private juce::Timer
{
public:
    DualDeEsserPanel (juce::AudioProcessorValueTreeState& apvts, ModuleOrderManager& orderManager,
                      const juce::String& moduleID1, const juce::String& moduleID2,
                      onirique::DeEsserModule& deEsser1Module, onirique::DeEsserModule& deEsser2Module,
                      const ModulePalette& palette, float refWidth, float refHeight);
    ~DualDeEsserPanel() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Row
    {
        juce::String moduleID;
        juce::String bypassParamID;
        onirique::DeEsserModule* dsp = nullptr;
        std::atomic<float>* bypassRaw = nullptr;
        std::atomic<float>* thresholdRaw = nullptr;
        bool bypassedCached = false;
        float veil = -1.0f;   // 0 = instance active, 1 = coupee (voile anime)

        std::unique_ptr<PowerSwitch> led;
        std::unique_ptr<juce::ButtonParameterAttachment> ledAttachment;
        std::unique_ptr<OrderBadge> badge;
        std::vector<std::unique_ptr<ParamKnob>> knobs;
        std::unique_ptr<MeterBar> levelMeter;
        std::unique_ptr<MeterBar> grMeter;
        std::unique_ptr<ParamToggle> listen;
        juce::String tagText;
    };

    void buildRow (Row& row, juce::AudioProcessorValueTreeState& apvts, ModuleOrderManager& orderManager,
                  const juce::String& moduleID, onirique::DeEsserModule& dsp);

    ModulePalette modulePalette;
    float refW, refH;
    std::array<Row, 2> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DualDeEsserPanel)
};

} // namespace onirique::ui
