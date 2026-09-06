#include "gui/ParameterComponents.h"

#include "ParameterIds.h"
#include "gui/KrainLookAndFeel.h"

namespace krain::gui
{

namespace
{
constexpr int captionHeight = 18;
constexpr int controlPadding = 4;
constexpr int groupTitleHeight = 22;

void styleCaption (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (Face::caption, 13.0f));
    label.setColour (juce::Label::textColourId, palette::boneDim);
    label.setInterceptsMouseClicks (false, false);
}
} // namespace

//==============================================================================
ParameterSlider::ParameterSlider (juce::AudioProcessorValueTreeState& state,
                                  const juce::String& parameterId,
                                  const juce::String& caption)
{
    styleCaption (label, caption);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 16);
    addAndMakeVisible (slider);

    // std::make_unique is the C++ equivalent of `new` for an owned object: the
    // unique_ptr frees it in this component's destructor, no matter how we exit.
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameterId, slider);

    // After the attachment, not before: SliderAttachment installs its own
    // text/value conversion in its constructor and would overwrite anything set up
    // ahead of it.
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (parameterId)))
    {
        const auto range = ranged->getNormalisableRange();
        const auto suffix = ranged->getLabel().trim();

        if (suffix.isEmpty() && range.start == 0.0f && range.end <= 1.5f)
        {
            // A bare 0-to-1 number tells a musician nothing; percent does. Feedback
            // runs to 1.2, and "120 %" is exactly how that should read.
            slider.textFromValueFunction = [] (double value)
            { return juce::String (juce::roundToInt (value * 100.0)) + " %"; };
            slider.valueFromTextFunction = [] (const juce::String& text)
            { return text.getDoubleValue() * 0.01; };
        }
        else
        {
            const auto places = range.interval >= 1.0f ? 0 : (range.interval >= 0.1f ? 1 : 2);
            const auto unit = suffix.isEmpty() ? juce::String() : " " + suffix;

            slider.textFromValueFunction = [places, unit] (double value)
            {
                // Without this, a parameter resting on zero displays as "-0.00".
                if (std::abs (value) < 5.0e-3)
                    value = 0.0;

                return juce::String (value, places) + unit;
            };
            slider.valueFromTextFunction = [] (const juce::String& text) { return text.getDoubleValue(); };
        }

        slider.updateText();
    }
}

void ParameterSlider::resized()
{
    auto bounds = getLocalBounds().reduced (controlPadding, 0);
    label.setBounds (bounds.removeFromTop (captionHeight));
    slider.setBounds (bounds);
}

//==============================================================================
ParameterToggle::ParameterToggle (juce::AudioProcessorValueTreeState& state,
                                  const juce::String& parameterId,
                                  const juce::String& caption)
{
    styleCaption (label, caption);
    addAndMakeVisible (label);

    button.setButtonText ({});
    addAndMakeVisible (button);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, parameterId, button);
}

void ParameterToggle::resized()
{
    auto bounds = getLocalBounds().reduced (controlPadding, 0);
    label.setBounds (bounds.removeFromTop (captionHeight));

    // Centre a fixed-size tick box in the remaining space.
    const auto box = juce::Rectangle<int> (30, 30).withCentre (bounds.getCentre());
    button.setBounds (box);
}

//==============================================================================
ParameterChoice::ParameterChoice (juce::AudioProcessorValueTreeState& state,
                                  const juce::String& parameterId,
                                  const juce::String& caption)
{
    styleCaption (label, caption);
    addAndMakeVisible (label);

    // The attachment does NOT populate the box - it only keeps the selected index
    // in sync. The items have to be added first, and they have to be added before
    // the attachment exists, or the initial selection has nothing to select.
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (parameterId)))
        comboBox.addItemList (choice->choices, 1);

    comboBox.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (comboBox);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, parameterId, comboBox);
}

void ParameterChoice::resized()
{
    auto bounds = getLocalBounds().reduced (controlPadding, 0);
    label.setBounds (bounds.removeFromTop (captionHeight));
    comboBox.setBounds (bounds.withSizeKeepingCentre (bounds.getWidth(), 26));
}

//==============================================================================
ParameterGroup::ParameterGroup (juce::String groupTitle) : title (std::move (groupTitle))
{
}

void ParameterGroup::addControl (juce::Component& control)
{
    // The reference overload of addAndMakeVisible means "borrow"; the pointer
    // overload would mean "take ownership". We borrow.
    addAndMakeVisible (control);
    controls.add (&control);

    // A Slider builds its value-box Label once, in its own constructor, using
    // whatever LookAndFeel was reachable at that moment - which is the default one,
    // because the controls are members and exist before the editor installs ours.
    // Drawing resolves the LookAndFeel live and so was already correct; the Label
    // was not. Now that this control is parented, ask for the rebuild explicitly.
    control.sendLookAndFeelChange();
}

void ParameterGroup::paint (juce::Graphics& g)
{
    // A group is a hairline rule with a name on it, not a box. Fewer edges means
    // the cloud above stays the only thing that draws the eye.
    const auto titleArea = getLocalBounds().removeFromTop (groupTitleHeight).reduced (2, 0);
    const auto titleFont = font (Face::display, 10.5f, true);

    g.setColour (palette::boneDim.withAlpha (0.85f));
    g.setFont (titleFont);

    // Manual tracking: JUCE has no letter-spacing, and the wide caps are half of
    // what makes these labels read as gallery rather than as engineering.
    auto x = (float) titleArea.getX();
    const auto baseline = (float) titleArea.getCentreY() + 4.0f;

    for (const auto character : title.toUpperCase())
    {
        const juce::String glyph (juce::String::charToString (character));
        g.drawSingleLineText (glyph, juce::roundToInt (x), juce::roundToInt (baseline));
        x += juce::GlyphArrangement::getStringWidth (titleFont, glyph) + 1.7f;
    }

    g.setColour (palette::line);
    const auto ruleY = (float) titleArea.getBottom() - 3.0f;
    g.fillRect (x + 6.0f, ruleY, (float) getWidth() - x - 8.0f, 1.0f);
}

void ParameterGroup::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (groupTitleHeight + 2);

    if (controls.isEmpty())
        return;

    const auto columnWidth = bounds.getWidth() / controls.size();

    for (auto* control : controls)
        control->setBounds (bounds.removeFromLeft (columnWidth));
}

} // namespace krain::gui
