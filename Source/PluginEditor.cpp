#include "PluginEditor.h"
#include "GUI/OniriqueWindowSize.h"
#include "GUI/ParamControls.h"
#include "GUI/TransferCurveDisplay.h"
#include "DSP/SSLChannelModule.h"
#include "DSP/FETCompressorModule.h"
#include "DSP/VariMuCompressorModule.h"
#include "DSP/DeEsserModule.h"
#include "DSP/NoiseGateModule.h"
#include "BinaryData.h"
#include <algorithm>

using namespace onirique;
using namespace onirique::ui;

namespace
{
    // Raccourci local : construit un ModulePanel a partir d'une liste de
    // RowSpec deja en coordonnees LOCALES au panneau (voir ui-spec.json,
    // deja soustrait de x/y du panneau -- calcule une fois pour toutes dans
    // ce fichier, jamais a la main ailleurs).
    std::unique_ptr<ModulePanel> makePanel (juce::AudioProcessorValueTreeState& apvts, ModuleOrderManager& orderMgr,
                                            const juce::String& id, const juce::String& title, ModuleKind kind,
                                            float w, float h, std::vector<RowSpec> rows)
    {
        return std::make_unique<ModulePanel> (apvts, orderMgr, id, title, paletteFor (kind), w, h, std::move (rows));
    }
}

