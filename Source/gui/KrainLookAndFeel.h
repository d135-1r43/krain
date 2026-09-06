#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace krain::gui
{

//==============================================================================
/** The plug-in's visual identity.

    Ground is deep blue-green ink rather than neutral black, and text is warm bone
    rather than grey - the cool/warm tension is what stops it reading as another
    dark plug-in. Boldness is spent in exactly two places: the gold of a value arc,
    and the grain cloud. Everything else stays quiet.

    Colour never appears as a literal in a component; it goes through these named
    values or through a LookAndFeel colour id, so the whole plug-in can be
    restyled from this one file.
*/
namespace palette
{
inline const juce::Colour ink { 0xff0b1418 };     ///< ground
inline const juce::Colour inkDeep { 0xff060d10 }; ///< recessed / gradient floor
inline const juce::Colour panel { 0xff101d22 };   ///< group surfaces
inline const juce::Colour line { 0xff1d2f37 };    ///< hairlines, slider tracks
inline const juce::Colour bone { 0xffe8e1d4 };    ///< primary text, warm
inline const juce::Colour boneDim { 0xff7e8b92 }; ///< secondary text, cool
inline const juce::Colour halo { 0xffe0a54a };    ///< gold: value, grains above unison
inline const juce::Colour unison { 0xff7fb2c4 };  ///< pale blue: grains at unison
inline const juce::Colour below { 0xff6e7fc4 };   ///< periwinkle: grains below unison

/** Warm means high. The ramp encodes transposition, so the shimmer structure is
    legible as colour before you read a single number. */
juce::Colour forSemitones (float semitones);
} // namespace palette

//==============================================================================
/** Which of the three typefaces a piece of text belongs to. */
enum class Face
{
    display, ///< Futura - wordmark and group titles, uppercase and tracked
    caption, ///< Avenir Next Condensed - control captions, narrow enough for the columns
    value,   ///< Menlo - numbers, so digits line up
};

/** Resolves to the first of the preferred names actually installed, so a missing
    font degrades to a reasonable neighbour instead of silently falling back to the
    system default. */
juce::Font font (Face face, float height, bool bold = false);

//==============================================================================
class KrainLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    KrainLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;

    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KrainLookAndFeel)
};

} // namespace krain::gui
