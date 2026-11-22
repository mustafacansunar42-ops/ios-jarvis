#include "PluginEditor.h"
#include "Identity.h"
#include "UI/Theme.h"

using namespace automech;
using namespace juce;

namespace
{
// Fixed layout in design units (1000 x 830); the whole root is scaled.
const Rectangle<int> kDisplay      { 24, 66, 952, 190 };
const Rectangle<int> kKeyboardPanel{ 24, 338, 952, 168 };
const Rectangle<int> kKnobPanel    { 24, 518, 952, 146 };
const Rectangle<int> kTabStrip     { 24, 676, 952, 32 };
const Rectangle<int> kTabPanel     { 24, 714, 952, 102 };
const int kRowY = 272;

void drawWrench (Graphics& g, Rectangle<float> b, bool)
{
    const auto c = b.getCentre();
    Path p;
    p.startNewSubPath (c.x - 5.0f, c.y + 5.0f);
    p.lineTo (c.x + 2.0f, c.y - 2.0f);
    g.setColour (Colours::white);
    g.strokePath (p, PathStrokeType (2.4f, PathStrokeType::curved, PathStrokeType::rounded));
    Path head;
    head.addCentredArc (c.x + 3.5f, c.y - 3.5f, 4.0f, 4.0f, 0.0f, 0.9f, MathConstants<float>::twoPi - 0.9f, true);
    g.strokePath (head, PathStrokeType (2.0f));
}

void drawScaleIcon (Graphics& g, Rectangle<float> b, bool)
{
    const auto c = b.getCentre();
    g.setColour (Colours::white);
    g.drawEllipse (c.x - 9.0f, c.y - 5.0f, 10.0f, 10.0f, 1.6f);
    Path chevron;
    chevron.startNewSubPath (c.x + 4.0f, c.y - 1.5f);
    chevron.lineTo (c.x + 7.0f, c.y + 1.5f);
    chevron.lineTo (c.x + 10.0f, c.y - 1.5f);
    g.strokePath (chevron, PathStrokeType (1.4f));
}

void drawMixerIcon (Graphics& g, Rectangle<float> b, bool on)
{
    const auto c = b.getCentre();
    const auto colour = on ? theme::accentRed : Colours::white;
    for (int i = 0; i < 3; ++i)
    {
        const float x = c.x - 7.0f + 7.0f * (float) i;
        g.setColour (colour.withAlpha (0.6f));
        g.fillRect (x - 0.75f, c.y - 9.0f, 1.5f, 18.0f);
        g.setColour (colour);
        const float ky = c.y + (i == 0 ? 3.0f : (i == 1 ? -4.0f : 1.0f));
        g.fillRoundedRectangle (x - 3.0f, ky - 2.0f, 6.0f, 4.0f, 1.0f);
    }
}

void drawGear (Graphics& g, Point<float> c, float r)
{
    Path gear;
    const int teeth = 8;
    for (int i = 0; i < teeth * 2; ++i)
    {
        const float a0 = MathConstants<float>::twoPi * (float) i / (float) (teeth * 2);
        const float a1 = MathConstants<float>::twoPi * (float) (i + 1) / (float) (teeth * 2);
        const float rad = (i % 2 == 0) ? r : r * 0.78f;
        const auto p0 = c.getPointOnCircumference (rad, a0), p1 = c.getPointOnCircumference (rad, a1);
        if (i == 0) gear.startNewSubPath (p0); else gear.lineTo (p0);
        gear.lineTo (p1);
    }
    gear.closeSubPath();
    gear.addEllipse (c.x - r * 0.32f, c.y - r * 0.32f, r * 0.64f, r * 0.64f);
    gear.setUsingNonZeroWinding (false);
    g.setColour (theme::label.withAlpha (0.9f));
    g.fillPath (gear);
}
} // namespace

//==============================================================================
AutoMechEditor::AutoMechEditor (AutoMechProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    visScratch.resize (1 << 15);

    root.painter = [this] (Graphics& g) { paintRoot (g); };
    addAndMakeVisible (root);

    buildHeader();
    buildMainControls();
    buildTabs();

    display.getVisualizer().setSampleRate (processor.getCurrentSampleRate());

    setResizable (true, true);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kDesignWidth / (double) kDesignHeight);
    setResizeLimits (600, 498, 1600, 1328);
    setSize (kDesignWidth, kDesignHeight);

    startTimerHz (60);
}

