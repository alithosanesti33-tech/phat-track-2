#include "PluginEditor.h"

void PhatTrackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                             float pos, float start, float end, juce::Slider&)
{
    const float diameter = (float)juce::jmin (w, h) * 0.70f;
    const float cx = (float)x + (float)w * 0.5f;
    const float cy = (float)y + (float)h * 0.48f;
    const float r = diameter * 0.5f;
    const float angle = start + pos * (end - start);
    g.setColour (juce::Colour (0xff090c0e));
    g.fillEllipse (cx - r - 2, cy - r - 2, diameter + 4, diameter + 4);
    g.setColour (juce::Colour (0xff30383b));
    g.fillEllipse (cx - r, cy - r, diameter, diameter);
    g.setColour (juce::Colour (0xff596164));
    g.drawEllipse (cx - r, cy - r, diameter, diameter, 1.2f);
    juce::Path arc;
    arc.addCentredArc (cx, cy, r + 4, r + 4, 0.0f, start, end, true);
    g.setColour (juce::Colour (0xff171e21));
    g.strokePath (arc, juce::PathStrokeType (3.0f));
    juce::Path valueArc;
    valueArc.addCentredArc (cx, cy, r + 4, r + 4, 0.0f, start, angle, true);
    g.setColour (juce::Colour (0xffe87538));
    g.strokePath (valueArc, juce::PathStrokeType (3.0f));
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.7f, -r + 7.0f, 3.4f, r * 0.42f, 1.5f);
    g.setColour (juce::Colour (0xffffc18b));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (cx, cy));
    g.setColour (juce::Colour (0xff15191b));
    g.fillEllipse (cx - r * 0.13f, cy - r * 0.13f, r * 0.26f, r * 0.26f);
}

PhatTrackAudioProcessorEditor::PhatTrackAudioProcessorEditor (PhatTrackAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);
    setSize (900, 590);
    title.setText ("PHATTRACK", juce::dontSendNotification);
    title.setFont (juce::Font (29.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, juce::Colour (0xfff1e8d8));
    addAndMakeVisible (title);
    subtitle.setText ("GUITAR-CONTROLLED ANALOG MONOSYNTH", juce::dontSendNotification);
    subtitle.setFont (juce::Font (10.0f, juce::Font::bold));
    subtitle.setColour (juce::Label::textColourId, juce::Colour (0xffd17a49));
    addAndMakeVisible (subtitle);
    display.setJustificationType (juce::Justification::centredLeft);
    display.setFont (juce::Font (14.0f, juce::Font::bold));
    display.setColour (juce::Label::textColourId, juce::Colour (0xffa9d4bc));
    addAndMakeVisible (display);
    trackerLabel.setJustificationType (juce::Justification::centredRight);
    trackerLabel.setFont (juce::Font (10.0f));
    trackerLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9ba6a2));
    addAndMakeVisible (trackerLabel);

    addKnob ("oscMix", "OSC MIX");
    addKnob ("sub", "SUB LEVEL");
    addKnob ("detune", "OSC 2 TUNE", "ct");
    addKnob ("cutoff", "CUTOFF", "Hz");
    addKnob ("resonance", "RESONANCE");
    addKnob ("filterEnv", "FILTER ENV");
    addKnob ("attack", "ATTACK", "s");
    addKnob ("decay", "DECAY", "s");
    addKnob ("sustain", "SUSTAIN");
    addKnob ("release", "RELEASE", "s");
    addKnob ("glide", "GLIDE", "s");
    addKnob ("drive", "OVERLOAD");
    addKnob ("lfoRate", "LFO RATE", "Hz");
    addKnob ("lfoDepth", "LFO AMOUNT");
    addKnob ("sensitivity", "INPUT THRESH", "dB");
    addKnob ("dry", "DRY BLEND");
    addKnob ("output", "OUTPUT");

    wave1.addItemList ({ "SAW", "SQUARE", "TRIANGLE" }, 1);
    wave2.addItemList ({ "SAW", "SQUARE", "TRIANGLE" }, 1);
    wave1.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff20282b));
    wave2.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff20282b));
    wave1.setColour (juce::ComboBox::textColourId, juce::Colour (0xfff1e8d8));
    wave2.setColour (juce::ComboBox::textColourId, juce::Colour (0xfff1e8d8));
    addAndMakeVisible (wave1); addAndMakeVisible (wave2);
    wave1Attachment = std::make_unique<ComboAttachment> (audioProcessor.parameters, "wave1", wave1);
    wave2Attachment = std::make_unique<ComboAttachment> (audioProcessor.parameters, "wave2", wave2);
    startTimerHz (12);
}

PhatTrackAudioProcessorEditor::~PhatTrackAudioProcessorEditor() { setLookAndFeel (nullptr); }

