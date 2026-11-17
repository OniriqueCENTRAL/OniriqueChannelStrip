#include "ParamControls.h"
#include <cmath>

namespace onirique::ui
{

juce::String formatKnobValue (float value, const juce::String& unitLabel)
{
    if (unitLabel == "Hz")
    {
        if (std::abs (value) >= 1000.0f)
            return juce::String (value / 1000.0f, 1) + "k";
        return juce::String (juce::roundToInt (value));
    }

    if (unitLabel == "ms")
    {
        if (std::abs (value) >= 1000.0f)
            return juce::String (value / 1000.0f, 1) + "k";
        if (std::abs (value) < 10.0f)
            return juce::String (value, 1);
        return juce::String (juce::roundToInt (value));
    }

    if (unitLabel == "dB")
    {
        const juce::String sign = value > 0.05f ? "+" : juce::String();
        return sign + juce::String (value, 1);
    }

    if (unitLabel == "%")
        return juce::String (juce::roundToInt (value));

    if (unitLabel == ":1")
        return std::abs (value) < 10.0f ? juce::String (value, 1) : juce::String (juce::roundToInt (value));

    return juce::String (value, std::abs (value) < 10.0f ? 2 : 1);
}

namespace
{
    constexpr float kPi = juce::MathConstants<float>::pi;

    juce::String withUnit (const juce::String& formatted, const juce::String& unitLabel)
    {
        if (unitLabel.isEmpty())
            return formatted;
        if (unitLabel == ":1")
            return formatted + ":1";
        return formatted + " " + unitLabel;
    }
}

//==============================================================================
ParamKnob::ParamKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& displayLabel)
    : displayName (displayLabel)
{
    auto* param = apvts.getParameter (paramID);
    jassert (param != nullptr);

    unitLabel = param != nullptr ? param->getLabel() : juce::String();

    slider.setRotaryParameters (kPi * -0.75f, kPi * 0.75f, true);
    slider.setMouseDragSensitivity (220);
    auto& props = slider.getProperties();
    props.set ("shown", 0.0f);     // à l'ouverture, les potards se déploient jusqu'à leur valeur
    props.set ("glow", 0.0f);

    // « Constellation » pour la dynamique, « Sceau » pour le timbre (les gains et l'EQ de la tranche SSL compris)
    static const juce::StringArray sslDynamics { "threshold", "ratio", "attack", "release", "makeup", "gateThreshold" };
    const bool dynamics = paramID.startsWith ("noisegate") || paramID.startsWith ("deesser") || paramID.startsWith ("fet1176")
                       || paramID.startsWith ("fairchild670")
                       || (paramID.startsWith ("sslchannel_") && sslDynamics.contains (paramID.fromFirstOccurrenceOf ("_", false, false)));
    props.set ("model", dynamics ? "constellation" : "sceau");
    addAndMakeVisible (slider);

    if (param != nullptr)
    {
        attachment = std::make_unique<juce::SliderParameterAttachment> (*param, slider);
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));   // double-clic = défaut

        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto range = ranged->getNormalisableRange();
            props.set ("bipolar", range.start < 0.0f && range.end > 0.0f);   // gains +/- : depuis le centre
        }
    }

    slider.onDragStart   = [this] { dragging = true;  animate(); };
    slider.onDragEnd     = [this] { dragging = false; animate(); };
    slider.onValueChange = [this] { animate(); repaint (valueArea.getSmallestIntegerContainer().expanded (2)); };
    slider.addMouseListener (this, false);

    animate();
}

ParamKnob::~ParamKnob()
{
    stopTimer();
    slider.removeMouseListener (this);
}

void ParamKnob::mouseEnter (const juce::MouseEvent&) { hovering = true;  animate(); }
void ParamKnob::mouseExit (const juce::MouseEvent&)  { hovering = isMouseOver (true); animate(); }

juce::String ParamKnob::valueText() const
{
    return withUnit (formatKnobValue ((float) slider.getValue(), unitLabel), unitLabel);
}

