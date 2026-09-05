#include "gui/ParameterComponents.h"

#include "ParameterIds.h"

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
    label.setFont (juce::FontOptions (13.0f));
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
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
    addAndMakeVisible (slider);

    // std::make_unique is the C++ equivalent of `new` for an owned object: the
    // unique_ptr frees it in this component's destructor, no matter how we exit.
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameterId, slider);
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

    addAndMakeVisible (comboBox);

    // The attachment fills the combo box with the parameter's choices for us.
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
}

void ParameterGroup::paint (juce::Graphics& g)
{
    // Everything here goes through the LookAndFeel colour ids, so dropping in a
    // custom LookAndFeel later restyles the group without touching this code.
    auto& lookAndFeel = getLookAndFeel();

    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (lookAndFeel.findColour (juce::GroupComponent::outlineColourId).withAlpha (0.5f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    g.setColour (lookAndFeel.findColour (juce::GroupComponent::textColourId));
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (title,
                getLocalBounds().removeFromTop (groupTitleHeight).reduced (10, 0),
                juce::Justification::centredLeft);
}

void ParameterGroup::resized()
{
    auto bounds = getLocalBounds().reduced (6);
    bounds.removeFromTop (groupTitleHeight - 6);

    if (controls.isEmpty())
        return;

    const auto columnWidth = bounds.getWidth() / controls.size();

    for (auto* control : controls)
        control->setBounds (bounds.removeFromLeft (columnWidth));
}

} // namespace krain::gui