OniriqueChannelStripEditor::OniriqueChannelStripEditor (OniriqueChannelStripProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), orderManager (p.apvts)
{
    setLookAndFeel (&lookAndFeel);

    auto& apvts = proc.apvts;

    // --- SSL CHANNEL (210x436) : 19 potards reels (2 emplacements "Q" de la
    //     maquette n'ont pas de parametre reel pour les filtres en plateau
    //     LF/HF -- regle 5 du brief : on ne les affiche pas plutot que
    //     d'inventer un parametre). --------------------------------------
    sslPanel = makePanel (apvts, orderManager, ModuleIDs::sslChannel, "SSL CHANNEL", ModuleKind::ssl, 210, 436, {
        RowSpec::knob ("input", "INPUT", 58, 50, 0.8f),
        RowSpec::knob ("hpf", "HPF", 116, 50, 0.8f),
        RowSpec::knob ("output", "OUTPUT", 174, 50, 0.8f),

        // "EQ"/"COMPRESSOR" : titres de groupe places dans la colonne de
        // droite des rangees LF et HF (seules rangees EQ a n'avoir que 2
        // potards, FREQ/GAIN -- pas de Q reel, voir plus haut -- donc la
        // 3e colonne y est libre : aucun risque de chevaucher un potard).
        RowSpec::section ("EQ", 150, 96, 54, 16, true),
        RowSpec::section ("LF", 12, 94, 40, 16),
        RowSpec::knob ("lowFreq", "FREQ", 58, 102, 0.8f),
        RowSpec::knob ("lowGain", "GAIN", 116, 102, 0.8f),

        RowSpec::section ("LMF", 12, 146, 40, 16),
        RowSpec::knob ("lowMidFreq", "FREQ", 58, 154, 0.8f),
        RowSpec::knob ("lowMidGain", "GAIN", 116, 154, 0.8f),
        RowSpec::knob ("lowMidQ", "Q", 174, 154, 0.8f),

        RowSpec::section ("HMF", 12, 198, 40, 16),
        RowSpec::knob ("highMidFreq", "FREQ", 58, 206, 0.8f),
        RowSpec::knob ("highMidGain", "GAIN", 116, 206, 0.8f),
        RowSpec::knob ("highMidQ", "Q", 174, 206, 0.8f),

        RowSpec::section ("HF", 12, 250, 40, 16),
        RowSpec::knob ("highFreq", "FREQ", 58, 258, 0.8f),
        RowSpec::knob ("highGain", "GAIN", 116, 258, 0.8f),

        // Meme principe que "EQ" plus haut : colonne de droite libre de la
        // rangee HF (2 potards seulement), juste avant la section
        // compresseur/gate qui suit.
        RowSpec::section ("COMPRESSOR", 146, 252, 60, 16, true),
        RowSpec::knob ("threshold", "THRESH", 58, 310, 0.8f),
        RowSpec::knob ("ratio", "RATIO", 116, 310, 0.8f),
        RowSpec::knob ("attack", "ATTACK", 174, 310, 0.8f),

        RowSpec::knob ("release", "RELEASE", 58, 362, 0.8f),
        RowSpec::knob ("makeup", "MAKEUP", 116, 362, 0.8f),
        RowSpec::knob ("gateThreshold", "GATE", 174, 362, 0.8f),
    });

    if (auto* sslModule = dynamic_cast<SSLChannelModule*> (proc.getModuleChain().getModule (ModuleIDs::sslChannel)))
    {
        sslGrMeter = &sslPanel->addExtra (std::make_unique<MeterBar> (
            MeterBar::Orientation::horizontal, 0.0f, -12.0f, true,
            std::vector<std::pair<float, juce::String>> { { 0.0f, "0" }, { -3.0f, "-3" }, { -5.0f, "-5" }, { -7.0f, "-7" }, { -10.0f, "-10" } },
            paletteFor (ModuleKind::ssl).accentBright), 34.0f, 410.0f, 164.0f, 14.0f);
        juce::ignoreUnused (sslModule);
    }

    // --- DE-ESSER (fusionne, 380x200) ------------------------------------
    auto* deEsser1 = dynamic_cast<DeEsserModule*> (proc.getModuleChain().getModule (ModuleIDs::deEsser1));
    auto* deEsser2 = dynamic_cast<DeEsserModule*> (proc.getModuleChain().getModule (ModuleIDs::deEsser2));
    jassert (deEsser1 != nullptr && deEsser2 != nullptr);
    deEsserPanel = std::make_unique<DualDeEsserPanel> (apvts, orderManager, ModuleIDs::deEsser1, ModuleIDs::deEsser2,
                                                       *deEsser1, *deEsser2, paletteFor (ModuleKind::deEsser), 380, 200);

    // --- SATURATION (374x200) : potards IN/OUT ajoutes (grille 3 colonnes
    //     resserree a 0.8 pour leur faire de la place, INPUT/DRIVE/TONE en
    //     haut, MIX/OUTPUT en bas sous DRIVE/TONE). IN est un trim pur,
    //     independant du Drive (qui pilote la saturation elle-meme). -------
    saturationPanel = makePanel (apvts, orderManager, ModuleIDs::saturation, "SATURATION", ModuleKind::saturation, 374, 200, {
        RowSpec::knob ("input", "IN", 196, 116, 0.8f),
        RowSpec::knob ("drive", "DRIVE", 266, 116, 0.8f),
        RowSpec::knob ("tone", "TONE", 336, 116, 0.8f),
        RowSpec::knob ("mix", "MIX", 266, 170, 0.8f),
        RowSpec::knob ("output", "OUT", 336, 170, 0.8f),
    });
    saturationPanel->addExtra (std::make_unique<TransferCurveDisplay> (apvts, ModuleIDs::saturation,
                               paletteFor (ModuleKind::saturation).accent, paletteFor (ModuleKind::saturation).accentBright),
                               12.0f, 38.0f, 140.0f, 150.0f);
    saturationPanel->addExtra (std::make_unique<ParamSegmented> (apvts, ModuleIDs::saturation + "_type",
                               paletteFor (ModuleKind::saturation).accent, paletteFor (ModuleKind::saturation).accentBright),
                               168.0f, 42.0f, 192.0f, 24.0f);

    // --- FAIRCHILD 670 (380x160) ------------------------------------------
    fairchildPanel = makePanel (apvts, orderManager, ModuleIDs::fairchild670, "FAIRCHILD 670", ModuleKind::fairchild, 380, 160, {
        RowSpec::knob ("input", "INPUT", 26, 84, 0.9f, true),
        RowSpec::knob ("threshold", "THRESH", 72, 84, 0.9f, true),
        RowSpec::knob ("attack", "ATTACK", 118, 84, 0.9f, true),
        RowSpec::knob ("release", "RELEASE", 164, 84, 0.9f, true),
        RowSpec::knob ("ratio", "RATIO", 210, 84, 0.9f, true),
        RowSpec::knob ("output", "OUTPUT", 256, 84, 0.9f, true),
        RowSpec::knob ("mix", "MIX", 302, 84, 0.9f, true),
    });
    if (auto* fairchildModule = dynamic_cast<VariMuCompressorModule*> (proc.getModuleChain().getModule (ModuleIDs::fairchild670)))
    {
        fairchildGrMeter = &fairchildPanel->addExtra (std::make_unique<MeterBar> (
            MeterBar::Orientation::vertical, 0.0f, -12.0f, true,
            std::vector<std::pair<float, juce::String>> { { 0.0f, "0" }, { -1.0f, "-1" }, { -3.0f, "-3" }, { -5.0f, "-5" }, { -7.0f, "-7" }, { -10.0f, "-10" } },
            paletteFor (ModuleKind::fairchild).accentBright), 333.0f, 36.0f, 14.0f, 100.0f);
        juce::ignoreUnused (fairchildModule);
    }

    // --- 1176 FET (374x160) -------------------------------------------------
    f76Panel = makePanel (apvts, orderManager, ModuleIDs::fet1176, "1176 FET", ModuleKind::fet1176, 374, 160, {
        RowSpec::knob ("input", "INPUT", 30, 104, 0.9f, true),
        RowSpec::knob ("threshold", "THRESH", 80, 104, 0.9f, true),
        RowSpec::knob ("attack", "ATTACK", 130, 104, 0.9f, true),
        RowSpec::knob ("release", "RELEASE", 180, 104, 0.9f, true),
        RowSpec::knob ("output", "OUTPUT", 230, 104, 0.9f, true),
        RowSpec::knob ("mix", "MIX", 280, 104, 0.9f, true),
    });
    f76Panel->addExtra (std::make_unique<ParamSegmented> (apvts, ModuleIDs::fet1176 + "_ratio",
                        paletteFor (ModuleKind::fet1176).accent, paletteFor (ModuleKind::fet1176).accentBright),
                        16.0f, 36.0f, 208.0f, 20.0f);
    f76Panel->addExtra (std::make_unique<ParamToggle> (apvts, ModuleIDs::fet1176 + "_autoGain", "AUTO",
                        paletteFor (ModuleKind::fet1176).accent, paletteFor (ModuleKind::fet1176).accentBright),
                        222.0f, 38.0f, 90.0f, 16.0f);
    if (auto* f76Module = dynamic_cast<FETCompressorModule*> (proc.getModuleChain().getModule (ModuleIDs::fet1176)))
    {
        f76GrMeter = &f76Panel->addExtra (std::make_unique<MeterBar> (
            MeterBar::Orientation::vertical, 0.0f, -12.0f, true,
            std::vector<std::pair<float, juce::String>> { { 0.0f, "0" }, { -1.0f, "-1" }, { -3.0f, "-3" }, { -5.0f, "-5" }, { -7.0f, "-7" }, { -10.0f, "-10" } },
            paletteFor (ModuleKind::fet1176).accentBright), 334.0f, 36.0f, 14.0f, 100.0f);
        juce::ignoreUnused (f76Module);
    }

    // --- NOISE GATE (210x132) : style "NS1" -- un gros potard THRESHOLD,
    //     plus ATTACK/RELEASE en potards secondaires (demande explicite).
    //     hold/range/hysteresis gardent leurs valeurs par defaut et restent
    //     automatisables depuis l'hote, simplement pas affiches ici --
    //     aucun parametre retire cote DSP, seule la presentation change. --
    noiseGatePanel = makePanel (apvts, orderManager, ModuleIDs::noiseGate, "NOISE GATE", ModuleKind::noiseGate, 210, 132, {
        RowSpec::knob ("threshold", "THRESHOLD", 44, 74, 1.05f),
        RowSpec::knob ("attack", "ATTACK", 108, 74, 0.62f),
        RowSpec::knob ("release", "RELEASE", 152, 74, 0.62f),
    });
    if (auto* gateModule = dynamic_cast<NoiseGateModule*> (proc.getModuleChain().getModule (ModuleIDs::noiseGate)))
    {
        noiseGateGrMeter = &noiseGatePanel->addExtra (std::make_unique<MeterBar> (
            MeterBar::Orientation::vertical, 0.0f, -40.0f, true,
            std::vector<std::pair<float, juce::String>> { { 0.0f, "0" }, { -10.0f, "-10" }, { -20.0f, "-20" }, { -30.0f, "-30" }, { -40.0f, "-40" } },
            paletteFor (ModuleKind::noiseGate).accentBright), 186.0f, 40.0f, 14.0f, 82.0f);
        juce::ignoreUnused (gateModule);
    }

    // --- EQ (762x200) : style "MAAG EQ4" -- 8 potards boost/cut a frequence
    //     fixe (une bande = un potard), plutot qu'une courbe a glisser.
    //     Les parametres freq/Q/type de chaque bande restent inchanges cote
    //     DSP (donc automatisables depuis l'hote) : seul `gain` est expose
    //     ici, le reste garde ses valeurs par defaut -- rien n'est retire,
    //     seule la presentation change (regle du projet). ------------------
    eqPanel = makePanel (apvts, orderManager, ModuleIDs::eq, "EQ", ModuleKind::eq, 762, 200, {
        RowSpec::knob ("band1_gain", "80 CUT",   48, 108, 1.3f),
        RowSpec::knob ("band2_gain", "150 LOW",  143, 108, 1.3f),
        RowSpec::knob ("band3_gain", "400",      238, 108, 1.3f),
        RowSpec::knob ("band4_gain", "900",      333, 108, 1.3f),
        RowSpec::knob ("band5_gain", "2.5k",     428, 108, 1.3f),
        RowSpec::knob ("band6_gain", "5k",       524, 108, 1.3f),
        RowSpec::knob ("band7_gain", "9k HIGH",  619, 108, 1.3f),
        RowSpec::knob ("band8_gain", "14k AIR",  714, 108, 1.3f),
    });

    addAndMakeVisible (*sslPanel);
    addAndMakeVisible (*deEsserPanel);
    addAndMakeVisible (*saturationPanel);
    addAndMakeVisible (*fairchildPanel);
    addAndMakeVisible (*f76Panel);
    addAndMakeVisible (*noiseGatePanel);
    addAndMakeVisible (*eqPanel);

    // --- En-tete : meters IN / OUT (le sigil, le titre et la chaine sont
    //     dessines par l'editeur lui-meme, voir paintHeader()) --------------
    inMeter = std::make_unique<MeterBar> (MeterBar::Orientation::horizontal, -40.0f, 0.0f, true,
                                          std::vector<std::pair<float, juce::String>> {}, ember);
    outMeter = std::make_unique<MeterBar> (MeterBar::Orientation::horizontal, -40.0f, 0.0f, true,
                                           std::vector<std::pair<float, juce::String>> {}, ember);
    addAndMakeVisible (*inMeter);
    addAndMakeVisible (*outMeter);

    logo = juce::ImageCache::getFromMemory (BinaryData::sigil_png, BinaryData::sigil_pngSize);
    shownChain = chainText();

    licenseOverlay = std::make_unique<onirique::license::Overlay> ("ONIRIQUE CHANNEL STRIP", proc.license);
    addAndMakeVisible (*licenseOverlay);
   #if ONIRIQUE_UI_TEST
    // essais locaux uniquement (jamais dans les versions publiees) : captures sans la page d'activation
    if (juce::SystemStats::getEnvironmentVariable ("ONIRIQUE_UI_TEST", {}).contains ("nolicense"))
        licenseOverlay->setVisible (false);
   #endif

    setOpaque (true);
    constrainer.setFixedAspectRatio (refCanvasW / refCanvasH);
    constrainer.setSizeLimits (500, 320, 2400, 1536);
    setConstrainer (&constrainer);
    setResizable (true, true);
    // taille d'ouverture : la règle commune des plugins ONIRIQUE (GUI/OniriqueWindowSize.h)
    const int w = onirique::window::openingSize (proc.apvts.state, "ONIRIQUE CHANNEL STRIP", { 1400, 896 }, { 500, 320 }, { 2400, 1536 }).x;
    setSize (w, juce::roundToInt ((float) w * refCanvasH / refCanvasW));
    sizeRestored = true;

    startTimerHz (60);
}

