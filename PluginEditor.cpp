#include "PluginEditor.h"

namespace
{
    const juce::Colour kAmber   (0xffffb000);
    const juce::Colour kCream   (0xffe6e0d0);
    const juce::Colour kPanel   (0xff09090a);
    const juce::Colour kOutline (0xff2b2b2f);

    constexpr int kWindowW = 1180;
    constexpr int kWindowH = 744;
    constexpr int kCheekW  = 46;
    constexpr int kCellW   = 76;
    constexpr int kCellH   = 106;
}

//==============================================================================
PhatLookAndFeel::PhatLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0d0d0f));
    setColour (juce::ComboBox::textColourId, kAmber);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3a3e));
    setColour (juce::ComboBox::arrowColourId, kCream);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff111113));
    setColour (juce::PopupMenu::textColourId, kCream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAmber);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff18181a));
    setColour (juce::TextButton::buttonOnColourId, kAmber);
    setColour (juce::TextButton::textColourOffId, kCream);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
}

void PhatLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float pos, float startAngle, float endAngle, juce::Slider&)
{
    const float cx = (float) x + (float) width * 0.5f;
    const float cy = (float) y + (float) height * 0.5f;
    const float r = std::min ((float) width, (float) height) * 0.40f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // tick marks around the knob
    g.setColour (juce::Colour (0xff8d8778));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const float s = std::sin (a), c = std::cos (a);
        g.drawLine (cx + s * (r + 4.0f), cy - c * (r + 4.0f),
                    cx + s * (r + 8.0f), cy - c * (r + 8.0f),
                    (i == 0 || i == 10) ? 1.6f : 1.0f);
    }

    // drop shadow
    g.setColour (juce::Colour (0x66000000));
    g.fillEllipse (cx - r + 1.5f, cy - r + 3.0f, r * 2.0f, r * 2.0f);

    // knob body
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a4e), cx - r * 0.5f, cy - r * 0.7f,
                                             juce::Colour (0xff0a0a0b), cx + r * 0.7f, cy + r * 0.9f, false));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colour (0xff1c1c1f));
    g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.5f);

    // top cap
    const float rc = r * 0.78f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2a2d), cx - rc * 0.4f, cy - rc * 0.6f,
                                             juce::Colour (0xff050506), cx + rc * 0.6f, cy + rc * 0.8f, false));
    g.fillEllipse (cx - rc, cy - rc, rc * 2.0f, rc * 2.0f);

    // white pointer line
    juce::Path pointer;
    pointer.addRectangle (-1.6f, -r * 0.96f, 3.2f, r * 0.5f);
    g.setColour (juce::Colour (0xfff7f4ea));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (cx, cy));
}

//==============================================================================
int PhatTrackAudioProcessorEditor::addKnob (const char* id, const char* labelText, const char* suffix, int decimals)
{
    auto c = std::make_unique<Control>();
    c->kind = Kind::knob;

    c->label.setText (labelText, juce::dontSendNotification);
    c->label.setJustificationType (juce::Justification::centred);
    c->label.setFont (juce::Font (9.5f, juce::Font::bold));
    c->label.setColour (juce::Label::textColourId, kCream);
    addAndMakeVisible (c->label);

    c->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    c->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
    c->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                   juce::MathConstants<float>::pi * 2.8f, true);
    c->slider.setColour (juce::Slider::textBoxTextColourId, kAmber);
    c->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff050806));
    c->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff2a2a2d));
    addAndMakeVisible (c->slider);

    if (auto* prm = audioProcessor.parameters.getParameter (id))
        c->slider.setDoubleClickReturnValue (true, (double) prm->convertFrom0to1 (prm->getDefaultValue()));

    c->sliderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.parameters, id, c->slider);

    // Show units next to the value (set after the attachment, which installs its own text functions)
    const juce::String unit (suffix);
    c->slider.textFromValueFunction = [unit, decimals] (double v) { return juce::String (v, decimals) + unit; };
    c->slider.valueFromTextFunction = [] (const juce::String& t) { return (double) t.getFloatValue(); };
    c->slider.updateText();

    controls.push_back (std::move (c));
    return (int) controls.size() - 1;
}

