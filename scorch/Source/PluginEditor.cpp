#include "PluginEditor.h"
#include "gui/Palette.h"

namespace scorch
{
namespace layout
{
// Design-unit geometry (760 x 920)
const juce::Rectangle<int> display { 152, 84, 456, 456 };
constexpr int row1Top = 650;
constexpr int row2Top = 760;
const std::array<int, 5> row1Centres { 140, 260, 380, 500, 620 };
const std::array<int, 4> row2Centres { 200, 320, 440, 560 };
const juce::Rectangle<float> meterStrip { 24.0f, 884.0f, 712.0f, 28.0f };
} // namespace layout

//==============================================================================
ChevronButton::ChevronButton (bool pointsLeft)
    : juce::Button (pointsLeft ? "Previous preset" : "Next preset"), left (pointsLeft)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ChevronButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (palette::panel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (highlighted ? palette::labelDim : palette::outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    const auto c = r.getCentre().translated (0.0f, down ? 1.0f : 0.0f);
    const float w = 3.5f, h = 5.5f;

    juce::Path chevron;
    chevron.startNewSubPath (c.x + (left ? w : -w) * 0.5f, c.y - h);
    chevron.lineTo (c.x + (left ? -w : w) * 0.5f, c.y);
    chevron.lineTo (c.x + (left ? w : -w) * 0.5f, c.y + h);

    g.setColour (highlighted ? palette::highlight.brighter (0.3f) : palette::highlight);
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
}

//==============================================================================
PixelTagButton::PixelTagButton (PixelVisualizer& v)
    : juce::Button ("Visualizer mode"), visualizer (v)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    onClick = [this] { visualizer.cycleMode(); };
}

void PixelTagButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto tag = juce::Rectangle<float> (46.0f, bounds.getHeight() - 2.0f).withX (1.0f).withY (1.0f);

    g.setColour (palette::accent.withAlpha (highlighted ? 0.3f : 0.15f));
    g.fillRoundedRectangle (tag, 3.0f);
    g.setColour (palette::accent);
    g.drawRoundedRectangle (tag, 3.0f, 1.0f);
    g.setColour (palette::highlight.brighter (0.2f));
    g.setFont (ScorchLookAndFeel::font (10.0f, 0.3f));
    g.drawText ("PIXEL", tag, juce::Justification::centred, false);

    // three mode squares, current one lit
    const int current = (int) visualizer.getMode();
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (i == current ? palette::highlight : palette::outline.brighter (0.2f));
        g.fillRect (juce::Rectangle<float> (tag.getRight() + 8.0f + (float) i * 8.0f, bounds.getCentreY() - 2.5f, 5.0f, 5.0f));
    }

    g.setColour (highlighted ? palette::value : palette::label);
    g.setFont (ScorchLookAndFeel::font (11.5f, 0.05f));
    g.drawText (visualizer.getModeName(), bounds.withTrimmedLeft (tag.getRight() + 38.0f),
                juce::Justification::centredLeft, false);
}

//==============================================================================
MainView::MainView (ScorchAudioProcessor& p)
    : processor (p),
      visualizer (p),
      drive   (param (params::id::drive),   "drive"),
      tone    (param (params::id::tone),    "tone"),
      heat    (param (params::id::heat),    "heat"),
      crunch  (param (params::id::crunch),  "crunch"),
      bias    (param (params::id::bias),    "bias"),
      lowCut  (param (params::id::lowCut),  "low cut"),
      highCut (param (params::id::highCut), "high cut"),
      mix     (param (params::id::mix),     "mix"),
      output  (param (params::id::output),  "output"),
      inputMeter  (p.getMeters().input,  "in"),
      outputMeter (p.getMeters().output, "out"),
      bypassAttachment   (p.apvts, params::id::bypass,   bypassButton),
      autoGainAttachment (p.apvts, params::id::autoGain, autoGainButton)
{
    // MODEL dropdown (choice parameter)
    for (int i = 0; i < params::modelNames.size(); ++i)
        modelBox.addItem (params::modelNames[i].toUpperCase(), i + 1);
    modelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, params::id::model, modelBox);

    // PRESET dropdown
    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0 && index != processor.getCurrentProgram())
            processor.setCurrentProgram (index);
    };

    // Oversampling
    oversamplingBox.addItemList (params::oversamplingNames, 1);
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, params::id::oversampling, oversamplingBox);

    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };

    bypassButton.setColour (juce::ToggleButton::tickColourId, palette::hot);
    autoGainButton.setColour (juce::ToggleButton::tickColourId, palette::accent);

    visualizer.onModeChanged = [this] { pixelTag.repaint(); };

    for (auto* c : std::initializer_list<juce::Component*> {
             &prevButton, &nextButton, &bypassButton,
             &visualizer, &pixelTag,
             &modelBox, &presetBox, &oversamplingBox,
             &drive, &tone, &heat, &crunch, &bias, &lowCut, &highCut, &mix, &output,
             &autoGainButton, &inputMeter, &outputMeter })
        addAndMakeVisible (c);

    syncPreset();
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
    processor.setCurrentProgram ((processor.getCurrentProgram() + delta + count) % count);
    syncPreset();
}