void ParamKnob::timerCallback()
{
    const float target = (float) slider.valueToProportionOfLength (slider.getValue());
    bool changed = approach (shown, target, 0.30f);
    changed |= approach (glow, dragging ? 1.0f : (hovering ? 0.75f : 0.0f), 0.2f);

    auto& props = slider.getProperties();
    props.set ("shown", shown);
    props.set ("glow", glow);
    slider.repaint();
    repaint (nameArea.getUnion (valueArea).getSmallestIntegerContainer().expanded (2));

    if (! changed)
        stopTimer();
}

void ParamKnob::resized()
{
    const auto area = getLocalBounds().toFloat();
    const float s = uiScale;
    nameArea = area.withHeight (14.0f * s);
    const auto dial = juce::Rectangle<float> (dialD, dialD).withCentre ({ area.getCentreX(), nameArea.getBottom() + 2.0f * s + dialD * 0.5f });
    // les potards du logo laissent une marge pour leur lueur : la zone dessinée déborde un peu de la case du potard
    slider.setBounds (dial.expanded (dialD * 0.2f).toNearestInt());
    valueArea = juce::Rectangle<float> (area.getWidth(), 14.0f * s).withCentre ({ area.getCentreX(), dial.getBottom() + 3.0f * s + 7.0f * s });
}

void ParamKnob::paint (juce::Graphics& g)
{
    const float s = uiScale;
    g.setColour (haze.interpolatedWith (mist, glow));
    g.setFont (fitFont (Face::body, 8.6f * s, nameArea.getWidth(), displayName, true, 0.12f));
    g.drawText (displayName, nameArea, juce::Justification::centred, false);

    g.setColour (mist.withAlpha (0.72f).interpolatedWith (ember.interpolatedWith (bone, 0.35f), glow));
    g.setFont (fitFont (Face::mono, 8.8f * s, valueArea.getWidth(), valueText()));
    g.drawText (valueText(), valueArea, juce::Justification::centred, false);
}

PowerSwitch::PowerSwitch()
{
    setClickingTogglesState (true);
}

void PowerSwitch::timerCallback()
{
    if (! approach (on, target(), 0.2f)) stopTimer();
    repaint();
}