int PhatTrackAudioProcessorEditor::addCombo (const char* id, const char* labelText)
{
    auto c = std::make_unique<Control>();
    c->kind = Kind::combo;

    c->label.setText (labelText, juce::dontSendNotification);
    c->label.setJustificationType (juce::Justification::centred);
    c->label.setFont (juce::Font (9.5f, juce::Font::bold));
    c->label.setColour (juce::Label::textColourId, kCream);
    addAndMakeVisible (c->label);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (audioProcessor.parameters.getParameter (id)))
        c->combo.addItemList (choice->choices, 1);
    addAndMakeVisible (c->combo);

    c->comboAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.parameters, id, c->combo);

    controls.push_back (std::move (c));
    return (int) controls.size() - 1;
}

int PhatTrackAudioProcessorEditor::addToggle (const char* id, const char* text)
{
    auto c = std::make_unique<Control>();
    c->kind = Kind::toggle;

    c->button.setButtonText (text);
    c->button.setClickingTogglesState (true);
    addAndMakeVisible (c->button);

    c->buttonAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.parameters, id, c->button);

    controls.push_back (std::move (c));
    return (int) controls.size() - 1;
}

void PhatTrackAudioProcessorEditor::addSection (const char* title, int x, int y, std::initializer_list<int> items)
{
    Section s;
    s.title = title;
    s.x = x;
    s.y = y;
    s.items = std::vector<int> (items);
    sections.push_back (std::move (s));
}

