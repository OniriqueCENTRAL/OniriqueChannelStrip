#include "ChainView.h"
#include "OniriqueKnobLogo.h"
#include "UIStyle.h"
#include "../Parameters.h"
#include <algorithm>

namespace onirique::ui
{

ChainView::ChainView (juce::AudioProcessorValueTreeState& apvts, ModuleOrderManager& orderManager,
                      std::function<float (const juce::String&)> reductionDb)
    : state (apvts), order (orderManager), reductionOf (std::move (reductionDb))
{
    using namespace onirique::ModuleIDs;
    const std::pair<const juce::String*, const char*> list[] {
        { &noiseGate, "GATE" }, { &sslChannel, "SSL" }, { &deEsser1, "DE-ESS 1" }, { &deEsser2, "DE-ESS 2" },
        { &saturation, "SATURATION" }, { &fet1176, "1176" }, { &fairchild670, "FAIRCHILD" }, { &eq, "EQ" } };
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        nodes[i].id = *list[i].first;
        nodes[i].name = list[i].second;
        nodes[i].bypass = apvts.getRawParameterValue (nodes[i].id + "_bypass");
    }
    setTitle ("Chaîne de traitement");
    setDescription ("Cliquer un module pour l'allumer ou le couper, le glisser pour changer l'ordre de traitement.");
    startTimerHz (30);
}

ChainView::~ChainView() { stopTimer(); }

float ChainView::slotX (int slot) const noexcept
{
    const float m = 22.0f * scale();
    return m + ((float) getWidth() - 2.0f * m) * (float) slot / 9.0f;
}

std::array<int, 8> ChainView::slots() const
{
    std::array<int, 8> idx { 0, 1, 2, 3, 4, 5, 6, 7 };
    std::stable_sort (idx.begin(), idx.end(), [this] (int a, int b) { return order.getOrder (nodes[(size_t) a].id) < order.getOrder (nodes[(size_t) b].id); });
    std::vector<int> seq (idx.begin(), idx.end());
    if (dragging >= 0)
    {
        seq.erase (std::find (seq.begin(), seq.end(), dragging));
        seq.insert (seq.begin() + (dropSlot() - 1), dragging);
    }
    std::array<int, 8> slotOf {};
    for (size_t p = 0; p < seq.size(); ++p)
        slotOf[(size_t) seq[p]] = (int) p + 1;
    return slotOf;
}

int ChainView::dropSlot() const noexcept
{
    const float step = (slotX (9) - slotX (0)) / 9.0f;
    return juce::jlimit (1, 8, juce::roundToInt ((dragX - slotX (0)) / juce::jmax (1.0f, step)));
}

int ChainView::nodeAt (juce::Point<float> p) const noexcept
{
    const float s = scale();
    for (size_t i = 0; i < nodes.size(); ++i)
        if (std::abs (p.x - nodes[i].x) < 34.0f * s && p.y > lineY() - 26.0f * s && p.y < lineY() + 34.0f * s)
            return (int) i;
    return -1;
}

void ChainView::timerCallback()
{
    clock += 1.0 / 30.0;
    const auto slotOf = slots();
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        auto& n = nodes[i];
        const float target = (int) i == dragging ? dragX : slotX (slotOf[i]);
        if (n.x < 0.0f) n.x = target; else approach (n.x, target, 0.3f);
        const float on = n.bypass != nullptr && n.bypass->load() > 0.5f ? 0.0f : 1.0f;
        if (n.lit < 0.0f) n.lit = on; else approach (n.lit, on, 0.25f);
        n.reduction = reductionOf != nullptr && on > 0.5f ? reductionOf (n.id) : 0.0f;
    }
    repaint();
}

