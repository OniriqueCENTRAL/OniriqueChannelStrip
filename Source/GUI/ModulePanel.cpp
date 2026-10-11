#include "ModulePanel.h"

namespace onirique::ui
{

namespace
{
    // En-tete de carte (unites maquette, locales au panneau) : titre a gauche,
    // bouton power puis badge d'ordre a droite.
    constexpr float headerH = 29.0f;
    constexpr float powerCx = 62.0f;   // depuis le bord droit du panneau (centre)
    constexpr float powerCy = 15.0f;
    constexpr float powerD = 22.0f;
    constexpr float badgeOffsetX = 44.0f;   // depuis le bord droit
    constexpr float badgeY = 6.0f;
    constexpr float badgeW = 34.0f, badgeH = 18.0f;
}

ModulePanel::ModulePanel (juce::AudioProcessorValueTreeState& apvts, ModuleOrderManager& orderManager,
                          const juce::String& moduleID, juce::String displayName, const ModulePalette& palette,
                          float refWidth, float refHeight, const std::vector<RowSpec>& rows)
    : modulePalette (palette), refW (refWidth), refH (refHeight),
      state (apvts), bypassParamID (moduleID + "_bypass"), titleText (std::move (displayName))
{
    bypassLed = std::make_unique<PowerSwitch>();
    addAndMakeVisible (*bypassLed);

    if (auto* param = apvts.getParameter (bypassParamID))
        bypassAttachment = std::make_unique<juce::ButtonParameterAttachment> (*param, *bypassLed);
    bypassRaw = apvts.getRawParameterValue (bypassParamID);

    orderBadge = std::make_unique<OrderBadge> (orderManager, moduleID, palette.accentBright);
    addAndMakeVisible (*orderBadge);

    for (auto& row : rows)
    {
        std::unique_ptr<juce::Component> comp;

        switch (row.type)
        {
            case RowSpec::Type::knob:
                comp = std::make_unique<ParamKnob> (apvts, moduleID + "_" + row.paramSuffix, row.label,
                                                    palette.accent, palette.accentBright, row.showMid);
                break;
            case RowSpec::Type::toggle:
                comp = std::make_unique<ParamToggle> (apvts, moduleID + "_" + row.paramSuffix, row.label,
                                                      palette.accent, palette.accentBright);
                break;
            case RowSpec::Type::segmented:
                comp = std::make_unique<ParamSegmented> (apvts, moduleID + "_" + row.paramSuffix,
                                                          palette.accent, palette.accentBright);
                break;
            case RowSpec::Type::combo:
                comp = std::make_unique<ParamCombo> (apvts, moduleID + "_" + row.paramSuffix, palette.accent);
                break;
            case RowSpec::Type::section:
                comp = std::make_unique<SectionLabel> (row.label, palette.accent, row.showMid);
                break;
        }

        addAndMakeVisible (*comp);
        rowComponents.push_back ({ std::move (comp), row });
    }

    timerCallback();   // etat initial correct (sans attendre le premier tick)
    startTimerHz (30);
}

ModulePanel::~ModulePanel() { stopTimer(); }

void ModulePanel::paint (juce::Graphics& g)
{
    const float s = scaleFactor();
    const bool on = bypassRaw == nullptr || bypassRaw->load() < 0.5f;
    paintCard (g, getLocalBounds().toFloat(), titleText, on, s, (refW - 104.0f) * s);
}

// module coupe : voile nuit sur le contenu de la carte (titre, power et badge restent nets)
void ModulePanel::paintOverChildren (juce::Graphics& g)
{
    if (veil < 0.002f) return;
    const float s = scaleFactor();
    g.setColour (deep.withAlpha (0.72f * veil));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f).withTrimmedTop (headerH * s), 9.0f * s);
}

void ModulePanel::resized()
{
    const float s = scaleFactor();

    auto place = [s] (juce::Component& c, float x, float y, float w, float hh)
    {
        c.setBounds (juce::roundToInt (x * s), juce::roundToInt (y * s),
                    juce::roundToInt (w * s), juce::roundToInt (hh * s));
    };

    const float w = refW;

    place (*bypassLed, w - powerCx - powerD * 0.5f, powerCy - powerD * 0.5f, powerD, powerD);
    place (*orderBadge, w - badgeOffsetX, badgeY, badgeW, badgeH);

    for (auto& rc : rowComponents)
    {
        place (*rc.comp, rc.spec.x, rc.spec.y, rc.spec.w, rc.spec.h);
        if (auto* k = dynamic_cast<ParamKnob*> (rc.comp.get()))
            k->setUiScale (s);
    }

    for (auto& ec : extras)
        place (*ec.comp, ec.x, ec.y, ec.w, ec.h);
}

void ModulePanel::timerCallback()
{
    const bool bypassed = bypassRaw != nullptr && bypassRaw->load() > 0.5f;
    const float target = bypassed ? 1.0f : 0.0f;

    if (veil < 0.0f)
    {
        veil = target;
        repaint();
    }
    else if (approach (veil, target, 0.25f))
    {
        repaint();
    }
}

} // namespace onirique::ui
