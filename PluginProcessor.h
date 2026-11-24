#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

class PhatTrackAudioProcessor final : public juce::AudioProcessor
{
public:
    PhatTrackAudioProcessor();
    ~PhatTrackAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PhatTrack"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS parameters;
    std::atomic<float> detectedHz { 0.0f };
    std::atomic<float> trackingConfidence { 0.0f };
    std::atomic<float> inputLevelDb { -100.0f };

private:
    class PitchTracker
    {
    public:
        void prepare (double sr);
        void reset();
        void push (float x);
        bool update (float& hz, float& confidence);
    private:
        static constexpr int frameSize = 2048;
        std::array<float, frameSize> ring {};
        int writePos = 0;
        int sinceUpdate = 0;
        double sampleRate = 44100.0;
        float lastHz = 0.0f;
    };

    PitchTracker tracker;
    double currentSampleRate = 44100.0;
    juce::ADSR ampEnv;
    juce::ADSR::Parameters envParams;
    float phase1 = 0.0f, phase2 = 0.0f, lfoPhase = 0.0f;
    float smoothedHz = 0.0f;
    float gateEnvelope = 0.0f;
    bool gateOn = false;
    float filter1 = 0.0f, filter2 = 0.0f, filter3 = 0.0f, filter4 = 0.0f;
    float lastInputLevel = 0.0f;
    float inputFollower = 0.0f;
    juce::AudioBuffer<float> dryBuffer;
    juce::Random random;

    float getParam (const char* id) const;
    float oscillator (float phase, int shape) const;
    void resetSynth();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhatTrackAudioProcessor)
};