OniriqueChannelStripEditor::~OniriqueChannelStripEditor()
{
    stopTimer();
    onirique::window::rememberOnThisComputer ("ONIRIQUE CHANNEL STRIP", getWidth(), getHeight());
    setLookAndFeel (nullptr);
}

//==============================================================================
juce::String OniriqueChannelStripEditor::chainText() const
{
    // ordre de traitement REEL (parametres <id>_order), mis a jour en direct
    static const std::pair<const juce::String*, const char*> modules[] {
        { &ModuleIDs::noiseGate, "GATE" },         { &ModuleIDs::sslChannel, "SSL" },
        { &ModuleIDs::deEsser1, "DE-ESS 1" },      { &ModuleIDs::deEsser2, "DE-ESS 2" },
        { &ModuleIDs::saturation, "SATURATION" },  { &ModuleIDs::fet1176, "1176" },
        { &ModuleIDs::fairchild670, "FAIRCHILD" }, { &ModuleIDs::eq, "EQ" },
    };

    std::array<std::pair<int, const char*>, 8> sorted;
    for (size_t i = 0; i < sorted.size(); ++i)
        sorted[i] = { orderManager.getOrder (*modules[i].first), modules[i].second };
    std::stable_sort (sorted.begin(), sorted.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });

    juce::String txt ("INPUT");
    for (const auto& m : sorted)
        txt << "  >  " << m.second;
    return txt << "  >  OUTPUT";
}

