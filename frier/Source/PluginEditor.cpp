#include "PluginEditor.h"
#include "gui/Palette.h"

namespace frier
{
namespace layout
{
// Design-unit geometry (1000 x 640)
const juce::Rectangle<float> presetFrame  { 360.0f, 26.0f, 280.0f, 32.0f };
const juce::Rectangle<int>   display      { 70, 92, 860, 248 };
const juce::Rectangle<int>   modelRow     { 70, 352, 620, 32 };
const juce::Rectangle<float> panel        { 70.0f, 398.0f, 860.0f, 222.0f };

constexpr float knobCentreY = 508.0f;

// Section x-ranges inside the bottom panel
constexpr float satStart = 70.0f, satEnd = 440.0f;
constexpr float shapeEnd = 603.0f, filterEnd = 766.0f, outputEnd = 930.0f;
} // namespace layout

//==============================================================================
ChevronButton::ChevronButton (bool pointsLeft)
    : juce::Button (pointsLeft ? "Previous preset" : "Next preset"), left (pointsLeft)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ChevronButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto c = getLocalBounds().toFloat().getCentre().translated (0.0f, down ? 1.0f : 0.0f);
    const float w = 4.5f, h = 7.5f;

    juce::Path chevron;
    if (left)
    {
        chevron.startNewSubPath (c.x + w * 0.5f, c.y - h);
        chevron.lineTo (c.x - w * 0.5f, c.y);
        chevron.lineTo (c.x + w * 0.5f, c.y + h);
    }
    else
    {
        chevron.startNewSubPath (c.x - w * 0.5f, c.y - h);
        chevron.lineTo (c.x + w * 0.5f, c.y);
        chevron.lineTo (c.x - w * 0.5f, c.y + h);
    }

    g.setColour (highlighted ? palette::goldBright : palette::gold);
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
}

//==============================================================================
MainView::MainView (FrierAudioProcessor& p)
    : processor (p),
      visualizer (p),
      modelSelector (param (params::id::model)),
      drive   (param (params::id::drive),   "Drive",    Knob::Size::Large),
      tone    (param (params::id::tone),    "Tone",     Knob::Size::Large),
      heat    (param (params::id::heat),    "Heat",     Knob::Size::Large),
      crunch  (param (params::id::crunch),  "Crunch",   Knob::Size::Small),
      bias    (param (params::id::bias),    "Bias",     Knob::Size::Small),
      lowCut  (param (params::id::lowCut),  "Low Cut",  Knob::Size::Small),
      highCut (param (params::id::highCut), "High Cut", Knob::Size::Small),
      mix     (param (params::id::mix),     "Mix",      Knob::Size::Small),
      output  (param (params::id::output),  "Output",   Knob::Size::Small),
      inputMeter  (p.getMeters().input,  "IN"),
      outputMeter (p.getMeters().output, "OUT"),
      bypassAttachment   (p.apvts, params::id::bypass,   bypassButton),
      autoGainAttachment (p.apvts, params::id::autoGain, autoGainButton)
{
    // Preset bar
    presetBox.getProperties().set (FrierLookAndFeel::flatComboProperty, true);
    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);

    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0)
            processor.setCurrentProgram (index);
    };

    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };

    // A / B
    aButton.onClick = [this] { processor.selectABSlot (0); syncHeader(); };
    bButton.onClick = [this] { processor.selectABSlot (1); syncHeader(); };
    aButton.setTooltip ("Compare: settings A");
    bButton.setTooltip ("Compare: settings B");

    bypassButton.setColour (juce::ToggleButton::tickColourId, palette::hot);
    autoGainButton.setColour (juce::ToggleButton::tickColourId, palette::gold);

    // Oversampling
    oversamplingBox.addItemList (params::oversamplingNames, 1);
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, params::id::oversampling, oversamplingBox);

    for (auto* c : std::initializer_list<juce::Component*> {
             &presetBox, &prevButton, &nextButton, &aButton, &bButton, &bypassButton,
             &visualizer, &modelSelector, &oversamplingBox,
             &drive, &tone, &heat, &crunch, &bias, &lowCut, &highCut, &mix, &output,
             &autoGainButton, &inputMeter, &outputMeter })
        addAndMakeVisible (c);

    syncHeader();
    startTimerHz (10);
    setSize (designWidth, designHeight);
}

juce::RangedAudioParameter& MainView::param (const char* id)
{
    auto* parameter = processor.apvts.getParameter (id);
    jassert (parameter != nullptr);
    return *parameter;
}