void PhatTrackAudioProcessorEditor::addKnob (const juce::String& id, const juce::String& name, const juce::String& suffix)
{
    Knob k;
    k.slider = std::make_unique<juce::Slider>();
    k.slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 17);
    k.slider->setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffc7ceca));
    k.slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff171d20));
    k.slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    if (suffix.isNotEmpty()) k.slider->setTextValueSuffix (" " + suffix);
    addAndMakeVisible (*k.slider);
    k.label = std::make_unique<juce::Label>();
    k.label->setText (name, juce::dontSendNotification);
    k.label->setJustificationType (juce::Justification::centred);
    k.label->setFont (juce::Font (9.5f, juce::Font::bold));
    k.label->setColour (juce::Label::textColourId, juce::Colour (0xffd9d2c6));
    addAndMakeVisible (*k.label);
    k.attachment = std::make_unique<Attachment> (audioProcessor.parameters, id, *k.slider);
    knobs.push_back (std::move (k));
}

void PhatTrackAudioProcessorEditor::timerCallback()
{
    const float hz = audioProcessor.detectedHz.load();
    const float conf = audioProcessor.trackingConfidence.load();
    const float db = audioProcessor.inputLevelDb.load();
    if (hz > 0.0f)
    {
        const int midi = juce::jlimit (0, 127, (int)std::round (69.0 + 12.0 * std::log2 (hz / 440.0f)));
        display.setText (juce::MidiMessage::getMidiNoteName (midi, true, true, 4) + "   " + juce::String (hz, 1) + " Hz", juce::dontSendNotification);
    }
    else display.setText ("WAITING FOR GUITAR INPUT", juce::dontSendNotification);
    trackerLabel.setText ("TRACK " + juce::String (juce::jroundToInt (conf * 100.0f)) + "%     IN " + juce::String (db, 1) + " dB", juce::dontSendNotification);
}

void PhatTrackAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff101619));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff293236), 0, 0, juce::Colour (0xff171e21), 0, (float)getHeight(), false));
    g.fillRoundedRectangle (bounds.reduced (9.0f), 12.0f);
    g.setColour (juce::Colour (0xff080c0e));
    g.drawRoundedRectangle (bounds.reduced (9.0f), 12.0f, 2.0f);
    g.setColour (juce::Colour (0xffe87538));
    g.fillRoundedRectangle (20.0f, 19.0f, 5.0f, 48.0f, 2.0f);

    // Distinctive, original synth panel styling: warm hardware accents and a compact status display.
    g.setColour (juce::Colour (0xff0b1113));
    g.fillRoundedRectangle (22.0f, 78.0f, (float)getWidth() - 44.0f, 52.0f, 5.0f);
    g.setColour (juce::Colour (0xff455156));
    g.drawRoundedRectangle (22.0f, 78.0f, (float)getWidth() - 44.0f, 52.0f, 5.0f, 1.0f);
    g.setColour (juce::Colour (0xffe87538));
    g.setFont (juce::Font (9.0f, juce::Font::bold));
    g.drawText ("OSCILLATORS", 30, 146, 165, 15, juce::Justification::centredLeft);
    g.drawText ("FILTER", 205, 146, 165, 15, juce::Justification::centredLeft);
    g.drawText ("AMPLIFIER", 380, 146, 165, 15, juce::Justification::centredLeft);
    g.drawText ("MOVEMENT + INPUT", 555, 146, 300, 15, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff4a5558));
    g.drawLine (195.0f, 154.0f, 195.0f, (float)getHeight() - 18.0f, 1.0f);
    g.drawLine (370.0f, 154.0f, 370.0f, (float)getHeight() - 18.0f, 1.0f);
    g.drawLine (545.0f, 154.0f, 545.0f, (float)getHeight() - 18.0f, 1.0f);
    g.setColour (juce::Colour (0xffe87538));
    g.setFont (juce::Font (8.0f));
    g.drawText ("OSC 1 WAVE", 35, 442, 100, 15, juce::Justification::centred);
    g.drawText ("OSC 2 WAVE", 35, 506, 100, 15, juce::Justification::centred);
    g.setColour (juce::Colour (0xff7d8988));
    g.setFont (juce::Font (9.0f));
    g.drawText ("MONO GUITAR INPUT  •  NO MIDI CONVERSION REQUIRED", 25, getHeight() - 25, getWidth() - 50, 14, juce::Justification::centred);
}

void PhatTrackAudioProcessorEditor::resized()
{
    title.setBounds (36, 21, 260, 34);
    subtitle.setBounds (38, 52, 340, 16);
    display.setBounds (36, 91, 390, 27);
    trackerLabel.setBounds (570, 91, 290, 27);
    const int colW = 175;
    const int knobW = 78;
    const int knobH = 105;
    const int startX = 25;
    const int topY = 164;
    const int rowGap = 125;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int group = (int)i / 4;
        const int inGroup = (int)i % 4;
        int x = startX + group * colW + (inGroup % 2) * 84;
        int y = topY + (inGroup / 2) * rowGap;
        // Final group contains five controls; the fifth sits centered on a third row.
        if (i >= 12) { x = 555 + ((int)i - 12) * 84; y = topY; }
        if (i == 16) { x = 597; y = 405; }
        knobs[i].slider->setBounds (x, y, knobW, knobH);
        knobs[i].label->setBounds (x - 5, y - 13, knobW + 10, 15);
    }
    wave1.setBounds (42, 458, 105, 27);
    wave2.setBounds (42, 522, 105, 27);
}
