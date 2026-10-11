#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace onirique::ui
{

//==============================================================================
/** Ecran de la Saturation : courbe de transfert (entree/sortie), grille,
    diagonale pointillee de reference, courbe accent vif avec lueur. Se
    redessine quand drive/type/tone changent (poll leger a 10 Hz, pas
    d'attachment standard pour un simple redessin). Utilise
    SaturationModule::previewShape() -- la MEME fonction que le DSP reel --
    pour que l'ecran reflete exactement ce qu'entend l'auditeur. */
class TransferCurveDisplay : public juce::Component,
                             private juce::Timer
{
public:
    TransferCurveDisplay (juce::AudioProcessorValueTreeState& apvts, const juce::String& moduleID,
                          juce::Colour accentColour, juce::Colour accentBrightColour);
    ~TransferCurveDisplay() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* typeParam  = nullptr;
    std::atomic<float>* toneParam  = nullptr;

    float lastDrive = -1.0f, lastType = -1.0f, lastTone = -999.0f;
    juce::Colour accent, accentBright;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransferCurveDisplay)
};

} // namespace onirique::ui