void MainView::stepPreset (int delta)
{
    const int count = processor.getNumPrograms();
    const int index = (processor.getCurrentProgram() + delta + count) % count;
    processor.setCurrentProgram (index);
    syncHeader();
}

void MainView::syncHeader()
{
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    aButton.setToggleState (processor.getABSlot() == 0, juce::dontSendNotification);
    bButton.setToggleState (processor.getABSlot() == 1, juce::dontSendNotification);
}

void MainView::timerCallback()
{
    // Keeps the header in step with host-side program changes and state restores.
    syncHeader();
}

//==============================================================================
void MainView::resized()
{
    using namespace layout;

    // Header
    presetBox.setBounds (presetFrame.toNearestInt().withTrimmedLeft (34).withTrimmedRight (34).reduced (0, 2));
    prevButton.setBounds ((int) presetFrame.getX() + 4, (int) presetFrame.getY() + 2, 28, 28);
    nextButton.setBounds ((int) presetFrame.getRight() - 32, (int) presetFrame.getY() + 2, 28, 28);

    aButton.setBounds (742, 28, 30, 28);
    bButton.setBounds (776, 28, 30, 28);
    bypassButton.setBounds (822, 28, 108, 28);

    // Meters (left: input, right: output)
    inputMeter.setBounds (20, 92, 34, 528);
    outputMeter.setBounds (946, 92, 34, 528);

    // Centre
    visualizer.setBounds (display);
    modelSelector.setBounds (modelRow);
    oversamplingBox.setBounds (850, 354, 80, 28);

    // Knobs
    auto placeKnob = [] (Knob& k, float centreX, Knob::Size size, int width)
    {
        const int h = Knob::preferredHeight (size);
        const int top = juce::roundToInt (knobCentreY - (float) (16 + 6) - (float) Knob::knobDiameter (size) * 0.5f);
        k.setBounds (juce::roundToInt (centreX - (float) width * 0.5f), top, width, h);
    };

    const float satWidth = satEnd - satStart;
    placeKnob (drive, satStart + satWidth * (1.0f / 6.0f), Knob::Size::Large, 116);
    placeKnob (tone,  satStart + satWidth * (3.0f / 6.0f), Knob::Size::Large, 116);
    placeKnob (heat,  satStart + satWidth * (5.0f / 6.0f), Knob::Size::Large, 116);

    auto placePair = [&] (Knob& a, Knob& b, float start, float end)
    {
        const float w = end - start;
        placeKnob (a, start + w * 0.28f, Knob::Size::Small, 80);
        placeKnob (b, start + w * 0.72f, Knob::Size::Small, 80);
    };

    placePair (crunch, bias,    satEnd,    shapeEnd);
    placePair (lowCut, highCut, shapeEnd,  filterEnd);
    placePair (mix,    output,  filterEnd, outputEnd);

    autoGainButton.setBounds ((int) outputEnd - 70, (int) panel.getY() + 10, 62, 18);
}

