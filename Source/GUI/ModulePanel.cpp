#include "ModulePanel.h"

namespace onirique::ui
{

ModulePanel::ModulePanel (juce::AudioProcessorValueTreeState& apvts, const juce::String& moduleID, juce::String displayName,
                          float refWidth, float refHeight, const std::vector<RowSpec>& rows)
    : refW (refWidth), refH (refHeight), titleText (std::move (displayName))
{
    bypassLed = std::make_unique<PowerSwitch>();
    addAndMakeVisible (*bypassLed);
    if (auto* param = apvts.getParameter (moduleID + "_bypass"))
        bypassAttachment = std::make_unique<juce::ButtonParameterAttachment> (*param, *bypassLed);
    bypassRaw = apvts.getRawParameterValue (moduleID + "_bypass");

    for (auto& row : rows)
    {
        auto knob = std::make_unique<ParamKnob> (apvts, moduleID + "_" + row.paramSuffix, row.label);
        addAndMakeVisible (*knob);
        knobs.push_back ({ std::move (knob), row });
    }

    timerCallback();   // état initial correct, sans attendre le premier tick
    startTimerHz (30);
}

ModulePanel::~ModulePanel() { stopTimer(); }

void ModulePanel::paint (juce::Graphics& g)
{
    const float s = scaleFactor();
    const bool on = bypassRaw == nullptr || bypassRaw->load() < 0.5f;
    paintSectionTitle (g, { 22.0f * s, 0.0f, (float) getWidth() - 22.0f * s, headH * s }, titleText, on, s);
}

void ModulePanel::resized()
{
    const float s = scaleFactor();
    auto place = [s] (juce::Component& c, float x, float y, float w, float h)
    {
        c.setBounds (juce::roundToInt (x * s), juce::roundToInt (y * s), juce::roundToInt (w * s), juce::roundToInt (h * s));
    };

    place (*bypassLed, -2.0f, -2.0f, 22.0f, 22.0f);
    for (auto& k : knobs)
    {
        place (*k.knob, k.spec.cx - k.spec.width() * 0.5f, k.spec.top, k.spec.width(), k.spec.height());
        k.knob->setUiScale (s, k.spec.dial * s);
    }
    for (auto& e : extras)
        place (*e.comp, e.x, e.y, e.w, e.h);
}

void ModulePanel::timerCallback()
{
    const float target = bypassRaw != nullptr && bypassRaw->load() > 0.5f ? 1.0f : 0.0f;
    const bool first = veil < 0.0f;
    if (first) veil = target;
    if (first || approach (veil, target, 0.25f))
    {
        // module coupé : ses réglages s'estompent (pas de voile rectangulaire, qui redessinerait une carte)
        for (auto* child : getChildren())
            if (child != bypassLed.get())
                child->setAlpha (1.0f - 0.62f * veil);
        repaint();
    }
}

} // namespace onirique::ui
