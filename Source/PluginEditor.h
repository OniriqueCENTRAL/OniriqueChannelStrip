#pragma once
#include "ui/OniriqueBar.h"   // la barre « Nuit » de VOCALBOX

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "License/LicenseOverlay.h"
#include "PluginProcessor.h"
#include "GUI/OniriqueLookAndFeel.h"
#include "GUI/ModuleOrderManager.h"
#include "GUI/ModulePanel.h"
#include "GUI/ChainView.h"
#include "GUI/MeterBar.h"

//==============================================================================
/** Interface de ONIRIQUE CHANNEL STRIP, de la même famille que VERB et TUNER : la barre « Nuit » en haut, la chaîne
    du son en constellation, puis les huit modules à plat sur la façade, séparés par des filets. Disposition aux
    coordonnées d'une façade de 1000 de large, mise à l'échelle en gardant ses proportions. */
class OniriqueChannelStripEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit OniriqueChannelStripEditor (OniriqueChannelStripProcessor&);
    ~OniriqueChannelStripEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    float scale() const noexcept { return (float) getWidth() / refCanvasW; }
    void renderBackground();
    float reductionOf (const juce::String& moduleID) const;

    OniriqueChannelStripProcessor& proc;
    bool sizeRestored = false;   // les redimensionnements de la construction ne sont pas gardés

    // la barre « Nuit » (presets, A/B, copier/coller, menu ⋯ avec la couleur de l'interface), au-dessus de la façade
    static juce::Image barSigil();
    onirique::nuit::Bar bar { proc.presets, proc.apvts, "CHANNEL STRIP", barSigil(), 1000, 1400 };

    onirique::ui::OniriqueLookAndFeel lookAndFeel;
    onirique::ui::ModuleOrderManager orderManager;

    std::unique_ptr<onirique::ui::ChainView> chain;
    std::unique_ptr<onirique::ui::MeterBar> inMeter, outMeter;

    // un module : son panneau et sa place sur la façade
    struct Placed { std::unique_ptr<onirique::ui::ModulePanel> panel; float x, y, w, h; };
    std::vector<Placed> panels;
    onirique::ui::ModulePanel& addPanel (const juce::String& id, const juce::String& title, float x, float y, float w, float h,
                                         std::vector<onirique::ui::RowSpec> knobs);

    // les meters des modules, nourris par le minuteur de l'éditeur : module, meter, réduction (sinon niveau détecté)
    struct Meter { juce::String id; onirique::ui::MeterBar* bar; bool reduction; };
    std::vector<Meter> meters;

    static constexpr float refCanvasW = 1000.0f;
    static constexpr float kTop = 50.0f;                // la barre « Nuit » au-dessus de la façade
    static constexpr float refCanvasH = 710.0f + kTop;

    juce::Image background;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<onirique::license::Overlay> licenseOverlay;
    int themeSeen = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OniriqueChannelStripEditor)
};
