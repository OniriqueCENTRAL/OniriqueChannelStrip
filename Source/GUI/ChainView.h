#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ModuleOrderManager.h"
#include <array>
#include <functional>

namespace onirique::ui
{

/** La chaîne du son en constellation, en haut de la façade : une étoile par module, dans l'ordre réel de
    traitement, reliées de IN à OUT, avec le signal qui la parcourt. Une étoile allumée = module actif ; elle
    s'entoure de braise quand le module réduit le niveau. Cliquer une étoile allume ou coupe le module, la glisser
    change l'ordre de traitement (paramètres <id>_order, voir ModuleOrderManager). */
class ChainView : public juce::Component,
                  private juce::Timer
{
public:
    ChainView (juce::AudioProcessorValueTreeState& apvts, ModuleOrderManager& orderManager,
               std::function<float (const juce::String& moduleID)> reductionDb);
    ~ChainView() override;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float slotX (int slot) const noexcept;   // 0 = IN, 1..8 = modules, 9 = OUT
    float lineY() const noexcept { return (float) getHeight() * 0.42f; }
    float scale() const noexcept { return (float) getHeight() / 90.0f; }
    int nodeAt (juce::Point<float>) const noexcept;
    int dropSlot() const noexcept;           // 1..8, pendant un glissement
    std::array<int, 8> slots() const;        // la place (1..8) de chaque module, glissement compris

    struct Node
    {
        juce::String id, name;
        std::atomic<float>* bypass = nullptr;
        float x = -1.0f, lit = -1.0f, reduction = 0.0f;
    };

    juce::AudioProcessorValueTreeState& state;
    ModuleOrderManager& order;
    std::function<float (const juce::String&)> reductionOf;
    std::array<Node, 8> nodes;
    int hover = -1, pressed = -1, dragging = -1;
    float dragX = 0.0f;
    double clock = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainView)
};

} // namespace onirique::ui
