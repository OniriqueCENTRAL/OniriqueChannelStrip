#pragma once

#include <juce_graphics/juce_graphics.h>
#include <array>
#include <cmath>
#include <OniriqueLicenseData.h>

namespace onirique::ui
{

//==============================================================================
//  DA ONIRIQUE (identique a ONIRIQUE VOCALBOX) : nuit / sang, accent braise,
//  lignes fines, lueurs. Toutes les couleurs de l'interface viennent de ce
//  fichier -- aucune couleur en dur ailleurs.
//==============================================================================

// ---- VOCAL MINI PRO : le noir et rouge d'ONIRIQUE, en plus moderne : noir profond, charbon neutre, rouge sombre pour la
// profondeur ; le rouge vif est l'accent, utilisé avec mesure (arcs, témoins, choix actif) ; rose pâle pour les reflets ;
// blanc cassé pour les textes, gris doux pour le secondaire. Pas de néon : des lueurs discrètes, du verre sombre.
// (Les noms historiques sont gardés : « ember » est l'accent, « lilac » le reflet clair.)
inline const juce::Colour night  { 0xff09080A };   // noir profond (fond)
inline const juce::Colour blood  { 0xff3D0910 };   // rouge sombre (dégradés, profondeur)
inline const juce::Colour deep   { 0xff121012 };   // charbon très sombre (panneaux)
inline const juce::Colour deep2  { 0xff1C1517 };   // charbon chaud (menus, info-bulles, panneaux ouverts)
inline const juce::Colour mist   { 0xffF2ECEC };   // blanc cassé (textes)
inline const juce::Colour haze   { 0xff978C8F };   // gris doux (informations secondaires)
inline const juce::Colour ember  { 0xffE5384A };   // rouge ONIRIQUE (accent)
inline const juce::Colour lilac  { 0xffFFB3BC };   // rose pâle (queues de reverb, reflets, extrémités d'arcs)
inline const juce::Colour bone   { 0xffFFF4F5 };   // blanc le plus clair (aiguilles, crêtes)
inline const juce::Colour mint   { 0xff7DE3B6 };   // témoin discret « ça agit » (comme le voyant d'un rack)
inline const juce::Colour line   { (juce::uint8) 255, (juce::uint8) 214, (juce::uint8) 218, 0.10f };

// Polices : display (titres), body (libelles), mono (valeurs) : Unbounded, Hanken Grotesk et JetBrains Mono, les
// polices du site ONIRIQUE (fournies avec le module de licence). Chargees par FontScope (une par interface ouverte :
// seuls un compteur et un pointeur brut sont statiques, rien n'est detruit au dechargement du plugin) ; sans
// interface ouverte, des equivalents systeme.
enum class Face { display, body, mono };

struct BrandFaces { juce::Typeface::Ptr display, body, mono, monoBold; };
inline BrandFaces*& brandFaces() noexcept { static BrandFaces* faces = nullptr; return faces; }
inline int& brandFaceUsers() noexcept { static int users = 0; return users; }

struct FontScope
{
    FontScope()
    {
        if (brandFaceUsers()++ == 0)
        {
            auto* f = new BrandFaces();
            f->display = juce::Typeface::createSystemTypefaceFor (OniriqueLicenseData::Unbounded800_ttf, OniriqueLicenseData::Unbounded800_ttfSize);
            f->body = juce::Typeface::createSystemTypefaceFor (OniriqueLicenseData::HankenGrotesk600_ttf, OniriqueLicenseData::HankenGrotesk600_ttfSize);
            f->mono = juce::Typeface::createSystemTypefaceFor (OniriqueLicenseData::JetBrainsMono500_ttf, OniriqueLicenseData::JetBrainsMono500_ttfSize);
            f->monoBold = juce::Typeface::createSystemTypefaceFor (OniriqueLicenseData::JetBrainsMono600_ttf, OniriqueLicenseData::JetBrainsMono600_ttfSize);
            brandFaces() = f;
        }
    }
    ~FontScope()
    {
        if (--brandFaceUsers() == 0) { delete brandFaces(); brandFaces() = nullptr; }
    }
    JUCE_DECLARE_NON_COPYABLE (FontScope)
};

inline juce::Font font (Face f, float h, bool bold = false, float kerning = 0.0f)
{
    h = std::max (4.0f, h);
    const auto* faces = brandFaces();
    const juce::Typeface::Ptr tf = faces == nullptr ? nullptr
                                 : f == Face::display ? faces->display : f == Face::body ? faces->body : (bold ? faces->monoBold : faces->mono);
    // Unbounded est tres large : un peu plus petit que la hauteur demandee, pour garder les memes encombrements
    juce::Font fnt = tf != nullptr ? juce::Font (juce::FontOptions (tf).withHeight (f == Face::display ? h * 0.86f : h))
                                   : juce::Font (juce::FontOptions (f == Face::display ? "Bahnschrift" : f == Face::body ? "Segoe UI" : "Cascadia Mono",
                                                                    h, bold ? juce::Font::bold : juce::Font::plain));
    if (kerning != 0.0f) fnt.setExtraKerningFactor (kerning);
    return fnt;
}

inline float textWidth (const juce::Font& f, const juce::String& t)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, t, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// police de hauteur maxH, reduite si necessaire pour que le texte tienne dans maxW (jamais coupe)
inline juce::Font fitFont (Face face, float maxH, float maxW, const juce::String& text, bool bold = false, float kerning = 0.0f)
{
    auto f = font (face, maxH, bold, kerning);
    const float w = textWidth (f, text);
    if (w > maxW && w > 0.0f) f.setHeight (std::max (4.0f, f.getHeight() * maxW / w));
    return f;
}

// trait lumineux : plusieurs passes de plus en plus larges et transparentes sous le trait net
inline void strokeGlow (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width, float intensity)
{
    if (intensity <= 0.002f) return;
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, intensity * 0.17f / (float) i)));
        g.strokePath (p, juce::PathStrokeType (width * (1.0f + 1.15f * (float) i), juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
}

// approche exponentielle d'une valeur animee ; renvoie true tant qu'elle bouge
inline bool approach (float& v, float target, float k) noexcept
{
    if (std::abs (target - v) < 1.0e-3f) { const bool moved = v != target; v = target; return moved; }
    v += k * (target - v);
    return true;
}

/** Pastille de valeur (potards, champs) : fond sombre, lisere, point braise. */
inline void drawValuePill (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, float glow, float u)
{
    const float rad = r.getHeight() * 0.5f;
    g.setColour (juce::Colours::black.withAlpha (0.34f));
    g.fillRoundedRectangle (r, rad);
    g.setColour (ember.withAlpha (0.14f * glow));
    g.fillRoundedRectangle (r, rad);
    g.setColour (line.interpolatedWith (ember.withAlpha (0.8f), glow));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, std::max (1.0f, 1.2f * u));

    const float dot = r.getHeight() * 0.22f;
    const auto dotR = juce::Rectangle<float> (dot, dot).withCentre ({ r.getX() + rad * 0.95f, r.getCentreY() });
    g.setColour (ember.withAlpha (0.25f + 0.35f * glow));
    g.fillEllipse (dotR.expanded (dot * 0.6f * glow));
    g.setColour (ember.interpolatedWith (bone, 0.35f * glow));
    g.fillEllipse (dotR);

    const auto textR = r.withTrimmedLeft (rad * 1.5f).withTrimmedRight (rad * 0.6f);
    g.setColour (mist);
    g.setFont (fitFont (Face::mono, r.getHeight() * 0.56f, textR.getWidth(), text));
    g.drawText (text, textR, juce::Justification::centred, false);
}

