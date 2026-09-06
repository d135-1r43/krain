#include "gui/KrainLookAndFeel.h"

namespace krain::gui
{

namespace palette
{
juce::Colour forSemitones (float semitones)
{
    // Eased rather than linear, so +12 is already clearly warm - the octave is the
    // interval you most need to pick out of the cloud at a glance.
    if (semitones >= 0.0f)
    {
        const auto t = std::sqrt (juce::jlimit (0.0f, 1.0f, semitones / 24.0f));
        return unison.interpolatedWith (halo, t);
    }

    const auto t = std::sqrt (juce::jlimit (0.0f, 1.0f, -semitones / 24.0f));
    return unison.interpolatedWith (below, t);
}
} // namespace palette

//==============================================================================
namespace
{
/** Picks the first installed name from a preference list. Cached, because
    findAllTypefaceNames() walks the system font list and is far too slow to call
    from paint(). */
juce::String resolveTypeface (Face face)
{
    static const auto available = juce::Font::findAllTypefaceNames();

    const auto pick = [] (std::initializer_list<const char*> names) -> juce::String
    {
        for (const auto* name : names)
            if (available.contains (name))
                return name;

        return {};
    };

    static const juce::String display = pick ({ "Futura", "Avenir Next", "Gill Sans", "Optima" });
    static const juce::String caption = pick ({ "Avenir Next Condensed", "Avenir Next", "Futura" });
    static const juce::String value = pick ({ "Menlo", "SF Mono", "Consolas", "Courier New" });

    switch (face)
    {
        case Face::display: return display;
        case Face::caption: return caption;
        case Face::value:   return value;
    }

    return {};
}
} // namespace

juce::Font font (Face face, float height, bool bold)
{
    auto options = juce::FontOptions().withHeight (height);

    if (const auto name = resolveTypeface (face); name.isNotEmpty())
        options = options.withName (name);

    if (bold)
        options = options.withStyle ("Bold");

    return juce::Font (options);
}

//==============================================================================
KrainLookAndFeel::KrainLookAndFeel()
{
    using namespace palette;

    setColour (juce::ResizableWindow::backgroundColourId, ink);
    setColour (juce::Label::textColourId, bone);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);

    setColour (juce::Slider::rotarySliderFillColourId, halo);
    setColour (juce::Slider::rotarySliderOutlineColourId, line);
    setColour (juce::Slider::thumbColourId, bone);
    setColour (juce::Slider::textBoxTextColourId, bone);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, halo.withAlpha (0.3f));

    setColour (juce::ToggleButton::tickColourId, halo);
    setColour (juce::ToggleButton::tickDisabledColourId, line);

    setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId, bone);
    setColour (juce::ComboBox::arrowColourId, boneDim);
    setColour (juce::ComboBox::focusedOutlineColourId, halo);

    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, bone);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, halo);
    setColour (juce::PopupMenu::highlightedTextColourId, inkDeep);

    setColour (juce::GroupComponent::outlineColourId, line);
    setColour (juce::GroupComponent::textColourId, boneDim);

    setColour (juce::CaretComponent::caretColourId, halo);
    setColour (juce::TextEditor::highlightColourId, halo.withAlpha (0.3f));
    setColour (juce::TextEditor::textColourId, bone);
    setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, halo);
}

//==============================================================================
void KrainLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float startAngle, float endAngle,
                                         juce::Slider& slider)
{
    // No knob body, no bevel, no shadow: a track, a value arc and a hairline
    // pointer. The value is readable from across the room; nothing pretends to be
    // a physical object.
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const auto thickness = juce::jmax (2.0f, radius * 0.11f);
    const auto arcRadius = radius - thickness * 0.5f;

    const auto stroke = juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (track, stroke);

    if (std::abs (angle - startAngle) > 1.0e-3f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId)
                         .withAlpha (slider.isEnabled() ? 1.0f : 0.4f));
        g.strokePath (value, stroke);
    }

    const auto outer = centre.getPointOnCircumference (arcRadius - thickness * 0.8f, angle);
    const auto inner = centre.getPointOnCircumference (arcRadius * 0.42f, angle);
    g.setColour (slider.findColour (juce::Slider::thumbColourId).withAlpha (0.85f));
    g.drawLine (juce::Line<float> (inner, outer), 1.3f);
}

//==============================================================================
void KrainLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsDown);

    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto side = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto box = juce::Rectangle<float> (side, side).withCentre (bounds.getCentre());
    const auto on = button.getToggleState();

    g.setColour (on ? palette::halo
                    : palette::line.brighter (shouldDrawButtonAsHighlighted ? 0.35f : 0.0f));
    g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.2f);

    if (on)
    {
        g.setColour (palette::halo);
        g.fillRoundedRectangle (box.reduced (side * 0.28f), 1.5f);
    }
}

//==============================================================================
void KrainLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                     int buttonX, int buttonY, int buttonW, int buttonH,
                                     juce::ComboBox& box)
{
    juce::ignoreUnused (isButtonDown, buttonX, buttonY, buttonW, buttonH);

    // A single underline instead of a bordered field - the choice reads as text
    // with an affordance, which sits quieter next to the arcs.
    auto bounds = juce::Rectangle<int> (width, height).toFloat();

    g.setColour (box.hasKeyboardFocus (false) ? palette::halo : palette::line);
    g.fillRect (bounds.removeFromBottom (1.0f).reduced (2.0f, 0.0f));

    juce::Path arrow;
    const auto cx = (float) width - 9.0f;
    const auto cy = (float) height * 0.5f - 1.0f;
    arrow.addTriangle (cx - 3.5f, cy - 1.5f, cx + 3.5f, cy - 1.5f, cx, cy + 2.5f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void KrainLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (2, 0, box.getWidth() - 16, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font KrainLookAndFeel::getComboBoxFont (juce::ComboBox&) { return font (Face::caption, 14.0f); }
juce::Font KrainLookAndFeel::getPopupMenuFont() { return font (Face::caption, 15.0f); }
juce::Font KrainLookAndFeel::getLabelFont (juce::Label& label) { return label.getFont(); }

//==============================================================================
juce::Label* KrainLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);

    // LookAndFeel_V4 gives the value box a background and an outline of its own.
    // Strip them: a number that needs a frame around it is a number in a form,
    // not a reading off an instrument.
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::backgroundWhenEditingColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineWhenEditingColourId, palette::halo);
    label->setColour (juce::Label::textWhenEditingColourId, palette::bone);
    label->setFont (font (Face::value, 11.5f));
    label->setJustificationType (juce::Justification::centred);

    return label;
}

//==============================================================================
juce::Slider::SliderLayout KrainLookAndFeel::getSliderLayout (juce::Slider& slider)
{
    // Value text sits under the arc, not inside it: the arc stays a clean ring and
    // the numbers line up across a whole row of controls.
    auto bounds = slider.getLocalBounds();
    const auto textHeight = 16;

    juce::Slider::SliderLayout layout;
    layout.textBoxBounds = bounds.removeFromBottom (textHeight);
    layout.sliderBounds = bounds;

    return layout;
}

} // namespace krain::gui
