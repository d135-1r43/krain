#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace graindelay::gui
{

//==============================================================================
/** A caption plus a rotary slider, wired to one APVTS parameter.

    C++ note (Java dev): members are destroyed in *reverse* declaration order, so
    the attachment is declared last and therefore torn down first. If it outlived
    the slider it would unregister itself from a dangling object. Getting this
    order right is the whole trick with JUCE attachments.
*/
class ParameterSlider final : public juce::Component
{
public:
    ParameterSlider (juce::AudioProcessorValueTreeState& state,
                     const juce::String& parameterId,
                     const juce::String& caption);

    void resized() override;

private:
    juce::Label label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterSlider)
};

//==============================================================================
/** A caption plus a toggle button, wired to one APVTS bool parameter. */
class ParameterToggle final : public juce::Component
{
public:
    ParameterToggle (juce::AudioProcessorValueTreeState& state,
                     const juce::String& parameterId,
                     const juce::String& caption);

    void resized() override;

private:
    juce::Label label;
    juce::ToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterToggle)
};

//==============================================================================
/** A caption plus a combo box, wired to one APVTS choice parameter. */
class ParameterChoice final : public juce::Component
{
public:
    ParameterChoice (juce::AudioProcessorValueTreeState& state,
                     const juce::String& parameterId,
                     const juce::String& caption);

    void resized() override;

private:
    juce::Label label;
    juce::ComboBox comboBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterChoice)
};

//==============================================================================
/** A titled box that lays its children out in a row.

    It does *not* own the controls: addControl() takes a reference and only keeps a
    pointer. The editor holds the controls as members, so their lifetime is already
    tied to the editor's - which outlives this group.
*/
class ParameterGroup final : public juce::Component
{
public:
    explicit ParameterGroup (juce::String groupTitle);

    /** Borrows the control; the caller stays the owner. */
    void addControl (juce::Component& control);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::String title;
    juce::Array<juce::Component*> controls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterGroup)
};

} // namespace graindelay::gui