/** Carte de module (verre sombre, filet fin, reflet haut, point braise +
    titre). L'ombre portee est dessinee par l'editeur, sous la carte. `s` =
    echelle maquette -> pixels. */
inline void paintCard (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, bool on,
                       float s, float titleMaxW)
{
    const float rad = 10.0f * s;
    juce::Path card;
    card.addRoundedRectangle (r.reduced (0.5f), rad);
    // verre sombre (DA PRO SERIES) : charbon, reflet rose pale tres leger en haut, filet fin
    g.setColour (deep.withAlpha (0.94f));
    g.fillPath (card);
    g.setGradientFill (juce::ColourGradient (lilac.withAlpha (0.035f), 0.0f, r.getY(), lilac.withAlpha (0.0f), 0.0f, r.getY() + r.getHeight() * 0.7f, false));
    g.fillPath (card);
    g.setColour (line);
    g.strokePath (card, juce::PathStrokeType (std::max (1.0f, 0.7f * s)));

    const float dot = 4.2f * s;
    const auto dotR = juce::Rectangle<float> (dot, dot).withCentre ({ r.getX() + 13.0f * s, r.getY() + 15.0f * s });
    g.setColour (ember.withAlpha (on ? 0.35f : 0.0f));
    g.fillEllipse (dotR.expanded (dot * 0.7f));
    g.setColour (on ? ember : haze.withAlpha (0.5f));
    g.fillEllipse (dotR);

    g.setColour (mist);
    g.setFont (fitFont (Face::display, 9.0f * s, titleMaxW, title, true, 0.06f));
    g.drawText (title, juce::Rectangle<float> (r.getX() + 22.0f * s, r.getY() + 6.0f * s, titleMaxW, 18.0f * s),
                juce::Justification::centredLeft, false);
}

//==============================================================================
//  Palette par module. Dans la DA ONIRIQUE, tous les modules partagent le meme
//  accent braise (comme le VocalBox) : la structure est conservee pour garder
//  l'API des panneaux, mais chaque entree pointe sur les memes teintes.
//==============================================================================

struct ModulePalette
{
    juce::Colour accent;
    juce::Colour accentBright;
    juce::Colour panelBase;
    juce::Colour border;
};

enum class ModuleKind { ssl, deEsser, saturation, fairchild, fet1176, noiseGate, eq };

inline const ModulePalette& paletteFor (ModuleKind) noexcept
{
    static const ModulePalette onirique { ember, bone, deep, line };
    return onirique;
}

/** Interpole lineairement deux couleurs. */
inline juce::Colour mix (juce::Colour a, juce::Colour b, float t) noexcept
{
    return a.interpolatedWith (b, juce::jlimit (0.0f, 1.0f, t));
}

//==============================================================================
//  Metriques
//==============================================================================

constexpr float baseCanvasWidth  = 1000.0f;
constexpr float baseCanvasHeight = 640.0f;

} // namespace onirique::ui
