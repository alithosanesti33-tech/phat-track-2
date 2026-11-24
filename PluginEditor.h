#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Dark panel, chunky black knobs with a white pointer, amber readouts.
class PhatLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PhatLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

//==============================================================================
class PhatTrackAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit PhatTrackAudioProcessorEditor (PhatTrackAudioProcessor&);
    ~PhatTrackAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    enum class Kind { knob, combo, toggle };

    struct Control
    {
        Kind kind = Kind::knob;
        juce::Label label;
        juce::Slider slider;
        juce::ComboBox combo;
        juce::TextButton button;
        // attachments are declared last so they are destroyed first
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAtt;
    };

    struct Section
    {
        juce::String title;
        int x = 0;
        int y = 0;
        std::vector<int> items;
        juce::Rectangle<int> bounds;
    };

    struct Key
    {
        int midi = 0;
        bool black = false;
        juce::Rectangle<float> r;
    };

    int addKnob (const char* id, const char* labelText, const char* suffix, int decimals);
    int addCombo (const char* id, const char* labelText);
    int addToggle (const char* id, const char* text);
    void addSection (const char* title, int x, int y, std::initializer_list<int> items);
    void layoutKeys();
    void refreshPresetList (const juce::String& selectName = {});
    void presetChosen();
    void saveCurrentPreset();
    void deleteCurrentPreset();
    static juce::File presetFolder();
    static juce::File presetFile (const juce::String& name);
    void drawWood (juce::Graphics&, juce::Rectangle<int>) const;

    static bool isBlackKey (int midi);

    static constexpr int lowNote = 40;    // E2, lowest guitar string
    static constexpr int highNote = 88;   // E6

    PhatTrackAudioProcessor& audioProcessor;
    PhatLookAndFeel lookAndFeel;   // declared before the controls so it outlives them

    std::vector<std::unique_ptr<Control>> controls;
    std::vector<Section> sections;
    std::vector<Key> keys;

    juce::Label noteLabel, trackLabel;

    // preset bar
    juce::ComboBox presetBox;
    juce::TextEditor nameBox;
    juce::TextButton saveButton { "SAVE" }, deleteButton { "DEL" };
    juce::StringArray userPresetNames;
    juce::Rectangle<int> displayBounds, keyboardBounds, lampBounds;

    float shownMidi = -1.0f;
    bool shownGate = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhatTrackAudioProcessorEditor)
};