AutoMechEditor::~AutoMechEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AutoMechEditor::resized()
{
    const float scale = (float) getWidth() / (float) kDesignWidth;
    root.setTransform (AffineTransform::scale (scale));
    root.setBounds (0, 0, kDesignWidth, kDesignHeight);
}

//==============================================================================
void AutoMechEditor::buildHeader()
{
    scaleButton = std::make_unique<IconButton> ("UI size", drawScaleIcon, true);
    scaleButton->onClick = [this] { showScaleMenu(); };
    root.addAndMakeVisible (*scaleButton);
    scaleButton->setBounds (606, 18, 40, 28);

    presetButton.onClick = [this] { showPresetMenu(); };
    root.addAndMakeVisible (presetButton);
    presetButton.setBounds (654, 18, 100, 28);

    abButton.onClick = [this] { processor.toggleAB(); };
    root.addAndMakeVisible (abButton);
    abButton.setBounds (762, 18, 52, 28);

    bypassButton.setClickingTogglesState (true);
    root.addAndMakeVisible (bypassButton);
    bypassButton.setBounds (822, 18, 80, 28);
    bypassAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ids::bypass, bypassButton);

    settingsButton = std::make_unique<IconButton> ("Settings", drawWrench);
    settingsButton->onClick = [this] { showSettingsMenu(); };
    root.addAndMakeVisible (*settingsButton);
    settingsButton->setBounds (910, 18, 34, 28);
}

void AutoMechEditor::buildMainControls()
{
    root.addAndMakeVisible (display);
    display.setBounds (kDisplay);

    auto& state = processor.apvts;

    keyCombo = std::make_unique<LabeledCombo> (state, ids::key, "Key", keyNames());
    root.addAndMakeVisible (*keyCombo);
    keyCombo->setBounds (24, kRowY, 150, 52);

    scaleCombo = std::make_unique<LabeledCombo> (state, ids::scale, "Scale / Chord", scaleNames());
    root.addAndMakeVisible (*scaleCombo);
    scaleCombo->setBounds (186, kRowY, 270, 52);

    midiButton = std::make_unique<IconButton> ("MIDI follow", drawMixerIcon);
    midiButton->setClickingTogglesState (true);
    midiButton->setTooltip ("Follow MIDI notes as the target pitch");
    root.addAndMakeVisible (*midiButton);
    midiButton->setBounds (468, kRowY + 16, 40, 34);
    midiAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (state, ids::midiFollow, *midiButton);

    learnButton.getProperties().set ("style", "learn");
    learnButton.onClick = [this] { processor.startLearn(); };
    root.addAndMakeVisible (learnButton);
    learnButton.setBounds (522, kRowY + 16, 262, 34);

    StringArray refs;
    for (int hz = 415; hz <= 466; ++hz) refs.add (String (hz) + " Hz");
    referenceCombo = std::make_unique<LabeledCombo> (state, ids::reference, "Reference", refs);
    root.addAndMakeVisible (*referenceCombo);
    referenceCombo->setBounds (798, kRowY, 178, 52);

    root.addAndMakeVisible (keyboard);
    keyboard.setBounds (kKeyboardPanel.reduced (12, 10));
    keyboard.onToggle = [this] (int pc) { toggleKeyboardNote (pc); };

    // Knob section
    const std::pair<const char*, const char*> bigDefs[] = { { ids::correction, "Correction" }, { ids::speed, "Speed" }, { ids::noteTrans, "Note Trans" } };
    const std::pair<const char*, const char*> smallDefs[] = { { ids::flex, "Flex" }, { ids::human, "Human" }, { ids::formant, "Formant" },
                                                          { ids::vibrato, "Vibrato" }, { ids::mix, "Mix" } };
    int x = kKnobPanel.getX() + 18;
    for (auto& [id, name] : bigDefs)
    {
        auto k = std::make_unique<LabeledKnob> (state, id, name, true);
        root.addAndMakeVisible (*k);
        k->setBounds (x, kKnobPanel.getY() + 14, LabeledKnob::preferredWidth (true), 120);
        x += LabeledKnob::preferredWidth (true) + 14;
        bigKnobs.push_back (std::move (k));
    }

    x = kKnobPanel.getX() + 440;
    for (auto& [id, name] : smallDefs)
    {
        auto k = std::make_unique<LabeledKnob> (state, id, name, false);
        root.addAndMakeVisible (*k);
        k->setBounds (x, kKnobPanel.getY() + 28, LabeledKnob::preferredWidth (false), 100);
        x += LabeledKnob::preferredWidth (false) + 22;
        smallKnobs.push_back (std::move (k));
    }
}

