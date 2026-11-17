#include "PluginEditor.h"
#include "GUI/OniriqueDA.h"   // la DA de VOCALBOX
#include "GUI/OniriqueWindowSize.h"
#include "GUI/ParamControls.h"
#include "GUI/TransferCurveDisplay.h"
#include "DSP/SSLChannelModule.h"
#include "DSP/FETCompressorModule.h"
#include "DSP/VariMuCompressorModule.h"
#include "DSP/DeEsserModule.h"
#include "DSP/NoiseGateModule.h"
#include "BinaryData.h"

using namespace onirique;
using namespace onirique::ui;

namespace
{
using Ticks = std::vector<std::pair<float, juce::String>>;

std::unique_ptr<MeterBar> grMeter (MeterBar::Orientation o, float floorDb, Ticks ticks)
{
    return std::make_unique<MeterBar> (o, 0.0f, floorDb, true, std::move (ticks), bone);
}

// les filets de la façade, entre les modules : { x1, y1, x2, y2 } en coordonnées de la façade
constexpr float grooves[][4] {
    { 16, 108, 984, 108 }, { 16, 390, 984, 390 }, { 16, 586, 984, 586 },
    { 528, 120, 528, 378 }, { 424, 402, 424, 574 }, { 824, 402, 824, 574 }, { 466, 598, 466, 704 },
};
} // namespace

ModulePanel& OniriqueChannelStripEditor::addPanel (const juce::String& id, const juce::String& title, float x, float y, float w, float h,
                                                   std::vector<RowSpec> knobs)
{
    panels.push_back ({ std::make_unique<ModulePanel> (proc.apvts, id, title, w, h, knobs), x, y, w, h });
    addAndMakeVisible (*panels.back().panel);
    return *panels.back().panel;
}