void OniriqueChannelStripEditor::timerCallback()
{
    clock += 1.0 / 60.0;
    ++frame;

    // meters a 30 images/s (ils lissent eux-memes leur affichage)
    if ((frame & 1) == 0)
    {
        inMeter->setValueDb (proc.getInputPeakDb());
        outMeter->setValueDb (proc.getOutputPeakDb());

        if (sslGrMeter != nullptr)
            if (auto* m = dynamic_cast<SSLChannelModule*> (proc.getModuleChain().getModule (ModuleIDs::sslChannel)))
                sslGrMeter->setValueDb (-m->getGainReductionDb());

        if (fairchildGrMeter != nullptr)
            if (auto* m = dynamic_cast<VariMuCompressorModule*> (proc.getModuleChain().getModule (ModuleIDs::fairchild670)))
                fairchildGrMeter->setValueDb (-m->getGainReductionDb());

        if (f76GrMeter != nullptr)
            if (auto* m = dynamic_cast<FETCompressorModule*> (proc.getModuleChain().getModule (ModuleIDs::fet1176)))
                f76GrMeter->setValueDb (-m->getGainReductionDb());

        if (noiseGateGrMeter != nullptr)
            if (auto* m = dynamic_cast<NoiseGateModule*> (proc.getModuleChain().getModule (ModuleIDs::noiseGate)))
                noiseGateGrMeter->setValueDb (-m->getGainReductionDb());
    }

    // chaine de traitement affichee sous le titre : suit les badges d'ordre
    if ((frame % 6) == 0)
    {
        const auto txt = chainText();
        if (txt != shownChain)
        {
            shownChain = txt;
            repaint (chainRect.getSmallestIntegerContainer());
        }
    }

    // en-tete vivant : lueur du sigil, signal qui parcourt la ligne, etoiles qui scintillent
    repaint (logoRect.expanded (logoRect.getHeight() * 0.4f).getSmallestIntegerContainer());
    repaint (signalRect.getSmallestIntegerContainer());
    if ((frame & 1) == 0)
        for (const auto& st : stars)
            repaint (juce::Rectangle<float> (st.second * 2.4f, st.second * 2.4f).withCentre (st.first).getSmallestIntegerContainer());
}