void AutoMechEditor::buildTabs()
{
    auto& state = processor.apvts;
    auto knob = [&state] (const char* id, const char* name) { return std::make_unique<LabeledKnob> (state, id, name, false); };
    auto combo = [&state] (const String& id, const String& name)
    {
        auto* choice = dynamic_cast<AudioParameterChoice*> (state.getParameter (id));
        return std::make_unique<LabeledCombo> (state, id, name, choice != nullptr ? choice->choices : StringArray());
    };
    auto toggle = [&state] (const char* id, const char* name) { return std::make_unique<LabeledToggle> (state, id, name); };
    auto hslider = [&state] (const char* id, const char* name) { return std::make_unique<LabeledHSlider> (state, id, name); };

    // CLEAN
    auto clean = std::make_unique<TabPage>();
    clean->add (toggle (ids::cleanOn, "Clean"), 70);
    clean->add (hslider (ids::cleanup, "Cleanup"), 150);
    clean->add (hslider (ids::lowCut, "Low Cut"), 150);
    clean->add (combo (ids::hum, "Hum"), 110);
    clean->addGap (10);
    clean->add (knob (ids::tighten, "Tighten"), 80);
    clean->add (knob (ids::onset, "Onset"), 80);
    clean->add (knob (ids::sensitivity, "Sensitivity"), 80);
    pages.push_back (std::move (clean));

    // VOICE
    auto voice = std::make_unique<TabPage>();
    voice->add (combo (ids::voiceType, "Voice Type"), 180);
    voice->add (toggle (ids::formantPreserve, "Formant Preserve"), 130);
    voice->add (knob (ids::formant, "Formant"), 80);
    voice->add (std::make_unique<InfoText> ("Pitch range", [this]
    {
        const auto r = voiceRange ((int) processor.apvts.getRawParameterValue (ids::voiceType)->load());
        return String ((int) r.first) + " - " + String ((int) r.second) + " Hz";
    }), 200);
    pages.push_back (std::move (voice));

    // VIBRATO
    auto vib = std::make_unique<TabPage>();
    vib->add (knob (ids::vibrato, "Depth"), 80);
    vib->add (knob (ids::vibRate, "Rate"), 80);
    vib->add (knob (ids::vibDelay, "Delay"), 80);
    vib->add (combo (ids::vibShape, "Shape"), 150);
    pages.push_back (std::move (vib));

    // HARMONY
    auto harm = std::make_unique<TabPage>();
    for (int h = 0; h < 3; ++h)
    {
        harm->add (combo (ids::harmInterval (h), "Voice " + String (h + 1)), 112);
        harm->add (std::make_unique<LabeledKnob> (state, ids::harmLevel (h), "Level", false), 66);
        harm->add (std::make_unique<LabeledKnob> (state, ids::harmPan (h), "Pan", false), 66);
    }
    pages.push_back (std::move (harm));

    // MECH
    auto mech = std::make_unique<TabPage>();
    mech->add (combo (ids::mechMode, "Mech Mode"), 150);
    mech->add (knob (ids::mechFreq, "Freq"), 80);
    mech->add (knob (ids::mechMix, "Mix"), 80);
    mech->add (std::make_unique<InfoText> ("Character", [this]
    {
        switch ((int) processor.apvts.getRawParameterValue (ids::mechMode)->load())
        {
            case 1:  return String ("Monotone robot on the key root");
            case 2:  return String ("Ring modulator (metallic)");
            case 3:  return String ("Short comb filter (tube voice)");
            default: return String ("Off");
        }
    }), 330);
    pages.push_back (std::move (mech));

    // FX
    auto fx = std::make_unique<TabPage>();
    fx->add (knob (ids::drive, "Drive"), 80);
    fx->add (knob (ids::width, "Width"), 80);
    pages.push_back (std::move (fx));

    // MIDI
    auto midi = std::make_unique<TabPage>();
    midi->add (toggle (ids::midiFollow, "Follow MIDI"), 110);
    midi->add (std::make_unique<InfoText> ("Held note", [this]
    {
        const int n = processor.telemetry.heldMidiNote.load();
        return n < 0 ? String ("No MIDI note") : midiNoteName (n);
    }), 180);
    midi->add (std::make_unique<InfoText> ("How it works", []
    {
        return String ("Held MIDI note sets the target pitch class");
    }), 400);
    pages.push_back (std::move (midi));

    for (auto& page : pages)
    {
        root.addChildComponent (*page);
        page->setBounds (kTabPanel.reduced (0, 0));
    }
    pages.front()->setVisible (true);

    root.addAndMakeVisible (tabs);
    tabs.setBounds (kTabStrip);
    tabs.onChange = [this] (int index)
    {
        for (size_t i = 0; i < pages.size(); ++i)
            pages[i]->setVisible ((int) i == index);
    };
}