//==============================================================================
void MainView::paint (juce::Graphics& g)
{
    using namespace layout;
    const auto bounds = getLocalBounds().toFloat();

    // Background: near-black with a warm glow, like light bleeding around an eclipse.
    g.fillAll (palette::background);
    g.setGradientFill (juce::ColourGradient (palette::glow.withAlpha (0.55f), 640.0f, 210.0f,
                                             palette::background.withAlpha (0.0f), 640.0f + 560.0f, 210.0f, true));
    g.fillRect (bounds);
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0.0f, 380.0f,
                                             juce::Colours::black.withAlpha (0.45f), 0.0f, 640.0f, false));
    g.fillRect (bounds);

    // --- Header ---------------------------------------------------------------
    {
        // Eclipse mark
        const juce::Point<float> c { 38.0f, 42.0f };
        for (int i = 6; i >= 1; --i)
        {
            g.setColour (palette::goldBright.withAlpha (0.045f * (float) (7 - i)));
            g.fillEllipse (juce::Rectangle<float> (26.0f + (float) i * 2.4f, 26.0f + (float) i * 2.4f).withCentre (c));
        }
        g.setColour (palette::goldBright);
        g.drawEllipse (juce::Rectangle<float> (25.0f, 25.0f).withCentre (c), 1.4f);
        g.setColour (juce::Colours::black);
        g.fillEllipse (juce::Rectangle<float> (23.5f, 23.5f).withCentre (c.translated (-0.6f, 0.4f)));

        g.setColour (palette::cream);
        g.setFont (FrierLookAndFeel::font (28.0f, 0.42f));
        g.drawText (identity::pluginName, juce::Rectangle<float> (64.0f, 16.0f, 220.0f, 30.0f), juce::Justification::centredLeft, false);

        g.setColour (palette::textDim);
        g.setFont (FrierLookAndFeel::font (10.0f, 0.38f));
        g.drawText (juce::String (identity::manufacturer).toUpperCase(),
                    juce::Rectangle<float> (65.0f, 46.0f, 260.0f, 14.0f), juce::Justification::centredLeft, false);
        g.setColour (palette::textFaint.brighter (0.3f));
        g.setFont (FrierLookAndFeel::font (11.0f, 0.04f));
        g.drawText (identity::tagline, juce::Rectangle<float> (65.0f, 60.0f, 280.0f, 14.0f), juce::Justification::centredLeft, false);

        // Preset frame
        g.setColour (palette::textDim);
        g.setFont (FrierLookAndFeel::font (10.0f, 0.3f));
        g.drawText ("PRESET", presetFrame.withY (10.0f).withHeight (14.0f), juce::Justification::centred, false);

        g.setColour (palette::panel.withAlpha (0.85f));
        g.fillRoundedRectangle (presetFrame, 4.0f);
        g.setColour (palette::panelBorder);
        g.drawRoundedRectangle (presetFrame.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (palette::divider);
        g.drawVerticalLine ((int) presetFrame.getX() + 34, presetFrame.getY() + 6.0f, presetFrame.getBottom() - 6.0f);
        g.drawVerticalLine ((int) presetFrame.getRight() - 35, presetFrame.getY() + 6.0f, presetFrame.getBottom() - 6.0f);

        g.setColour (palette::textDim);
        g.drawText ("A / B", juce::Rectangle<float> (742.0f, 10.0f, 64.0f, 14.0f), juce::Justification::centred, false);
    }

    // --- Display frame ----------------------------------------------------------
    g.setColour (palette::panelBorder);
    g.drawRoundedRectangle (display.toFloat().expanded (1.5f), 7.0f, 1.0f);

    // --- Model row --------------------------------------------------------------
    g.setColour (palette::textDim);
    g.setFont (FrierLookAndFeel::font (10.0f, 0.28f));
    g.drawText ("OVERSAMPLING", juce::Rectangle<float> (700.0f, 354.0f, 142.0f, 28.0f), juce::Justification::centredRight, false);

    // --- Bottom panel -----------------------------------------------------------
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (panel.translated (0.0f, 3.0f).expanded (2.0f), 10.0f);
    g.setColour (palette::panel.withAlpha (0.92f));
    g.fillRoundedRectangle (panel, 9.0f);
    g.setColour (palette::panelBorder);
    g.drawRoundedRectangle (panel.reduced (0.5f), 9.0f, 1.0f);

    g.setColour (palette::divider);
    for (const float x : { satEnd, shapeEnd, filterEnd })
        g.drawVerticalLine (juce::roundToInt (x), panel.getY() + 16.0f, panel.getBottom() - 16.0f);

    g.setFont (FrierLookAndFeel::font (11.0f, 0.34f));
    auto sectionTitle = [&] (const juce::String& text, float start, float end)
    {
        g.setColour (palette::textDim.brighter (0.15f));
        g.drawText (text, juce::Rectangle<float> (start, panel.getY() + 10.0f, end - start, 18.0f), juce::Justification::centred, false);
    };

    sectionTitle ("SATURATION", satStart, satEnd);
    sectionTitle ("SHAPE",      satEnd,   shapeEnd);
    sectionTitle ("FILTER",     shapeEnd, filterEnd);
    sectionTitle ("OUTPUT",     filterEnd, outputEnd - 64.0f);
}
} // namespace frier

//==============================================================================
FrierAudioProcessorEditor::FrierAudioProcessorEditor (FrierAudioProcessor& p)
    : AudioProcessorEditor (p), view (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (view);

    using MV = frier::MainView;
    setResizable (true, true);
    setResizeLimits (MV::designWidth * 6 / 10, MV::designHeight * 6 / 10,
                     MV::designWidth * 2,      MV::designHeight * 2);
    if (auto* aspect = getConstrainer())
        aspect->setFixedAspectRatio ((double) MV::designWidth / (double) MV::designHeight);

    setSize (MV::designWidth, MV::designHeight);
}

FrierAudioProcessorEditor::~FrierAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void FrierAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (frier::palette::background);
}

void FrierAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) frier::MainView::designWidth;
    view.setBounds (0, 0, frier::MainView::designWidth, frier::MainView::designHeight);
    view.setTransform (juce::AffineTransform::scale (scale));
}