bool PhatTrackAudioProcessorEditor::isBlackKey (int midi)
{
    const int pc = midi % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

//==============================================================================
PhatTrackAudioProcessorEditor::PhatTrackAudioProcessorEditor (PhatTrackAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    // ---- oscillators ----
    const int o1Oct  = addCombo  (ids::o1Oct,  "OCTAVE");
    const int o1Wave = addCombo  (ids::o1Wave, "WAVE");
    const int o1Semi = addKnob   (ids::o1Semi, "SEMITONE", " st", 0);
    const int o1Det  = addKnob   (ids::o1Det,  "DETUNE", " ct", 1);
    const int o1Lvl  = addKnob   (ids::o1Lvl,  "LEVEL", "", 2);

    const int o2Oct  = addCombo  (ids::o2Oct,  "OCTAVE");
    const int o2Wave = addCombo  (ids::o2Wave, "WAVE");
    const int o2Semi = addKnob   (ids::o2Semi, "SEMITONE", " st", 0);
    const int o2Det  = addKnob   (ids::o2Det,  "DETUNE", " ct", 1);
    const int o2Lvl  = addKnob   (ids::o2Lvl,  "LEVEL", "", 2);

    const int sub    = addKnob   (ids::sub,    "SUB", "", 2);
    const int noise  = addKnob   (ids::noise,  "NOISE", "", 2);
    const int sync   = addToggle (ids::sync,   "SYNC");

    // ---- filter and envelopes ----
    const int cutoff = addKnob (ids::cutoff,  "CUTOFF", " Hz", 0);
    const int res    = addKnob (ids::res,     "RESONANCE", "", 2);
    const int keyTrk = addKnob (ids::keyTrk,  "KEY TRACK", "", 2);
    const int envAmt = addKnob (ids::fEnvAmt, "ENV AMOUNT", "", 2);
    const int drive  = addKnob (ids::drive,   "DRIVE", "", 2);

    const int fA = addKnob (ids::fA, "ATTACK",  " s", 3);
    const int fD = addKnob (ids::fD, "DECAY",   " s", 3);
    const int fS = addKnob (ids::fS, "SUSTAIN", "", 2);
    const int fR = addKnob (ids::fR, "RELEASE", " s", 3);

    const int aA = addKnob (ids::aA, "ATTACK",  " s", 3);
    const int aD = addKnob (ids::aD, "DECAY",   " s", 3);
    const int aS = addKnob (ids::aS, "SUSTAIN", "", 2);
    const int aR = addKnob (ids::aR, "RELEASE", " s", 3);

    // ---- modulation, master, input ----
    const int lfoRate = addKnob  (ids::lfoRate, "RATE", " Hz", 2);
    const int modSrc  = addCombo (ids::modSrc,  "SOURCE");
    const int modDst  = addCombo (ids::modDst,  "DEST");
    const int modAmt  = addKnob  (ids::modAmt,  "AMOUNT", "", 2);

    const int octave  = addCombo  (ids::octave,  "OCTAVE");
    const int tune    = addKnob   (ids::tune,    "FINE TUNE", " ct", 1);
    const int glide   = addKnob   (ids::glide,   "GLIDE", " s", 2);
    const int glideOn = addToggle (ids::glideOn, "GLIDE");
    const int volume  = addKnob   (ids::volume,  "VOLUME", "", 2);

    const int gate    = addKnob   (ids::gate,   "NOISE GATE", " dB", 1);
    const int sens    = addKnob   (ids::sens,   "SENSITIVITY", " dB", 1);
    const int dry     = addKnob   (ids::dry,    "DRY MIX", "", 2);
    const int retrig  = addToggle (ids::retrig, "RETRIG");

    // ---- panel sections (classic left-to-right signal flow) ----
    const int rowA = 96, rowB = 262, rowC = 428;
    addSection ("OSCILLATOR 1",     62, rowA, { o1Oct, o1Wave, o1Semi, o1Det, o1Lvl });
    addSection ("OSCILLATOR 2",    466, rowA, { o2Oct, o2Wave, o2Semi, o2Det, o2Lvl });
    addSection ("MIXER",           870, rowA, { sub, noise, sync });
    addSection ("FILTER",           62, rowB, { cutoff, res, keyTrk, envAmt, drive });
    addSection ("FILTER ENVELOPE", 466, rowB, { fA, fD, fS, fR });
    addSection ("AMP ENVELOPE",    794, rowB, { aA, aD, aS, aR });
    addSection ("MODULATION",       62, rowC, { lfoRate, modSrc, modDst, modAmt });
    addSection ("MASTER",          390, rowC, { octave, tune, glide, glideOn, volume });
    addSection ("INPUT",           794, rowC, { gate, sens, dry, retrig });

    // ---- readouts ----
    noteLabel.setJustificationType (juce::Justification::centredLeft);
    noteLabel.setFont (juce::Font (24.0f, juce::Font::bold));
    noteLabel.setColour (juce::Label::textColourId, kAmber);
    noteLabel.setText ("--", juce::dontSendNotification);
    addAndMakeVisible (noteLabel);

    trackLabel.setJustificationType (juce::Justification::centredLeft);
    trackLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    trackLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc99100));
    trackLabel.setText ("PLAY A NOTE", juce::dontSendNotification);
    addAndMakeVisible (trackLabel);

    // ---- preset bar ----
    presetBox.setTextWhenNothingSelected ("PRESETS");
    presetBox.onChange = [this] { presetChosen(); };
    addAndMakeVisible (presetBox);

    nameBox.setTextToShowWhenEmpty ("name to save as", juce::Colour (0xff6d675a));
    nameBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0d0d0f));
    nameBox.setColour (juce::TextEditor::textColourId, kAmber);
    nameBox.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3a3a3e));
    nameBox.setColour (juce::TextEditor::focusedOutlineColourId, kAmber);
    addAndMakeVisible (nameBox);

    saveButton.onClick = [this] { saveCurrentPreset(); };
    addAndMakeVisible (saveButton);
    deleteButton.onClick = [this] { deleteCurrentPreset(); };
    addAndMakeVisible (deleteButton);

    refreshPresetList();
    presetBox.setSelectedId (1, juce::dontSendNotification);   // a fresh plugin opens on "Thing of Gold"

    setSize (kWindowW, kWindowH);
    startTimerHz (30);
}

PhatTrackAudioProcessorEditor::~PhatTrackAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
// Presets. Factory preset "Thing of Gold" is built in; user presets are small XML
// files in the user's Application Support folder, so they survive reinstalling the plugin.
juce::File PhatTrackAudioProcessorEditor::presetFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("PhatTrack").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

juce::File PhatTrackAudioProcessorEditor::presetFile (const juce::String& name)
{
    return presetFolder().getChildFile (juce::File::createLegalFileName (name) + ".phatpreset");
}

