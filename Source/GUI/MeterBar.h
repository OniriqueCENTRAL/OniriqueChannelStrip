#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>

namespace onirique::ui
{

//==============================================================================
/** Meter segmente horizontal ou vertical, utilise pour toutes les mesures du
    plugin (GR, niveau/threshold, IN/OUT) : un seul composant parametrable
    plutot qu'une classe par cas, les differences etant purement une affaire
    de plage affichee et de cote de remplissage.

    Convention : `dispMin` correspond toujours a l'extremite "proche" de la
    barre (haut pour un meter vertical, gauche pour un horizontal), `dispMax`
    a l'extremite "eloignee". `fillFromNearEnd` choisit le cote fixe depuis
    lequel le segment lumineux grandit :
      - true  (GR) : le remplissage part de l'extremite proche (0 dB, pas de
        reduction) et grandit vers la valeur courante -- "se remplit depuis
        le haut/la gauche".
      - false (niveau/threshold, IN/OUT) : le remplissage part de
        l'extremite eloignee (silence) jusqu'a la valeur courante -- "se
        remplit depuis le bas".

    Ballistique : montee instantanee, descente lissee (~300 ms), peak-hold
    (~500 ms). Rafraichissement 30 Hz, repaint seulement si la valeur
    affichee change de plus de 0.1 dB (cout CPU minimal a l'arret). */
class MeterBar : public juce::Component,
                 private juce::Timer
{
public:
    enum class Orientation { horizontal, vertical };

    MeterBar (Orientation orientation, float dispMinDb, float dispMaxDb, bool fillFromNearEnd,
              std::vector<std::pair<float, juce::String>> ticks, juce::Colour fillColour);
    ~MeterBar() override;

    /** Valeur courante a afficher (dB). A appeler depuis le Timer de
        l'editeur, lui-meme alimente par les getters atomiques du DSP. */
    void setValueDb (float newDb) noexcept;

    /** Active un trait blanc deplaçable (De-Esser : threshold sur le meter
        de niveau). Le callback recoit la nouvelle valeur en dB a chaque
        glissement ; appelant doit pousser cette valeur au parametre reel. */
    void enableThresholdMarker (std::function<void (float)> callback);
    void setThresholdDb (float db) noexcept;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    float dbToFraction (float db) const noexcept;
    float fractionToDb (float frac) const noexcept;
    float positionToFraction (juce::Point<float>) const noexcept;

    Orientation orientationKind;
    float dispMin, dispMax;
    bool fillFromNear;
    std::vector<std::pair<float, juce::String>> tickList;
    juce::Colour fillColourValue;

    float currentDb = -100.0f, targetDb = -100.0f, lastPaintedDb = -1000.0f;
    float peakDb = -100.0f;
    int peakHoldCounter = 0;

    bool hasThreshold = false;
    float thresholdDbValue = -20.0f;
    std::function<void (float)> onThresholdChanged;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterBar)
};

} // namespace onirique::ui
