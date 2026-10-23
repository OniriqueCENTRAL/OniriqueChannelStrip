#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "License/LicenseOverlay.h"
#include "PluginProcessor.h"
#include "GUI/OniriqueLookAndFeel.h"
#include "GUI/ModuleOrderManager.h"
#include "GUI/ModulePanel.h"
#include "GUI/DualDeEsserPanel.h"
#include "GUI/MeterBar.h"

//==============================================================================
/** Interface complete de ONIRIQUE CHANNEL STRIP, dans la DA ONIRIQUE (la meme
    que ONIRIQUE VOCALBOX : nuit / sang, accent braise, cartes en verre
    sombre, lueurs, en-tete vivant). Disposition ABSOLUE, aux coordonnees de
    la maquette 1000x640, mise a l'echelle au redimensionnement en conservant
    le ratio 1000:640 (ComponentBoundsConstrainer). Aucun changement DSP. */
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
    void paintHeader (juce::Graphics&);
    void paintStar (juce::Graphics&, juce::Point<float> c, float r, float alpha);
    void renderBackground();
    juce::String chainText() const;

    OniriqueChannelStripProcessor& proc;
    bool sizeRestored = false;   // les redimensionnements de la construction ne sont pas gardés

    onirique::ui::OniriqueLookAndFeel lookAndFeel;
    onirique::ui::ModuleOrderManager orderManager;

    std::unique_ptr<onirique::ui::MeterBar> inMeter, outMeter;

    std::unique_ptr<onirique::ui::ModulePanel> sslPanel;
    std::unique_ptr<onirique::ui::DualDeEsserPanel> deEsserPanel;
    std::unique_ptr<onirique::ui::ModulePanel> saturationPanel;
    std::unique_ptr<onirique::ui::ModulePanel> fairchildPanel;
    std::unique_ptr<onirique::ui::ModulePanel> f76Panel;
    std::unique_ptr<onirique::ui::ModulePanel> noiseGatePanel;
    std::unique_ptr<onirique::ui::ModulePanel> eqPanel;

    onirique::ui::MeterBar* sslGrMeter = nullptr;
    onirique::ui::MeterBar* fairchildGrMeter = nullptr;
    onirique::ui::MeterBar* f76GrMeter = nullptr;
    onirique::ui::MeterBar* noiseGateGrMeter = nullptr;

    static constexpr float refCanvasW = 1000.0f;
    static constexpr float refCanvasH = 640.0f;

    // ---- en-tete vivant (sigil qui respire, etoiles, impulsion sur la ligne de signal)
    juce::Image background, logo, logoScaled, logoGlow;
    juce::Rectangle<float> logoRect, signalRect, chainRect;
    std::vector<std::pair<juce::Point<float>, float>> stars;   // position, taille
    juce::String shownChain;
    double clock = 0.0;
    int frame = 0;

    juce::ComponentBoundsConstrainer constrainer;

    std::unique_ptr<onirique::license::Overlay> licenseOverlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OniriqueChannelStripEditor)
};