void PowerSwitch::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    if (on < 0.0f) on = target();
    if (! juce::approximatelyEqual (on, target()) && ! isTimerRunning()) startTimerHz (60);

    const auto b = getLocalBounds().toFloat();
    const float d = std::min (b.getWidth(), b.getHeight()) * 0.8f;
    if (d < 6.0f) return;
    const auto c = b.getCentre();
    const auto disk = juce::Rectangle<float> (d, d).withCentre (c);
    const float u = d / 24.0f;

    juce::Path ring;
    ring.addEllipse (disk);
    strokeGlow (g, ring, ember, 1.4f * u, on * (highlighted ? 1.0f : 0.7f));
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillEllipse (disk);
    g.setColour (ember.withAlpha (0.16f * on));
    g.fillEllipse (disk);
    g.setColour (line.interpolatedWith (ember, on).withMultipliedAlpha (highlighted ? 1.0f : 0.9f));
    g.drawEllipse (disk, std::max (1.0f, 1.4f * u));

    // pictogramme power : arc ouvert en haut + barre verticale, extremites arrondies
    const float r = 0.26f * d, stroke = std::max (1.2f, 0.09f * d);
    juce::Path p;
    p.addCentredArc (c.x, c.y + 0.02f * d, r, r, 0.0f, 0.23f * kPi, 1.77f * kPi, true);
    p.startNewSubPath (c.x, c.y - 0.32f * d);
    p.lineTo (c.x, c.y - 0.04f * d);
    g.setColour (haze.interpolatedWith (bone, juce::jmax (on, highlighted ? 0.5f : 0.0f)));
    g.strokePath (p, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
PillButton::PillButton (const juce::String& label) : juce::ToggleButton (label)
{
    setClickingTogglesState (true);
}

void PillButton::timerCallback()
{
    if (! approach (on, getToggleState() ? 1.0f : 0.0f, 0.16f)) stopTimer();
    repaint();
}

void PillButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const float tgt = getToggleState() ? 1.0f : 0.0f;
    if (on < 0.0f) on = tgt;
    if (! juce::approximatelyEqual (on, tgt) && ! isTimerRunning()) startTimerHz (60);

    const auto r = getLocalBounds().toFloat().reduced (1.5f);
    const float u = r.getHeight() / 26.0f;
    const float radius = r.getHeight() * 0.5f;

    juce::Path shape;
    shape.addRoundedRectangle (r, radius);
    strokeGlow (g, shape, ember, 1.2f * u, on * 0.8f);
    g.setColour (juce::Colours::black.withAlpha (highlighted ? 0.28f : 0.36f));
    g.fillPath (shape);
    if (on > 0.001f)
    {
        g.setGradientFill (juce::ColourGradient (ember.withAlpha (0.30f * on), r.getX(), r.getY(),
                                                 blood.withAlpha (0.25f * on), r.getRight(), r.getBottom(), false));
        g.fillPath (shape);
    }
    g.setColour (line.interpolatedWith (ember, on));
    g.strokePath (shape, juce::PathStrokeType (std::max (1.0f, 1.3f * u)));

    const float d = r.getHeight() * 0.28f;
    const float pad = r.getHeight() * 0.36f;
    const auto ledR = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + pad + d * 0.5f, r.getCentreY() });
    g.setColour (ember.withAlpha (0.35f * on));
    g.fillEllipse (ledR.expanded (d * 0.8f));
    g.setColour (haze.withAlpha (0.35f).interpolatedWith (ember, on));
    g.fillEllipse (ledR);

    const auto textR = r.withTrimmedLeft (pad * 1.6f + d).withTrimmedRight (pad * 0.6f);
    g.setColour (haze.interpolatedWith (bone, juce::jmax (on, highlighted ? 0.6f : 0.0f)));
    g.setFont (fitFont (Face::body, r.getHeight() * 0.50f, textR.getWidth(), getButtonText(), true, 0.1f));
    g.drawText (getButtonText(), textR, juce::Justification::centred, false);
}

//==============================================================================
ParamToggle::ParamToggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                          const juce::String& displayLabel, juce::Colour, juce::Colour)
    : button (displayLabel)
{
    addAndMakeVisible (button);

    if (auto* param = apvts.getParameter (paramID))
        attachment = std::make_unique<juce::ButtonParameterAttachment> (*param, button);
    else
        jassertfalse;
}

void ParamToggle::resized() { button.setBounds (getLocalBounds()); }

//==============================================================================
ParamSegmented::ParamSegmented (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                                juce::Colour, juce::Colour)
{
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramID)))
    {
        labels = choiceParam->choices;
        attachment = std::make_unique<juce::ParameterAttachment> (*choiceParam, [this] (float v)
        {
            const int idx = juce::jlimit (0, juce::jmax (0, labels.size() - 1), juce::roundToInt (v));
            if (idx != selected)
            {
                selected = idx;
                if (! isTimerRunning()) startTimerHz (60);
            }
        });
        attachment->sendInitialUpdate();
        slide = (float) selected;
    }
    else
    {
        jassertfalse;
    }
}

ParamSegmented::~ParamSegmented() { stopTimer(); }

int ParamSegmented::segmentAt (float x) const noexcept
{
    if (labels.isEmpty() || getWidth() <= 0) return -1;
    return juce::jlimit (0, labels.size() - 1, (int) (x * (float) labels.size() / (float) getWidth()));
}

void ParamSegmented::timerCallback()
{
    if (! approach (slide, (float) selected, 0.22f)) stopTimer();
    repaint();
}

void ParamSegmented::mouseDown (const juce::MouseEvent& e)
{
    const int idx = segmentAt (e.position.x);
    if (idx >= 0 && idx != selected && attachment != nullptr)
        attachment->setValueAsCompleteGesture ((float) idx);
}