//==============================================================================
void OniriqueChannelStripEditor::resized()
{
    if (sizeRestored) onirique::window::remember (proc.apvts.state, getWidth(), getHeight());   // taille gardée avec le projet
    const float s = scale();
    const float W = (float) getWidth();

    auto place = [s] (juce::Component& c, float x, float y, float w, float h)
    {
        c.setBounds (juce::roundToInt (x * s), juce::roundToInt (y * s), juce::roundToInt (w * s), juce::roundToInt (h * s));
    };

    place (*inMeter, 800.0f, 9.0f, 180.0f, 12.0f);
    place (*outMeter, 800.0f, 27.0f, 180.0f, 12.0f);

    place (*sslPanel, 10.0f, 56.0f, 210.0f, 436.0f);
    place (*deEsserPanel, 228.0f, 56.0f, 380.0f, 200.0f);
    place (*saturationPanel, 616.0f, 56.0f, 374.0f, 200.0f);
    place (*fairchildPanel, 228.0f, 264.0f, 380.0f, 160.0f);
    place (*f76Panel, 616.0f, 264.0f, 374.0f, 160.0f);
    place (*noiseGatePanel, 10.0f, 500.0f, 210.0f, 132.0f);
    place (*eqPanel, 228.0f, 432.0f, 762.0f, 200.0f);
    // La pastille de licence : dans l'en-tête, contre les vumètres IN / OUT, centrée sur eux ; sa place suit son texte.
    const int badge = licenseOverlay->pillWidth();
    licenseOverlay->fit (getLocalBounds(), s, { 786 - badge, 11, badge, 26 });

    // ---- en-tete
    logoRect = { 12.0f * s, 6.0f * s, 38.0f * s * 985.0f / 895.0f, 38.0f * s };
    signalRect = { 0.0f, 46.0f * s, W, 8.0f * s };
    chainRect = { logoRect.getRight() + 9.0f * s, 30.0f * s, 520.0f * s, 12.0f * s };

    if (logo.isValid())
    {
        // sigil pre-reduit (net) + version floutee pour la lueur, recalcules seulement au redimensionnement
        const int lh = juce::jlimit (16, 600, juce::roundToInt (logoRect.getHeight() * 2.0f));
        logoScaled = logo.rescaled (juce::roundToInt ((float) lh * 985.0f / 895.0f), lh, juce::Graphics::highResamplingQuality);
        const int pad = lh / 4;
        juce::Image src (juce::Image::ARGB, logoScaled.getWidth() + 2 * pad, logoScaled.getHeight() + 2 * pad, true);
        { juce::Graphics lg (src); lg.drawImageAt (logoScaled, pad, pad); }
        logoGlow = juce::Image (juce::Image::ARGB, src.getWidth(), src.getHeight(), true);
        juce::ImageConvolutionKernel k (15);
        k.createGaussianBlur ((float) lh / 22.0f);
        k.applyToImage (logoGlow, src, src.getBounds());
    }

    stars.clear();
    juce::Random rnd (11);
    for (int i = 0; i < 12; ++i)
    {
        const float x = W * (0.42f + 0.34f * rnd.nextFloat());
        const float y = (6.0f + 34.0f * rnd.nextFloat()) * s;
        stars.push_back ({ { x, y }, (1.7f + 3.0f * rnd.nextFloat() * rnd.nextFloat()) * s });
    }
}

