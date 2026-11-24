#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PhatTrackLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
};

class PhatTrackAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit PhatTrackAudioProcessorEditor (PhatTrackAudioProcessor&);
    ~PhatTrackAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    PhatTrackAudioProcessor& audioProcessor;
    PhatTrackLookAndFeel lookAndFeel;

    struct Knob
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<Attachment> attachment;
    };
    std::vector<Knob> knobs;
    juce::ComboBox wave1, wave2;
    std::unique_ptr<ComboAttachment> wave1Attachment, wave2Attachment;
    juce::Label title, subtitle, display, trackerLabel;
    juce::Rectangle<int> displayBounds;

    void addKnob (const juce::String& id, const juce::String& name, const juce::String& suffix = {});
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhatTrackAudioProcessorEditor)
};