OniriqueChannelStripEditor::OniriqueChannelStripEditor (OniriqueChannelStripProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), orderManager (p.apvts)
{
    onirique::look::load();   // la couleur de l'interface choisie, avant le premier dessin
    setLookAndFeel (&lookAndFeel);
    auto& apvts = proc.apvts;
    using R = RowSpec;
    using O = MeterBar::Orientation;

    // ---- la chaîne du son, entre les meters IN et OUT
    chain = std::make_unique<ChainView> (apvts, orderManager, [this] (const juce::String& id) { return reductionOf (id); });
    addAndMakeVisible (*chain);
    inMeter = std::make_unique<MeterBar> (O::vertical, 0.0f, -48.0f, false, Ticks {}, ember);
    outMeter = std::make_unique<MeterBar> (O::vertical, 0.0f, -48.0f, false, Ticks {}, ember);
    addAndMakeVisible (*inMeter);
    addAndMakeVisible (*outMeter);

    // ---- SSL CHANNEL : l'EQ en colonnes (une par bande), le compresseur à droite
    auto& ssl = addPanel (ModuleIDs::sslChannel, "SSL CHANNEL", 16, 118, 500, 262, {
        R::knob ("input", "INPUT", 30, 54, 34),          R::knob ("hpf", "HPF", 30, 122, 34),             R::knob ("output", "OUTPUT", 30, 190, 34),
        R::knob ("lowFreq", "FREQ", 94, 54, 34),         R::knob ("lowGain", "GAIN", 94, 122, 34),
        R::knob ("lowMidFreq", "FREQ", 150, 54, 34),     R::knob ("lowMidGain", "GAIN", 150, 122, 34),    R::knob ("lowMidQ", "Q", 150, 190, 34),
        R::knob ("highMidFreq", "FREQ", 206, 54, 34),    R::knob ("highMidGain", "GAIN", 206, 122, 34),   R::knob ("highMidQ", "Q", 206, 190, 34),
        R::knob ("highFreq", "FREQ", 262, 54, 34),       R::knob ("highGain", "GAIN", 262, 122, 34),
        R::knob ("threshold", "THRESH", 344, 54, 34),    R::knob ("ratio", "RATIO", 400, 54, 34),         R::knob ("attack", "ATTACK", 456, 54, 34),
        R::knob ("release", "RELEASE", 344, 122, 34),    R::knob ("makeup", "MAKEUP", 400, 122, 34),      R::knob ("gateThreshold", "GATE", 456, 122, 34),
    });
    ssl.addExtra (std::make_unique<SectionLabel> ("EQ", ember, true), 66, 24, 228, 14);
    for (auto [band, cx] : { std::pair { "LF", 94.0f }, { "LMF", 150.0f }, { "HMF", 206.0f }, { "HF", 262.0f } })
        ssl.addExtra (std::make_unique<SectionLabel> (band, ember), cx - 28, 38, 56, 12);
    ssl.addExtra (std::make_unique<SectionLabel> ("COMPRESSEUR", ember, true), 318, 24, 182, 14);
    meters.push_back ({ ModuleIDs::sslChannel, &ssl.addExtra (grMeter (O::horizontal, -12.0f, { { 0.0f, "0" }, { -3.0f, "-3" }, { -6.0f, "-6" }, { -10.0f, "-10" } }), 322, 214, 166, 9), true });

    // ---- SATURATION : la courbe de transfert et son type à gauche, DRIVE au milieu des réglages
    auto& sat = addPanel (ModuleIDs::saturation, "SATURATION", 540, 118, 444, 262, {
        R::knob ("input", "IN", 246, 44, 44),  R::knob ("drive", "DRIVE", 330, 26, 72),  R::knob ("tone", "TONE", 414, 44, 44),
        R::knob ("mix", "MIX", 280, 152, 44),  R::knob ("output", "OUT", 380, 152, 44),
    });
    sat.addExtra (std::make_unique<TransferCurveDisplay> (apvts, ModuleIDs::saturation, ember, bone), 0, 28, 190, 190);
    sat.addExtra (std::make_unique<ParamSegmented> (apvts, ModuleIDs::saturation + "_type", ember, bone), 0, 230, 190, 26);

    // ---- les deux DE-ESSER l'un sous l'autre : le seuil se règle sur le meter de niveau (trait blanc)
    for (int k = 0; k < 2; ++k)
    {
        const auto& id = k == 0 ? ModuleIDs::deEsser1 : ModuleIDs::deEsser2;
        auto& de = addPanel (id, k == 0 ? "DE-ESS 1" : "DE-ESS 2", 16, k == 0 ? 400.0f : 492.0f, 396, 84, {
            R::knob ("frequency", "FREQ", 28, 20, 30), R::knob ("range", "RANGE", 84, 20, 30), R::knob ("attack", "ATTACK", 140, 20, 30),
            R::knob ("release", "RELEASE", 196, 20, 30), R::knob ("mix", "MIX", 252, 20, 30),
        });
        de.addExtra (std::make_unique<ParamToggle> (apvts, id + "_listen", "LISTEN", ember, bone), 286, 40, 64, 20);
        auto& level = de.addExtra (std::make_unique<MeterBar> (O::vertical, 0.0f, -40.0f, false, Ticks {}, ember), 360, 22, 8, 58);
        level.enableThresholdMarker ([&apvts, id] (float db)
        {
            if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id + "_threshold")))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (param->convertTo0to1 (db));
                param->endChangeGesture();
            }
        });
        meters.push_back ({ id, &level, false });
        meters.push_back ({ id, &de.addExtra (grMeter (O::vertical, -12.0f, { { 0.0f, "0" }, { -6.0f, "-6" }, { -12.0f, "-12" } }), 374, 22, 8, 58), true });
    }

    // ---- 1176 FET : le ratio et AUTO en haut, les six potards dessous
    auto& fet = addPanel (ModuleIDs::fet1176, "1176 FET", 436, 400, 376, 176, {
        R::knob ("input", "INPUT", 28, 70, 36), R::knob ("threshold", "THRESH", 84, 70, 36), R::knob ("attack", "ATTACK", 140, 70, 36),
        R::knob ("release", "RELEASE", 196, 70, 36), R::knob ("output", "OUTPUT", 252, 70, 36), R::knob ("mix", "MIX", 308, 70, 36),
    });
    fet.addExtra (std::make_unique<ParamSegmented> (apvts, ModuleIDs::fet1176 + "_ratio", ember, bone), 0, 28, 210, 24);
    fet.addExtra (std::make_unique<ParamToggle> (apvts, ModuleIDs::fet1176 + "_autoGain", "AUTO GAIN", ember, bone), 220, 28, 104, 24);
    meters.push_back ({ ModuleIDs::fet1176, &fet.addExtra (grMeter (O::vertical, -12.0f, { { 0.0f, "0" }, { -3.0f, "-3" }, { -6.0f, "-6" }, { -12.0f, "-12" } }), 342, 26, 8, 140), true });

    // ---- NOISE GATE : un grand THRESHOLD, ATTACK et RELEASE dessous
    auto& gate = addPanel (ModuleIDs::noiseGate, "NOISE GATE", 836, 400, 148, 176, {
        R::knob ("threshold", "THRESHOLD", 52, 22, 52), R::knob ("attack", "ATTACK", 26, 108, 30), R::knob ("release", "RELEASE", 82, 108, 30),
    });
    meters.push_back ({ ModuleIDs::noiseGate, &gate.addExtra (grMeter (O::vertical, -40.0f, { { 0.0f, "0" }, { -20.0f, "-20" }, { -40.0f, "-40" } }), 116, 26, 8, 144), true });

    // ---- FAIRCHILD 670 : les sept potards en ligne et son meter
    auto& fair = addPanel (ModuleIDs::fairchild670, "FAIRCHILD 670", 16, 596, 440, 108, {
        R::knob ("input", "INPUT", 28, 26, 34), R::knob ("threshold", "THRESH", 84, 26, 34), R::knob ("attack", "ATTACK", 140, 26, 34),
        R::knob ("release", "RELEASE", 196, 26, 34), R::knob ("ratio", "RATIO", 252, 26, 34), R::knob ("output", "OUTPUT", 308, 26, 34),
        R::knob ("mix", "MIX", 364, 26, 34),
    });
    meters.push_back ({ ModuleIDs::fairchild670, &fair.addExtra (grMeter (O::vertical, -12.0f, { { 0.0f, "0" }, { -6.0f, "-6" }, { -12.0f, "-12" } }), 404, 26, 8, 74), true });

    // ---- EQ : huit bandes à fréquence fixe, un potard par bande
    addPanel (ModuleIDs::eq, "EQ", 476, 596, 508, 108, {
        R::knob ("band1_gain", "80 CUT", 30, 26, 36),  R::knob ("band2_gain", "150 LOW", 94, 26, 36), R::knob ("band3_gain", "400", 158, 26, 36),
        R::knob ("band4_gain", "900", 222, 26, 36),    R::knob ("band5_gain", "2.5k", 286, 26, 36),   R::knob ("band6_gain", "5k", 350, 26, 36),
        R::knob ("band7_gain", "9k HIGH", 414, 26, 36), R::knob ("band8_gain", "14k AIR", 478, 26, 36),
    });

    addAndMakeVisible (bar);
    bar.onThemeChanged = [this] { themeSeen = -1; };

    licenseOverlay = std::make_unique<onirique::license::Overlay> ("ONIRIQUE CHANNEL STRIP", proc.license);
    addAndMakeVisible (*licenseOverlay);

    setOpaque (true);
    constrainer.setFixedAspectRatio (refCanvasW / refCanvasH);
    constrainer.setSizeLimits (600, 456, 2400, 1824);
    setConstrainer (&constrainer);
    setResizable (true, true);
    // taille d'ouverture : la règle commune des plugins ONIRIQUE (GUI/OniriqueWindowSize.h)
    const int w = onirique::window::openingSize (proc.apvts.state, "ONIRIQUE CHANNEL STRIP", { 1300, 988 }, { 600, 456 }, { 2400, 1824 }).x;
    setSize (w, juce::roundToInt ((float) w * refCanvasH / refCanvasW));
    sizeRestored = true;

    startTimerHz (30);
}