//==============================================================================
void OniriqueChannelStripEditor::paintStar (juce::Graphics& g, juce::Point<float> c, float r, float alpha)
{
    // etoile a 4 branches, reprise du sigil
    juce::Path st;
    const float k = 0.16f * r;
    st.startNewSubPath (c.x, c.y - r);
    st.lineTo (c.x + k, c.y - k); st.lineTo (c.x + r, c.y); st.lineTo (c.x + k, c.y + k);
    st.lineTo (c.x, c.y + r);     st.lineTo (c.x - k, c.y + k); st.lineTo (c.x - r, c.y); st.lineTo (c.x - k, c.y - k);
    st.closeSubPath();
    g.setColour (ember.withAlpha (0.25f * alpha));
    g.fillEllipse (juce::Rectangle<float> (r * 1.1f, r * 1.1f).withCentre (c));
    g.setColour (mist.withAlpha (alpha));
    g.fillPath (st);
}

void OniriqueChannelStripEditor::paintHeader (juce::Graphics& g)
{
    const float s = scale();
    const float W = (float) getWidth();
    const float t = (float) clock;

    for (size_t i = 0; i < stars.size(); ++i)
    {
        const float a = 0.15f + 0.75f * (0.5f + 0.5f * std::sin (t * (0.9f + 0.37f * (float) (i % 5)) + (float) i * 1.7f));
        paintStar (g, stars[i].first, stars[i].second, a);
    }

    // sigil : lueur braise qui respire + trait net
    if (logoScaled.isValid())
    {
        const float breathe = 0.5f + 0.5f * std::sin (t * juce::MathConstants<float>::twoPi / 5.0f);
        const float pad = logoRect.getHeight() / 4.0f;
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setColour (ember.withAlpha (0.55f + 0.35f * breathe));
        g.drawImage (logoGlow, logoRect.expanded (pad), juce::RectanglePlacement::stretchToFit, true);
        g.setColour (juce::Colours::white.withAlpha (0.08f + 0.10f * breathe));
        g.drawImage (logoGlow, logoRect.expanded (pad), juce::RectanglePlacement::stretchToFit, true);
        g.setOpacity (1.0f);
        g.drawImage (logoScaled, logoRect, juce::RectanglePlacement::stretchToFit);
    }

    // titre + chaine de traitement (ordre reel)
    {
        const float x = logoRect.getRight() + 9.0f * s;
        auto f1 = font (Face::display, 20.0f * s, true, 0.10f);
        auto f2 = font (Face::display, 20.0f * s, false, 0.10f);
        const float w1 = textWidth (f1, "ONIRIQUE") + 2.0f;
        const float w2 = textWidth (f2, "CHANNEL STRIP") + 2.0f;
        const float y = 6.0f * s, hh = 24.0f * s;
        g.setFont (f1);
        g.setColour (mist);
        g.drawText ("ONIRIQUE", juce::Rectangle<float> (x, y, w1, hh), juce::Justification::centredLeft, false);
        g.setColour (ember);
        g.drawText ("CHANNEL STRIP", juce::Rectangle<float> (x + w1 + 7.0f * s, y, w2 + 30.0f * s, hh), juce::Justification::centredLeft, false);

        g.setColour (haze.withAlpha (0.75f));
        g.setFont (fitFont (Face::mono, 7.0f * s, chainRect.getWidth(), shownChain, false, 0.06f));
        g.drawText (shownChain, chainRect, juce::Justification::centredLeft, false);
    }

    // libelles des meters IN / OUT
    g.setColour (haze);
    g.setFont (font (Face::mono, 7.5f * s, true, 0.08f));
    g.drawText ("IN",  juce::Rectangle<float> (772.0f * s,  9.0f * s, 24.0f * s, 12.0f * s), juce::Justification::centredRight, false);
    g.drawText ("OUT", juce::Rectangle<float> (772.0f * s, 27.0f * s, 24.0f * s, 12.0f * s), juce::Justification::centredRight, false);

    // ligne de signal : filet qui s'estompe aux bords + impulsion lumineuse qui la parcourt
    {
        const float ly = 49.5f * s;
        juce::ColourGradient cg (juce::Colours::transparentBlack, 10.0f * s, ly, juce::Colours::transparentBlack, W - 10.0f * s, ly, false);
        cg.addColour (0.08, line);
        cg.addColour (0.92, line);
        g.setGradientFill (cg);
        g.fillRect (juce::Rectangle<float> (10.0f * s, ly, W - 20.0f * s, std::max (1.0f, 0.7f * s)));

        const float len = 88.0f * s;
        const float span = W - 20.0f * s + len;
        const float px = 10.0f * s - len + std::fmod (t * 0.24f, 1.0f) * span;
        const auto streak = juce::Rectangle<float> (px, ly - 1.0f * s, len, 2.4f * s);
        juce::ColourGradient sg (bone.withAlpha (0.0f), streak.getX(), ly, ember, streak.getRight(), ly, false);
        sg.addColour (0.6, bone);
        g.reduceClipRegion (juce::Rectangle<float> (10.0f * s, signalRect.getY(), W - 20.0f * s, signalRect.getHeight()).toNearestInt());
        g.setColour (ember.withAlpha (0.25f));
        g.fillRoundedRectangle (streak.expanded (0.0f, 1.7f * s).withTrimmedLeft (len * 0.5f), 2.0f * s);
        g.setGradientFill (sg);
        g.fillRoundedRectangle (streak, streak.getHeight() * 0.5f);
    }
}

