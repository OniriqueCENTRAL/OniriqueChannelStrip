#include "DualDeEsserPanel.h"

namespace onirique::ui
{

namespace
{
    struct KnobPos { const char* suffix; const char* label; float cx, cy; };

    // x des 5 potards resserre (72/108/.../180/216, pas de 36 au lieu de 44
    // a l'origine) pour degager une colonne LISTEN avant les meters, sans
    // toucher au reste de la mise en page (badge/meters inchanges).
    const KnobPos knobLayout[5] = {
        { "frequency", "FREQ",    72.0f,  0.0f },
        { "range",     "RANGE",  108.0f,  0.0f },
        { "attack",    "ATTACK", 144.0f,  0.0f },
        { "release",   "RELEASE",180.0f,  0.0f },
        { "mix",       "MIX",    216.0f,  0.0f },
    };

    constexpr float knobScale = 0.78f;   // cadrans plus compacts : 5 potards sur un pas de 36
    constexpr float row1KnobY = 64.0f, row2KnobY = 146.0f;
    constexpr float row1LedY = 52.0f,  row2LedY = 134.0f;
    constexpr float row1BadgeY = 66.0f, row2BadgeY = 148.0f;
    constexpr float row1MeterY = 40.0f, row2MeterY = 122.0f;
    constexpr float ledCx = 25.0f;
    constexpr float powerD = 20.0f;
    constexpr float meterLen = 56.0f;
    constexpr float levelMeterX = 302.0f, grMeterX = 350.0f;
    constexpr float separatorY = 114.0f;
    constexpr float headerH = 29.0f;

    // Bouton LISTEN (style Waves) : isole ce que le de-esser retire, dans
    // l'espace degage entre le dernier potard (MIX) et le meter de niveau.
    constexpr float listenX = 240.0f, listenW = 50.0f, listenH = 18.0f;
}

DualDeEsserPanel::DualDeEsserPanel (juce::AudioProcessorValueTreeState& apvts, ModuleOrderManager& orderManager,
                                    const juce::String& moduleID1, const juce::String& moduleID2,
                                    onirique::DeEsserModule& deEsser1Module, onirique::DeEsserModule& deEsser2Module,
                                    const ModulePalette& palette, float refWidth, float refHeight)
    : modulePalette (palette), refW (refWidth), refH (refHeight)
{
    buildRow (rows[0], apvts, orderManager, moduleID1, deEsser1Module);
    rows[0].tagText = "DE-ESS 1";

    buildRow (rows[1], apvts, orderManager, moduleID2, deEsser2Module);
    rows[1].tagText = "DE-ESS 2";

    timerCallback();
    startTimerHz (30);
}

DualDeEsserPanel::~DualDeEsserPanel() { stopTimer(); }

void DualDeEsserPanel::buildRow (Row& row, juce::AudioProcessorValueTreeState& apvts, ModuleOrderManager& orderManager,
                                 const juce::String& moduleID, onirique::DeEsserModule& dsp)
{
    row.moduleID = moduleID;
    row.bypassParamID = moduleID + "_bypass";
    row.dsp = &dsp;

    row.bypassRaw = apvts.getRawParameterValue (row.bypassParamID);
    row.thresholdRaw = apvts.getRawParameterValue (moduleID + "_threshold");

    row.led = std::make_unique<PowerSwitch>();
    addAndMakeVisible (*row.led);
    if (auto* param = apvts.getParameter (row.bypassParamID))
        row.ledAttachment = std::make_unique<juce::ButtonParameterAttachment> (*param, *row.led);

    row.badge = std::make_unique<OrderBadge> (orderManager, moduleID, modulePalette.accentBright);
    addAndMakeVisible (*row.badge);

    for (auto& k : knobLayout)
    {
        auto knob = std::make_unique<ParamKnob> (apvts, moduleID + "_" + k.suffix, k.label,
                                                 modulePalette.accent, modulePalette.accentBright);
        addAndMakeVisible (*knob);
        row.knobs.push_back (std::move (knob));
    }

    row.levelMeter = std::make_unique<MeterBar> (MeterBar::Orientation::vertical, 0.0f, -40.0f, false,
                                                 std::vector<std::pair<float, juce::String>> { { 0.0f, "0" }, { -20.0f, "-20" }, { -40.0f, "-40" } },
                                                 modulePalette.accent);
    row.levelMeter->enableThresholdMarker ([&apvts, moduleID] (float newDb)
    {
        if (auto* param = apvts.getParameter (moduleID + "_threshold"))
        {
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (ranged->convertTo0to1 (newDb));
                param->endChangeGesture();
            }
        }
    });
    addAndMakeVisible (*row.levelMeter);

    row.grMeter = std::make_unique<MeterBar> (MeterBar::Orientation::vertical, 0.0f, -12.0f, true,
                                              std::vector<std::pair<float, juce::String>> { { 0.0f, "0" }, { -3.0f, "-3" }, { -7.0f, "-7" }, { -10.0f, "-10" } },
                                              modulePalette.accentBright);
    addAndMakeVisible (*row.grMeter);

    // LISTEN (style Waves) : isole et laisse entendre uniquement ce que le
    // de-esser retire -- lie a "<id>_listen" (voir DeEsserModule::process()).
    row.listen = std::make_unique<ParamToggle> (apvts, moduleID + "_listen", "LISTEN",
                                                modulePalette.accent, modulePalette.accentBright);
    addAndMakeVisible (*row.listen);
}

void DualDeEsserPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float s = w / juce::jmax (1.0f, refW);

    const bool anyOn = ! rows[0].bypassedCached || ! rows[1].bypassedCached;
    paintCard (g, bounds, "DE-ESSER", anyOn, s, (refW - 40.0f) * s);

    // Separateur fin entre les deux lignes.
    g.setColour (line);
    g.fillRect (juce::Rectangle<float> (12.0f * s, separatorY * s, w - 24.0f * s, 1.0f));

    // Tags "DE-ESS 1/2" + libelles TH/GR de chaque ligne.
    const float tagY[2] = { 90.0f, 172.0f };
    const float meterY[2] = { row1MeterY, row2MeterY };

    for (int r = 0; r < 2; ++r)
    {
        const auto& row = rows[(size_t) r];
        g.setColour (haze.interpolatedWith (mist, row.bypassedCached ? 0.0f : 0.5f));
        g.setFont (fitFont (Face::mono, 7.0f * s, 42.0f * s, row.tagText, true, 0.04f));
        g.drawText (row.tagText, juce::Rectangle<float> ((ledCx - 21.0f) * s, tagY[r] * s, 42.0f * s, 12.0f * s),
                    juce::Justification::centred, false);

        g.setColour (haze.withAlpha (0.85f));
        g.setFont (font (Face::mono, 6.5f * s, true));
        g.drawText ("TH", juce::Rectangle<float> ((levelMeterX - 14.0f) * s, (meterY[r] + meterLen + 4.0f) * s, 28.0f * s, 10.0f * s),
                    juce::Justification::centred, false);
        g.drawText ("GR", juce::Rectangle<float> ((grMeterX - 14.0f) * s, (meterY[r] + meterLen + 4.0f) * s, 28.0f * s, 10.0f * s),
                    juce::Justification::centred, false);
    }
}

// instance coupee : voile nuit sur sa ligne (power, badge et tag restent nets)
void DualDeEsserPanel::paintOverChildren (juce::Graphics& g)
{
    const float s = getWidth() / juce::jmax (1.0f, refW);
    const float top[2] = { headerH, separatorY + 1.0f };
    const float bottom[2] = { separatorY - 1.0f, refH - 3.0f };

    for (int r = 0; r < 2; ++r)
    {
        const float v = rows[(size_t) r].veil;
        if (v < 0.002f) continue;
        g.setColour (deep.withAlpha (0.72f * v));
        g.fillRoundedRectangle (juce::Rectangle<float> (50.0f * s, top[r] * s, (refW - 53.0f) * s, (bottom[r] - top[r]) * s), 8.0f * s);
    }
}

void DualDeEsserPanel::resized()
{
    const float s = getWidth() / juce::jmax (1.0f, refW);

    auto place = [s] (juce::Component& c, float x, float y, float w, float h)
    {
        c.setBounds (juce::roundToInt (x * s), juce::roundToInt (y * s), juce::roundToInt (w * s), juce::roundToInt (h * s));
    };

    const float ledY[2] = { row1LedY, row2LedY };
    const float badgeY[2] = { row1BadgeY, row2BadgeY };
    const float knobY[2] = { row1KnobY, row2KnobY };
    const float meterY[2] = { row1MeterY, row2MeterY };

    for (int r = 0; r < 2; ++r)
    {
        auto& row = rows[(size_t) r];

        place (*row.led, ledCx - powerD * 0.5f, ledY[r] - powerD * 0.5f, powerD, powerD);
        place (*row.badge, ledCx - 17.0f, badgeY[r], 34.0f, 18.0f);

        for (size_t i = 0; i < row.knobs.size(); ++i)
        {
            const float dialD = 23.0f * 2.0f * knobScale;
            const float w = dialD + 10.0f, h = dialD + 23.0f;
            place (*row.knobs[i], knobLayout[i].cx - w * 0.5f, knobY[r] - dialD * 0.5f, w, h);
            row.knobs[i]->setUiScale (s);
        }

        place (*row.levelMeter, levelMeterX - 7.0f, meterY[r], 14.0f, meterLen);
        place (*row.grMeter, grMeterX - 7.0f, meterY[r], 14.0f, meterLen);

        place (*row.listen, listenX, knobY[r] - listenH * 0.5f, listenW, listenH);
    }
}

void DualDeEsserPanel::timerCallback()
{
    bool needsRepaint = false;

    for (auto& row : rows)
    {
        if (row.dsp == nullptr)
            continue;

        row.levelMeter->setValueDb (row.dsp->getDetectorLevelDb());
        row.grMeter->setValueDb (-row.dsp->getGainReductionDb());

        if (row.thresholdRaw != nullptr)
            row.levelMeter->setThresholdDb (row.thresholdRaw->load());

        const bool bypassed = row.bypassRaw != nullptr && row.bypassRaw->load() > 0.5f;
        const float target = bypassed ? 1.0f : 0.0f;
        if (bypassed != row.bypassedCached) { row.bypassedCached = bypassed; needsRepaint = true; }
        if (row.veil < 0.0f) { row.veil = target; needsRepaint = true; }
        else if (approach (row.veil, target, 0.25f)) needsRepaint = true;
    }

    if (needsRepaint)
        repaint();
}

} // namespace onirique::ui