//==============================================================================
void AutoMechEditor::paintRoot (Graphics& g)
{
    const auto bounds = Rectangle<float> (0.0f, 0.0f, (float) kDesignWidth, (float) kDesignHeight);

    // Red body with a soft glossy sweep
    g.setGradientFill (ColourGradient (theme::bgTop, 0.0f, 0.0f, theme::bgBottom, 0.0f, bounds.getBottom(), false));
    g.fillAll();
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.10f), 160.0f, -60.0f,
                                       Colours::white.withAlpha (0.0f), 700.0f, 420.0f, true));
    g.fillRect (bounds);

    // Header
    const auto wordFont = theme::font (27.0f, false, 0.38f);
    g.setFont (wordFont);
    g.setColour (Colour (0xffFFEDED));
    g.drawText (kPluginName, 28, 16, 260, 32, Justification::centredLeft);
    const float wordW = GlyphArrangement::getStringWidth (wordFont, kPluginName);
    g.setColour (theme::label.withAlpha (0.75f));
    g.setFont (theme::font (10.0f, true, 0.25f));
    g.drawText (String ("by ") + String (kDeveloperName).toUpperCase(), (int) (28.0f + wordW + 14.0f), 25, 260, 16, Justification::centredLeft);
    drawGear (g, { 962.0f, 32.0f }, 10.0f);

    // Label above the MIDI icon button and the LEARN button
    g.setColour (theme::label.withAlpha (0.85f));
    g.setFont (theme::font (9.5f, true, 0.14f));
    g.drawText ("MIDI", 468, kRowY, 60, 13, Justification::centredLeft);
    g.drawText ("AUTO-DETECT", 524, kRowY, 200, 13, Justification::centredLeft);

    // Recessed panels
    theme::drawRecessedPanel (g, kKeyboardPanel.toFloat());
    theme::drawRecessedPanel (g, kKnobPanel.toFloat());
    theme::drawRecessedPanel (g, kTabPanel.toFloat());

    // Divider between the large and small knob groups
    g.setColour (Colours::black.withAlpha (0.25f));
    g.fillRect ((float) kKnobPanel.getX() + 400.0f, (float) kKnobPanel.getY() + 18.0f, 1.0f, (float) kKnobPanel.getHeight() - 36.0f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.fillRect ((float) kKnobPanel.getX() + 401.0f, (float) kKnobPanel.getY() + 18.0f, 1.0f, (float) kKnobPanel.getHeight() - 36.0f);
}