void MainView::syncPreset()
{
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
}

void MainView::timerCallback()
{
    // keeps the preset field in step with host-side program changes and state restores
    syncPreset();
}

//==============================================================================
void MainView::resized()
{
    using namespace layout;

    // header (top right)
    prevButton.setBounds (574, 18, 26, 26);
    nextButton.setBounds (604, 18, 26, 26);
    bypassButton.setBounds (640, 18, 96, 26);

    // display + PIXEL tag
    visualizer.setBounds (display);
    pixelTag.setBounds (display.getCentreX() - 64, display.getBottom() + 14, 150, 20);

    // dropdowns
    modelBox.setBounds (150, 606, 220, 32);
    presetBox.setBounds (390, 606, 220, 32);

    // knob rows
    auto place = [] (Knob& k, int centreX, int top)
    {
        k.setBounds (centreX - Knob::preferredWidth / 2, top, Knob::preferredWidth, Knob::preferredHeight);
    };

    std::array<Knob*, 5> row1 { &drive, &tone, &heat, &crunch, &bias };
    std::array<Knob*, 4> row2 { &lowCut, &highCut, &mix, &output };
    for (size_t i = 0; i < row1.size(); ++i) place (*row1[i], row1Centres[i], row1Top);
    for (size_t i = 0; i < row2.size(); ++i) place (*row2[i], row2Centres[i], row2Top);

    // second-row side controls, aligned with the knob centres
    const int row2Centre = row2Top + Knob::bubbleHeight + 4 + Knob::knobSize / 2;
    oversamplingBox.setBounds (34, row2Centre - 13, 92, 26);
    autoGainButton.setBounds (634, row2Centre - 13, 96, 26);

    // footer meters
    inputMeter.setBounds (meterStrip.toNearestInt().withTrimmedLeft (16).withWidth (330).reduced (0, 5));
    outputMeter.setBounds (meterStrip.toNearestInt().withTrimmedLeft (366).withWidth (330).reduced (0, 5));
}