OniriqueChannelStripEditor::~OniriqueChannelStripEditor()
{
    stopTimer();
    onirique::window::rememberOnThisComputer ("ONIRIQUE CHANNEL STRIP", getWidth(), getHeight());
    setLookAndFeel (nullptr);
}

//==============================================================================
float OniriqueChannelStripEditor::reductionOf (const juce::String& id) const
{
    auto* m = proc.getModuleChain().getModule (id);
    if (auto* x = dynamic_cast<SSLChannelModule*> (m))       return x->getGainReductionDb();
    if (auto* x = dynamic_cast<FETCompressorModule*> (m))    return x->getGainReductionDb();
    if (auto* x = dynamic_cast<VariMuCompressorModule*> (m)) return x->getGainReductionDb();
    if (auto* x = dynamic_cast<DeEsserModule*> (m))          return x->getGainReductionDb();
    if (auto* x = dynamic_cast<NoiseGateModule*> (m))        return x->getGainReductionDb();
    return 0.0f;
}

void OniriqueChannelStripEditor::timerCallback()
{
    // la couleur de l'interface (menu ⋯, ou changée dans un autre plugin ONIRIQUE) : tout se redessine
    if (onirique::look::changedSince (themeSeen))
    {
        if (auto* lf = dynamic_cast<OniriqueLookAndFeel*> (&getLookAndFeel())) { lf->applyColours(); sendLookAndFeelChange(); }
        background = {};
        std::function<void (juce::Component&)> all = [&all] (juce::Component& k) { k.repaint(); for (auto* ch : k.getChildren()) all (*ch); };
        all (*this);
    }

    inMeter->setValueDb (proc.getInputPeakDb());
    outMeter->setValueDb (proc.getOutputPeakDb());
    for (auto& m : meters)
    {
        if (m.reduction)
            m.bar->setValueDb (-reductionOf (m.id));
        else if (auto* de = dynamic_cast<DeEsserModule*> (proc.getModuleChain().getModule (m.id)))
        {
            m.bar->setValueDb (de->getDetectorLevelDb());
            if (auto* t = proc.apvts.getRawParameterValue (m.id + "_threshold"))
                m.bar->setThresholdDb (t->load());
        }
    }
}