//==============================================================================
void AutoMechEditor::pollProcessor()
{
    auto& vis = display.getVisualizer();
    vis.setSampleRate (processor.getCurrentSampleRate());
    const int n = processor.readVisualiserSamples (visScratch.data(), (int) visScratch.size());
    if (n > 0)
        vis.pushSamples (visScratch.data(), n);

    const auto& t = processor.telemetry;
    DisplayState s;
    s.detectedHz = t.detectedHz.load();
    s.detectedMidi = t.detectedMidi.load();
    s.outputMidi = t.outputMidi.load();
    s.targetNote = t.targetNote.load();
    s.voiced = t.voiced.load();
    s.locked = t.locked.load();
    s.mask = processor.currentScaleMask();
    display.update (s);

    const int detectedPc = s.voiced ? (((int) std::lround (s.detectedMidi) % 12) + 12) % 12 : -1;
    keyboard.setState (s.mask, detectedPc);

    int key = 0, scale = 0;
    if (processor.popLearnResult (key, scale))
    {
        processor.setParam (ids::key, (float) key);
        processor.setParam (ids::scale, (float) scale);
    }
    const String learnText = processor.isLearning() ? String::fromUTF8 ("LISTENING\xe2\x80\xa6") : String ("LEARN");
    if (learnButton.getButtonText() != learnText)
        learnButton.setButtonText (learnText);

    abButton.setButtonText (processor.getABSlot() == 0 ? "A/B" : "B/A");
    abButton.setToggleState (processor.getABSlot() == 1, dontSendNotification);

    if (auto index = tabs.getCurrent(); juce::isPositiveAndBelow (index, (int) pages.size()))
        pages[(size_t) index]->refreshInfo();
}

void AutoMechEditor::toggleKeyboardNote (int pc)
{
    auto mask = processor.currentScaleMask();
    mask = (uint16_t) (mask ^ (1u << (unsigned) pc));
    for (int i = 0; i < 12; ++i)
        processor.setParam (ids::noteEnabled (i), ((mask >> (unsigned) i) & 1u) ? 1.0f : 0.0f);
    processor.setParam (ids::scale, (float) scaleCustom);
}

//==============================================================================
void AutoMechEditor::showPresetMenu()
{
    PopupMenu menu;
    const auto& names = AutoMechProcessor::factoryPresetNames();
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i]);
    menu.addSeparator();
    menu.addItem (100, "Save preset...");
    menu.addItem (101, "Load preset...");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetButton), [this] (int result)
    {
        if (result >= 1 && result <= AutoMechProcessor::factoryPresetNames().size())
        {
            processor.setCurrentProgram (result - 1);
            return;
        }
        if (result != 100 && result != 101)
            return;

        const bool saving = result == 100;
        const auto dir = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("AutoMech Presets");
        dir.createDirectory();
        fileChooser = std::make_unique<FileChooser> (saving ? "Save AutoMech preset" : "Load AutoMech preset", dir, "*.amech");
        const int chooserFlags = saving ? FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting
                                 : FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync (chooserFlags, [this, saving] (const FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == File()) return;
            if (saving) processor.saveUserPreset (file.withFileExtension ("amech"));
            else        processor.loadUserPreset (file);
        });
    });
}

void AutoMechEditor::showScaleMenu()
{
    PopupMenu menu;
    const int sizes[] = { 75, 100, 125, 150 };
    for (int s : sizes)
        menu.addItem (s, "UI size " + String (s) + "%", true, std::abs (getWidth() - kDesignWidth * s / 100) < 4);

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (scaleButton.get()), [this] (int result)
    {
        if (result > 0)
            setSize (kDesignWidth * result / 100, kDesignHeight * result / 100);
    });
}

void AutoMechEditor::showSettingsMenu()
{
    PopupMenu menu;
    menu.addItem (1, "About AutoMech");
    menu.addItem (2, "Reset all parameters");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (settingsButton.get()), [this] (int result)
    {
        if (result == 1)
        {
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::NoIcon, "AutoMech",
                String ("AutoMech ") + kVersion + "\nby " + kDeveloperName
                + "\n\nReal-time vocal pitch correction with mechanical voice mode.", "OK", this);
        }
        else if (result == 2)
        {
            for (auto* p : processor.getParameters())
                p->setValueNotifyingHost (p->getDefaultValue());
        }
    });
}