void ParamSegmented::mouseMove (const juce::MouseEvent& e)
{
    const int h = segmentAt (e.position.x);
    if (h != hover) { hover = h; repaint(); }
}

void ParamSegmented::mouseExit (const juce::MouseEvent&)
{
    if (hover != -1) { hover = -1; repaint(); }
}

void ParamSegmented::paint (juce::Graphics& g)
{
    const int n = labels.size();
    if (n == 0) return;

    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float u = r.getHeight() / 26.0f;
    const float radius = r.getHeight() * 0.5f;
    const float segW = r.getWidth() / (float) n;
    const float pos = slide < 0.0f ? (float) selected : slide;

    g.setColour (juce::Colours::black.withAlpha (0.34f));
    g.fillRoundedRectangle (r, radius);
    g.setColour (line);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, std::max (1.0f, 1.1f * u));

    // pastille braise qui glisse vers le choix
    const auto pill = juce::Rectangle<float> (r.getX() + segW * pos, r.getY(), segW, r.getHeight()).reduced (2.5f * u);
    juce::Path pp;
    pp.addRoundedRectangle (pill, pill.getHeight() * 0.5f);
    strokeGlow (g, pp, ember, 1.0f * u, 0.6f);
    g.setGradientFill (juce::ColourGradient (ember.brighter (0.12f), pill.getX(), pill.getY(),
                                             ember.darker (0.35f), pill.getX(), pill.getBottom(), false));
    g.fillPath (pp);

    for (int i = 0; i < n; ++i)
    {
        const juce::Rectangle<float> seg (r.getX() + segW * (float) i, r.getY(), segW, r.getHeight());
        const float sel = juce::jlimit (0.0f, 1.0f, 1.0f - std::abs (pos - (float) i));
        const auto base = i == hover ? mist : haze;
        g.setColour (base.interpolatedWith (night, sel));
        g.setFont (fitFont (Face::body, r.getHeight() * 0.50f, seg.getWidth() - 6.0f * u, labels[i], true, 0.06f));
        g.drawText (labels[i], seg, juce::Justification::centred, false);
    }
}

//==============================================================================
ParamCombo::ParamCombo (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, juce::Colour)
{
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramID)))
        box.addItemList (choiceParam->choices, 1);
    else
        jassertfalse;

    addAndMakeVisible (box);

    if (auto* param = apvts.getParameter (paramID))
        attachment = std::make_unique<juce::ComboBoxParameterAttachment> (*param, box);
}

void ParamCombo::resized() { box.setBounds (getLocalBounds()); }

//==============================================================================
SectionLabel::SectionLabel (const juce::String& text, juce::Colour, bool withRule)
    : labelText (text), hasRule (withRule)
{
    setInterceptsMouseClicks (false, false);
}

void SectionLabel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float h = b.getHeight();

    // sans filet : le nom d'une bande (LF, LMF…), centré au-dessus de sa colonne
    if (! hasRule)
    {
        g.setColour (haze.withAlpha (0.85f));
        g.setFont (fitFont (Face::body, h * 0.78f, b.getWidth(), labelText, true, 0.12f));
        g.drawText (labelText, b, juce::Justification::centred, false);
        return;
    }

    // avec filet : le titre d'un groupe (EQ, COMPRESSEUR), puis un filet jusqu'au bout
    const auto f = fitFont (Face::body, h * 0.72f, b.getWidth(), labelText, true, 0.14f);
    const float tw = std::min (b.getWidth(), textWidth (f, labelText) + 4.0f);
    g.setColour (mist.withAlpha (0.7f));
    g.setFont (f);
    g.drawText (labelText, b.removeFromLeft (tw), juce::Justification::centredLeft, false);
    if (b.getWidth() > 8.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (juce::Rectangle<float> (b.getX() + 6.0f, b.getCentreY(), b.getWidth() - 6.0f, 1.0f));
    }
}

} // namespace onirique::ui