void PhatTrackAudioProcessorEditor::refreshPresetList (const juce::String& selectName)
{
    userPresetNames.clear();
    for (const auto& f : presetFolder().findChildFiles (juce::File::findFiles, false, "*.phatpreset"))
        userPresetNames.add (f.getFileNameWithoutExtension());
    userPresetNames.sort (true);

    presetBox.clear (juce::dontSendNotification);
    presetBox.addItem ("Thing of Gold", 1);
    if (userPresetNames.size() > 0)
        presetBox.addSeparator();
    for (int i = 0; i < userPresetNames.size(); ++i)
        presetBox.addItem (userPresetNames[i], 100 + i);

    const int idx = selectName.isNotEmpty() ? userPresetNames.indexOf (selectName) : -1;
    if (idx >= 0)
        presetBox.setSelectedId (100 + idx, juce::dontSendNotification);
}

void PhatTrackAudioProcessorEditor::presetChosen()
{
    const int id = presetBox.getSelectedId();

    if (id == 1)
    {
        audioProcessor.loadThingOfGold();
        nameBox.setText ("", false);
        return;
    }

    const int idx = id - 100;
    if (idx < 0 || idx >= userPresetNames.size())
        return;

    const auto name = userPresetNames[idx];
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (presetFile (name)));
    if (xml != nullptr && xml->hasTagName ("PhatTrackState"))
    {
        audioProcessor.parameters.replaceState (juce::ValueTree::fromXml (*xml));
        nameBox.setText (name, false);
    }
}

void PhatTrackAudioProcessorEditor::saveCurrentPreset()
{
    juce::String name = nameBox.getText().trim();

    if (name.isEmpty())
    {
        int n = 1;
        while (presetFile ("My Preset " + juce::String (n)).existsAsFile())
            ++n;
        name = "My Preset " + juce::String (n);
    }

    auto state = audioProcessor.parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml == nullptr)
        return;

    if (presetFile (name).replaceWithText (xml->toString()))
    {
        nameBox.setText (name, false);
        refreshPresetList (juce::File::createLegalFileName (name));
    }
}

void PhatTrackAudioProcessorEditor::deleteCurrentPreset()
{
    const int idx = presetBox.getSelectedId() - 100;
    if (idx < 0 || idx >= userPresetNames.size())
        return;   // the factory preset cannot be deleted

    presetFile (userPresetNames[idx]).deleteFile();
    nameBox.setText ("", false);
    refreshPresetList();
}

//==============================================================================
void PhatTrackAudioProcessorEditor::layoutKeys()
{
    keys.clear();

    int whites = 0;
    for (int m = lowNote; m <= highNote; ++m)
        if (! isBlackKey (m))
            ++whites;

    const float ww = (float) keyboardBounds.getWidth() / (float) whites;
    const float bw = ww * 0.62f;
    const float bh = (float) keyboardBounds.getHeight() * 0.62f;
    const float kx = (float) keyboardBounds.getX();
    const float ky = (float) keyboardBounds.getY();

    int wi = 0;
    for (int m = lowNote; m <= highNote; ++m)
    {
        Key k;
        k.midi = m;
        k.black = isBlackKey (m);
        if (! k.black)
        {
            k.r = juce::Rectangle<float> (kx + (float) wi * ww, ky, ww, (float) keyboardBounds.getHeight());
            ++wi;
        }
        else
        {
            k.r = juce::Rectangle<float> (kx + (float) wi * ww - bw * 0.5f, ky, bw, bh);
        }
        keys.push_back (k);
    }
}