// fond fixe (degrades + ombres portees des cartes) rendu une fois par taille :
// les animations (potards, en-tete) ne recalculent jamais les flous d'ombre
void OniriqueChannelStripEditor::renderBackground()
{
    const float s = scale();
    const float W = (float) getWidth(), H = (float) getHeight();
    background = juce::Image (juce::Image::RGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), false);
    juce::Graphics g (background);

    // ---- fond (DA PRO SERIES) : noir profond, lueurs rouges tres discretes, grain fin
    g.fillAll (night);
    g.setGradientFill (juce::ColourGradient (blood.withAlpha (0.55f), W * 0.22f, H * 0.45f, night.withAlpha (0.0f), W * 0.22f + W * 0.6f, H * 0.45f, true));
    g.fillRect (getLocalBounds());
    g.setGradientFill (juce::ColourGradient (blood.withAlpha (0.22f), W * 0.92f, H * 1.05f, night.withAlpha (0.0f), W * 0.92f - W * 0.45f, H * 1.05f, true));
    g.fillRect (getLocalBounds());
    {
        juce::Random rnd (21);
        for (int i = 0; i < 120; ++i)
        {
            const float x = rnd.nextFloat() * W, y = rnd.nextFloat() * H, d = (0.5f + 1.2f * rnd.nextFloat() * rnd.nextFloat()) * s;
            g.setColour (mist.withAlpha (0.03f + 0.07f * rnd.nextFloat()));
            g.fillEllipse (x, y, d, d);
        }
    }

    // ---- ombres portees des cartes (elles debordent des panneaux)
    for (auto* c : std::initializer_list<juce::Component*> { sslPanel.get(), deEsserPanel.get(), saturationPanel.get(),
                                                             fairchildPanel.get(), f76Panel.get(), noiseGatePanel.get(), eqPanel.get() })
    {
        juce::Path card;
        card.addRoundedRectangle (c->getBounds().toFloat(), 10.0f * s);
        juce::DropShadow (juce::Colours::black.withAlpha (0.55f), juce::roundToInt (19.0f * s), { 0, juce::roundToInt (8.0f * s) })
            .drawForPath (g, card);
    }
}

void OniriqueChannelStripEditor::paint (juce::Graphics& g)
{
    if (background.getWidth() != getWidth() || background.getHeight() != getHeight())
        renderBackground();
    g.drawImageAt (background, 0, 0);

    juce::Graphics::ScopedSaveState ss (g);
    paintHeader (g);
}