//==============================================================================
void OniriqueChannelStripEditor::resized()
{
    if (sizeRestored) onirique::window::remember (proc.apvts.state, getWidth(), getHeight());   // taille gardée avec le projet
    const float s = scale();
    auto place = [s] (juce::Component& c, float x, float y, float w, float h)
    {
        c.setBounds (juce::roundToInt (x * s), juce::roundToInt ((y + kTop) * s), juce::roundToInt (w * s), juce::roundToInt (h * s));
    };

    place (*inMeter, 18, 18, 7, 70);
    place (*outMeter, 975, 18, 7, 70);
    place (*chain, 34, 10, 932, 90);
    for (auto& p : panels)
        place (*p.panel, p.x, p.y, p.w, p.h);

    // la barre « Nuit » en haut, la pastille de licence à sa place dans la barre
    bar.setPillWidth (licenseOverlay != nullptr ? licenseOverlay->pillWidth() : 0);
    bar.place (s, getLocalBounds());
    if (licenseOverlay != nullptr)
        licenseOverlay->fit (getLocalBounds(), s, bar.getPillArea(), juce::Justification::centredRight);
}

juce::Image OniriqueChannelStripEditor::barSigil()
{
    return juce::ImageCache::getFromMemory (BinaryData::sigil_png, BinaryData::sigil_pngSize);
}

// le fond fixe (nuit de VOCALBOX, filets entre les modules, noms des meters) : rendu une fois par taille
void OniriqueChannelStripEditor::renderBackground()
{
    const float s = scale();
    background = juce::Image (juce::Image::RGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), false);
    juce::Graphics g (background);
    g.drawImageAt (onirique::da::backdrop (getWidth(), getHeight(), (float) getWidth() / 1100.0f), 0, 0);

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    for (auto& l : grooves)
        g.drawLine (l[0] * s, (l[1] + kTop) * s, l[2] * s, (l[3] + kTop) * s, 1.0f);

    g.setColour (haze);
    g.setFont (font (Face::body, 8.5f * s, true, 0.14f));
    g.drawText ("IN", juce::Rectangle<float> (2 * s, (92 + kTop) * s, 40 * s, 12 * s), juce::Justification::centred, false);
    g.drawText ("OUT", juce::Rectangle<float> (958 * s, (92 + kTop) * s, 40 * s, 12 * s), juce::Justification::centred, false);
}

void OniriqueChannelStripEditor::paint (juce::Graphics& g)
{
    if (background.getWidth() != getWidth() || background.getHeight() != getHeight())
        renderBackground();
    g.drawImageAt (background, 0, 0);
}