void PhatTrackAudioProcessorEditor::resized()
{
    for (auto& s : sections)
    {
        const int n = (int) s.items.size();
        s.bounds = juce::Rectangle<int> (s.x, s.y, n * kCellW + 16, kCellH + 24);

        for (int i = 0; i < n; ++i)
        {
            Control& c = *controls[(size_t) s.items[(size_t) i]];
            const int cx = s.x + 8 + i * kCellW;
            const int top = s.y + 22;

            switch (c.kind)
            {
                case Kind::knob:
                    c.label.setBounds (cx, top, kCellW, 14);
                    c.slider.setBounds (cx, top + 14, kCellW, 84);
                    break;
                case Kind::combo:
                    c.label.setBounds (cx, top, kCellW, 14);
                    c.combo.setBounds (cx + 3, top + 28, kCellW - 6, 24);
                    break;
                case Kind::toggle:
                    c.button.setBounds (cx + 6, top + 26, kCellW - 12, 26);
                    break;
            }
        }
    }

    displayBounds  = juce::Rectangle<int> (330, 14, 440, 62);
    noteLabel.setBounds (displayBounds.getX() + 14, displayBounds.getY() + 3, displayBounds.getWidth() - 28, 34);
    trackLabel.setBounds (displayBounds.getX() + 14, displayBounds.getY() + 39, displayBounds.getWidth() - 28, 18);
    presetBox.setBounds (790, 16, 230, 24);
    nameBox.setBounds (790, 46, 118, 24);
    saveButton.setBounds (912, 46, 52, 24);
    deleteButton.setBounds (968, 46, 52, 24);
    lampBounds     = juce::Rectangle<int> (1040, 34, 16, 16);
    keyboardBounds = juce::Rectangle<int> (62, 602, 1056, 124);
    layoutKeys();
}

//==============================================================================
void PhatTrackAudioProcessorEditor::drawWood (juce::Graphics& g, juce::Rectangle<int> r) const
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5e3822), (float) r.getX(), 0.0f,
                                             juce::Colour (0xff361d11), (float) r.getRight(), 0.0f, false));
    g.fillRect (r);

    juce::Random rnd (1234 + r.getX());
    for (int i = 0; i < 110; ++i)
    {
        const float x = (float) r.getX() + rnd.nextFloat() * (float) r.getWidth();
        const float y0 = rnd.nextFloat() * (float) r.getHeight();
        const float len = 60.0f + rnd.nextFloat() * 320.0f;
        const float drift = (rnd.nextFloat() - 0.5f) * 4.0f;
        if (rnd.nextBool())
            g.setColour (juce::Colour::fromFloatRGBA (0.0f, 0.0f, 0.0f, 0.06f + rnd.nextFloat() * 0.18f));
        else
            g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.78f, 0.55f, 0.03f + rnd.nextFloat() * 0.07f));
        g.drawLine (x, y0, x + drift, y0 + len, 1.0f);
    }

    g.setColour (juce::Colour (0x30ffffff));
    g.fillRect (r.getX(), 0, r.getWidth(), 2);
}