//==============================================================================
void MainView::paint (juce::Graphics& g)
{
    using namespace layout;
    const auto bounds = getLocalBounds().toFloat();
    const auto displayCentre = display.toFloat().getCentre();
    const float displayRadius = (float) display.getWidth() * 0.5f;

    // Background: near-black, a soft dark-orange spotlight behind the display, vignette.
    g.fillAll (palette::background);
    g.setGradientFill (juce::ColourGradient (palette::glow, displayCentre,
                                             palette::background.withAlpha (0.0f), displayCentre.translated (430.0f, 0.0f), true));
    g.fillRect (bounds);
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, bounds.getCentre(),
                                             juce::Colours::black.withAlpha (0.65f), bounds.getTopLeft(), true));
    g.fillRect (bounds);

    // --- Header ----------------------------------------------------------------
    g.setColour (palette::label);
    g.setFont (ScorchLookAndFeel::font (10.0f, 0.38f));
    g.drawText (juce::String (identity::manufacturer).toUpperCase(),
                juce::Rectangle<float> (24.0f, 18.0f, 280.0f, 14.0f), juce::Justification::centredLeft, false);
    g.setColour (palette::labelDim);
    g.drawText ("SATURATION // DISTORTION", juce::Rectangle<float> (24.0f, 32.0f, 280.0f, 14.0f),
                juce::Justification::centredLeft, false);

    const auto wordmark = juce::Rectangle<float> (230.0f, 14.0f, 300.0f, 36.0f);
    g.setFont (ScorchLookAndFeel::font (30.0f, 0.42f));
    g.setColour (palette::accent.withAlpha (0.35f));
    g.drawText (identity::pluginName, wordmark.translated (0.0f, 1.5f), juce::Justification::centred, false);
    g.setColour (palette::value);
    g.drawText (identity::pluginName, wordmark, juce::Justification::centred, false);
    g.setColour (palette::accent);
    g.fillRect (juce::Rectangle<float> (40.0f, 1.5f).withCentre ({ wordmark.getCentreX(), wordmark.getBottom() + 4.0f }));

    g.setColour (palette::labelDim);
    g.setFont (ScorchLookAndFeel::font (10.0f, 0.06f));
    g.drawText ("preset", juce::Rectangle<float> (574.0f, 46.0f, 56.0f, 14.0f), juce::Justification::centred, false);

    // --- Circular display rim ------------------------------------------------------
    {
        const auto disc = juce::Rectangle<float> (displayRadius * 2.0f, displayRadius * 2.0f).withCentre (displayCentre);

        // soft outer shadow / glow
        g.setGradientFill (juce::ColourGradient (palette::accent.withAlpha (0.18f), displayCentre,
                                                 juce::Colours::transparentBlack, displayCentre.translated (displayRadius + 40.0f, 0.0f), true));
        g.fillEllipse (disc.expanded (40.0f));
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.fillEllipse (disc.expanded (9.0f).translated (0.0f, 4.0f));

        // metallic dark-orange rim: bright on top, deep at the bottom
        g.setGradientFill (juce::ColourGradient (palette::highlight, displayCentre.x, disc.getY() - 8.0f,
                                                 palette::deep.darker (0.4f), displayCentre.x, disc.getBottom() + 8.0f, false));
        g.drawEllipse (disc.expanded (4.5f), 5.0f);

        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawEllipse (disc.expanded (6.5f), 0.8f);
        g.setColour (juce::Colours::black);
        g.drawEllipse (disc.expanded (1.5f), 1.6f);
    }

    // --- Dropdown labels -----------------------------------------------------------
    g.setColour (palette::label);
    g.setFont (ScorchLookAndFeel::font (11.0f, 0.06f));
    g.drawText ("model",  juce::Rectangle<float> (150.0f, 588.0f, 220.0f, 14.0f), juce::Justification::centred, false);
    g.drawText ("preset", juce::Rectangle<float> (390.0f, 588.0f, 220.0f, 14.0f), juce::Justification::centred, false);

    const float row2Label = (float) (row2Top + Knob::bubbleHeight + 4 + Knob::knobSize / 2) - 30.0f;
    g.drawText ("oversampling", juce::Rectangle<float> (24.0f, row2Label, 112.0f, 14.0f), juce::Justification::centred, false);

    // --- Footer --------------------------------------------------------------------
    g.setColour (palette::labelDim);
    g.setFont (ScorchLookAndFeel::font (10.0f, 0.05f));
    g.drawText ("drag to adjust  |  shift = fine  |  double-click = reset  |  click the display to change view",
                juce::Rectangle<float> (0.0f, 866.0f, (float) designWidth, 14.0f), juce::Justification::centred, false);

    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (meterStrip, 5.0f);
    g.setColour (palette::outline);
    g.drawRoundedRectangle (meterStrip.reduced (0.5f), 5.0f, 1.0f);
    g.drawVerticalLine (juce::roundToInt (meterStrip.getCentreX()), meterStrip.getY() + 6.0f, meterStrip.getBottom() - 6.0f);
}
} // namespace scorch

//==============================================================================
ScorchAudioProcessorEditor::ScorchAudioProcessorEditor (ScorchAudioProcessor& p)
    : AudioProcessorEditor (p), view (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (view);

    using MV = scorch::MainView;
    setResizable (true, true);
    setResizeLimits (MV::designWidth * 6 / 10, MV::designHeight * 6 / 10,
                     MV::designWidth * 2,      MV::designHeight * 2);
    if (auto* aspect = getConstrainer())
        aspect->setFixedAspectRatio ((double) MV::designWidth / (double) MV::designHeight);

    setSize (MV::designWidth, MV::designHeight);
}

ScorchAudioProcessorEditor::~ScorchAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ScorchAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (scorch::palette::background);
}

void ScorchAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) scorch::MainView::designWidth;
    view.setBounds (0, 0, scorch::MainView::designWidth, scorch::MainView::designHeight);
    view.setTransform (juce::AffineTransform::scale (scale));
}
