#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "PhatCore.h"

// Parameter IDs, shared by processor and editor so a typo is a compile error.
namespace ids
{
    // input / trigger
    inline constexpr const char* gate    = "gate";
    inline constexpr const char* sens    = "sens";
    inline constexpr const char* dry     = "dry";
    inline constexpr const char* retrig  = "retrig";
    // master
    inline constexpr const char* octave  = "octave";
    inline constexpr const char* tune    = "tune";
    inline constexpr const char* glide   = "glide";
    inline constexpr const char* glideOn = "glideon";
    inline constexpr const char* volume  = "volume";
    // oscillator 1
    inline constexpr const char* o1Oct   = "o1oct";
    inline constexpr const char* o1Wave  = "o1wave";
    inline constexpr const char* o1Semi  = "o1semi";
    inline constexpr const char* o1Det   = "o1det";
    inline constexpr const char* o1Lvl   = "o1lvl";
    // oscillator 2
    inline constexpr const char* o2Oct   = "o2oct";
    inline constexpr const char* o2Wave  = "o2wave";
    inline constexpr const char* o2Semi  = "o2semi";
    inline constexpr const char* o2Det   = "o2det";
    inline constexpr const char* o2Lvl   = "o2lvl";
    // mixer
    inline constexpr const char* sync    = "sync";
    inline constexpr const char* sub     = "sub";
    inline constexpr const char* noise   = "noise";
    // filter
    inline constexpr const char* cutoff  = "cutoff";
    inline constexpr const char* res     = "res";
    inline constexpr const char* keyTrk  = "keytrk";
    inline constexpr const char* fEnvAmt = "fenvamt";
    inline constexpr const char* drive   = "drive";
    // filter envelope
    inline constexpr const char* fA = "fatk";
    inline constexpr const char* fD = "fdec";
    inline constexpr const char* fS = "fsus";
    inline constexpr const char* fR = "frel";
    // amp envelope
    inline constexpr const char* aA = "aatk";
    inline constexpr const char* aD = "adec";
    inline constexpr const char* aS = "asus";
    inline constexpr const char* aR = "arel";
    // modulation
    inline constexpr const char* lfoRate = "lforate";
    inline constexpr const char* modSrc  = "modsrc";
    inline constexpr const char* modDst  = "moddst";
    inline constexpr const char* modAmt  = "modamt";
}

class PhatTrackAudioProcessor final : public juce::AudioProcessor
{
public:
    PhatTrackAudioProcessor();
    ~PhatTrackAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PhatTrack"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS parameters;

    // Read by the editor (message thread), written by the audio thread.
    std::atomic<float> detectedHz { 0.0f };
    std::atomic<float> trackingConfidence { 0.0f };
    std::atomic<float> inputLevelDb { -100.0f };
    std::atomic<float> playedMidi { -1.0f };   // tracked guitar note while a note is sounding, else -1
    std::atomic<float> gateOpenFlag { 0.0f };

private:
    struct ParamPtrs
    {
        std::atomic<float> *gate = nullptr, *sens = nullptr, *dry = nullptr, *retrig = nullptr;
        std::atomic<float> *octave = nullptr, *tune = nullptr, *glide = nullptr, *glideOn = nullptr, *volume = nullptr;
        std::atomic<float> *o1Oct = nullptr, *o1Wave = nullptr, *o1Semi = nullptr, *o1Det = nullptr, *o1Lvl = nullptr;
        std::atomic<float> *o2Oct = nullptr, *o2Wave = nullptr, *o2Semi = nullptr, *o2Det = nullptr, *o2Lvl = nullptr;
        std::atomic<float> *sync = nullptr, *sub = nullptr, *noise = nullptr;
        std::atomic<float> *cutoff = nullptr, *res = nullptr, *keyTrk = nullptr, *fEnvAmt = nullptr, *drive = nullptr;
        std::atomic<float> *fA = nullptr, *fD = nullptr, *fS = nullptr, *fR = nullptr;
        std::atomic<float> *aA = nullptr, *aD = nullptr, *aS = nullptr, *aR = nullptr;
        std::atomic<float> *lfoRate = nullptr, *modSrc = nullptr, *modDst = nullptr, *modAmt = nullptr;
    } p;

    void resetState();

    phat::PitchTracker tracker;
    phat::GateTrigger gateTrigger;
    phat::Voice voice;

    double sampleRate = 48000.0;

    // tracking state (audio thread only)
    float rawMidi = 60.0f, slowMidi = 60.0f, smoothMidi = 60.0f, glideMidi = 60.0f;
    float lastClarity = 0.0f, lastHz = 0.0f;
    bool haveRaw = false, pitchReady = false, needSnap = true;
    int samplesSinceResult = 1000000;
    int jumpCooldown = 0;
    float validGain = 0.0f, dryGate = 0.0f;
    float smoothCoefSlow = 0.01f, smoothCoefFast = 0.1f, slowFollowCoef = 0.01f;
    float validCoef = 0.05f, dryAtk = 0.01f, dryRel = 0.001f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhatTrackAudioProcessor)
};