void PhatTrackAudioProcessorEditor::paint (juce::Graphics& g)
{
    // panel face
    g.fillAll (kPanel);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff18181b), 0.0f, 0.0f,
                                             juce::Colour (0xff060607), 0.0f, (float) getHeight(), false));
    g.fillRect (kCheekW, 0, getWidth() - 2 * kCheekW, getHeight());

    // wooden end cheeks
    drawWood (g, juce::Rectangle<int> (0, 0, kCheekW, getHeight()));
    drawWood (g, juce::Rectangle<int> (getWidth() - kCheekW, 0, kCheekW, getHeight()));

    // title
    g.setColour (juce::Colour (0xfff2eee2));
    g.setFont (juce::Font (30.0f, juce::Font::bold));
    g.drawText ("PHATTRACK", 70, 14, 240, 36, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff9a9486));
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawText ("GUITAR-TO-SYNTH TRACKING MONOSYNTH", 72, 50, 300, 14, juce::Justification::centredLeft);

    // amber display
    g.setColour (juce::Colour (0xff050806));
    g.fillRoundedRectangle (displayBounds.toFloat(), 6.0f);
    g.setColour (juce::Colour (0xff3a3a3e));
    g.drawRoundedRectangle (displayBounds.toFloat(), 6.0f, 1.5f);

    // gate lamp
    g.setColour (shownGate ? kAmber : juce::Colour (0xff2a2000));
    g.fillEllipse (lampBounds.toFloat());
    g.setColour (juce::Colour (0xff3a3a3e));
    g.drawEllipse (lampBounds.toFloat(), 1.0f);
    g.setColour (kCream);
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText ("GATE", lampBounds.getX() - 14, lampBounds.getBottom() + 3, lampBounds.getWidth() + 28, 12,
                juce::Justification::centred);

    // section frames
    for (const auto& s : sections)
    {
        g.setColour (kOutline);
        g.drawRoundedRectangle (s.bounds.toFloat().reduced (1.0f), 5.0f, 1.0f);

        g.setFont (juce::Font (10.5f, juce::Font::bold));
        const int tw = (int) ((float) s.title.length() * 7.4f) + 16;
        g.setColour (kPanel);
        g.fillRect (s.x + 10, s.y - 1, tw, 14);
        g.setColour (kCream);
        g.drawText (s.title, s.x + 10, s.y - 1, tw, 14, juce::Justification::centred);
    }

    // keyboard: the key for the note you are playing lights up
    const int pressed = shownMidi >= 0.0f ? (int) std::lround (shownMidi) : -1;

    g.setColour (juce::Colours::black);
    g.fillRect (keyboardBounds.expanded (3, 3));

    for (const auto& k : keys)
    {
        if (k.black) continue;
        const bool down = (k.midi == pressed);
        auto r = k.r.reduced (0.7f, 0.0f);
        if (down) r = r.withTrimmedTop (3.0f);

        if (down)
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffd266), r.getX(), r.getY(),
                                                     juce::Colour (0xffff9d00), r.getX(), r.getBottom(), false));
        else
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6f3ea), r.getX(), r.getY(),
                                                     juce::Colour (0xffd3cebd), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 2.0f);

        if (k.midi % 12 == 0)
        {
            auto lr = k.r.toNearestInt();
            auto labelArea = lr.removeFromBottom (16);
            g.setColour (juce::Colour (0xff6d675a));
            g.setFont (juce::Font (9.0f, juce::Font::bold));
            g.drawText (juce::MidiMessage::getMidiNoteName (k.midi, true, true, 4), labelArea,
                        juce::Justification::centred);
        }
    }

    for (const auto& k : keys)
    {
        if (! k.black) continue;
        const bool down = (k.midi == pressed);
        auto r = k.r;
        if (down) r = r.withTrimmedTop (2.0f);

        if (down)
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffb000), r.getX(), r.getY(),
                                                     juce::Colour (0xffc47a00), r.getX(), r.getBottom(), false));
        else
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3d), r.getX(), r.getY(),
                                                     juce::Colour (0xff070708), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 2.0f);
    }
}

//==============================================================================
void PhatTrackAudioProcessorEditor::timerCallback()
{
    const float midi = audioProcessor.playedMidi.load();
    const bool gate = audioProcessor.gateOpenFlag.load() > 0.5f;
    const float hz = audioProcessor.detectedHz.load();
    const float conf = audioProcessor.trackingConfidence.load();
    const float db = audioProcessor.inputLevelDb.load();

    const int newKey = midi >= 0.0f ? (int) std::lround (midi) : -1;
    const int oldKey = shownMidi >= 0.0f ? (int) std::lround (shownMidi) : -1;
    const bool changed = (newKey != oldKey) || (gate != shownGate);
    shownMidi = midi;
    shownGate = gate;

    if (changed)
    {
        repaint (keyboardBounds.expanded (4, 4));
        repaint (lampBounds.expanded (16, 16));
    }

    if (newKey >= 0 && hz > 0.0f)
    {
        const float cents = (midi - (float) newKey) * 100.0f;
        noteLabel.setText (juce::MidiMessage::getMidiNoteName (newKey, true, true, 4)
                               + "   " + juce::String (hz, 1) + " Hz   "
                               + (cents >= 0.0f ? "+" : "") + juce::String (juce::roundToInt (cents)) + " ct",
                           juce::dontSendNotification);
    }
    else
    {
        noteLabel.setText ("--", juce::dontSendNotification);
    }

    trackLabel.setText ("TRACK " + juce::String (juce::roundToInt (conf * 100.0f))
                            + "%     INPUT " + juce::String (db, 1) + " dB",
                        juce::dontSendNotification);
}