void ChainView::paint (juce::Graphics& g)
{
    using logoknob::star;
    using logoknob::stroke;
    const float s = scale(), y = lineY();
    const float x0 = slotX (0), x9 = slotX (9);

    // le fil de la constellation, puis le signal qui le parcourt de IN à OUT
    g.setColour (bone.withAlpha (0.16f));
    g.fillRect (juce::Rectangle<float> (x0, y - 0.5f, x9 - x0, 1.0f));
    {
        const float len = 90.0f * s, t = (float) std::fmod (clock * 0.22, 1.0);
        const float px = x0 - len + t * (x9 - x0 + len);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (juce::Rectangle<float> (x0, y - 6.0f * s, x9 - x0, 12.0f * s).toNearestInt());
        juce::ColourGradient streak (bone.withAlpha (0.0f), px, y, ember, px + len, y, false);
        streak.addColour (0.7, bone.withAlpha (0.7f));
        g.setGradientFill (streak);
        g.fillRect (juce::Rectangle<float> (px, y - 0.9f * s, len, 1.8f * s));
    }

    // IN et OUT : deux petits anneaux aux bouts du fil
    for (int end = 0; end < 2; ++end)
    {
        const float x = end == 0 ? x0 : x9;
        g.setColour (bone.withAlpha (0.55f));
        g.drawEllipse (juce::Rectangle<float> (7.0f * s, 7.0f * s).withCentre ({ x, y }), juce::jmax (1.0f, 1.2f * s));
        g.setColour (haze);
        g.setFont (font (Face::body, 9.0f * s, true, 0.14f));
        g.drawText (end == 0 ? "IN" : "OUT", juce::Rectangle<float> (60.0f * s, 14.0f * s).withCentre ({ x, y + 20.0f * s }), juce::Justification::centred, false);
    }

    // les modules : l'étoile, sa braise quand il réduit le niveau, son rang au-dessus et son nom dessous
    const auto slotOf = slots();
    for (size_t pass = 0; pass < 2; ++pass)   // l'étoile glissée se dessine par-dessus les autres
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            if ((pass == 1) != ((int) i == dragging)) continue;
            const auto& n = nodes[i];
            const juce::Point<float> c (n.x, y);
            const bool hot = (int) i == hover || (int) i == dragging;
            const float r = (hot ? 10.0f : 8.5f) * s;

            g.setColour (night);
            g.fillEllipse (juce::Rectangle<float> (2.6f * r, 2.6f * r).withCentre (c));   // le fil s'arrête autour de l'étoile
            const float gr = juce::jlimit (0.0f, 1.0f, n.reduction / 8.0f);
            if (gr > 0.01f)
            {
                juce::Path halo;
                halo.addEllipse (juce::Rectangle<float> (2.4f * r, 2.4f * r).withCentre (c));
                stroke (g, halo, ember.withAlpha (0.25f + 0.6f * gr), 1.2f * s, 8.0f * s * gr);
            }
            star (g, c, r, bone.withAlpha (0.28f + 0.72f * n.lit), n.lit * 10.0f * s);

            g.setColour (haze.withAlpha (0.55f + 0.3f * n.lit));
            g.setFont (font (Face::mono, 8.0f * s));
            g.drawText (juce::String (slotOf[i]), juce::Rectangle<float> (30.0f * s, 12.0f * s).withCentre ({ c.x, y - 21.0f * s }), juce::Justification::centred, false);

            g.setColour (mist.withAlpha (0.38f + 0.5f * juce::jmax (n.lit, hot ? 0.8f : 0.0f)));
            g.setFont (fitFont (Face::body, 9.5f * s, 76.0f * s, n.name, true, 0.12f));
            g.drawText (n.name, juce::Rectangle<float> (80.0f * s, 14.0f * s).withCentre ({ c.x, y + 20.0f * s }), juce::Justification::centred, false);
        }

    g.setColour (haze.withAlpha (0.45f));
    g.setFont (font (Face::body, 8.5f * s, false, 0.04f));
    g.drawText (juce::String::fromUTF8 ("Cliquer un module pour l'allumer ou le couper · le glisser pour changer l'ordre"),
                juce::Rectangle<float> (x0, y + 34.0f * s, x9 - x0, 12.0f * s), juce::Justification::centred, false);
}

void ChainView::mouseMove (const juce::MouseEvent& e)
{
    const int h = nodeAt (e.position);
    if (h != hover) hover = h;
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void ChainView::mouseExit (const juce::MouseEvent&) { hover = -1; }

void ChainView::mouseDown (const juce::MouseEvent& e)
{
    pressed = nodeAt (e.position);
    dragging = -1;
}

void ChainView::mouseDrag (const juce::MouseEvent& e)
{
    if (pressed < 0) return;
    if (dragging < 0 && e.getDistanceFromDragStartX() * e.getDistanceFromDragStartX() > 16)
    {
        dragging = pressed;
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }
    if (dragging >= 0)
        dragX = juce::jlimit (slotX (1), slotX (8), e.position.x);
}

void ChainView::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        order.setOrder (nodes[(size_t) dragging].id, dropSlot());
    else if (pressed >= 0)
        if (auto* p = state.getParameter (nodes[(size_t) pressed].id + "_bypass"))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
            p->endChangeGesture();
        }
    pressed = dragging = -1;
    setMouseCursor (hover >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

} // namespace onirique::ui
