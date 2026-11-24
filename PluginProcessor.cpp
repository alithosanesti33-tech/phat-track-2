#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        using P = juce::AudioProcessorValueTreeState;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
        auto addFloat = [&p](const char* id, const char* name, float lo, float hi, float def, float skew = 1.0f)
        {
            juce::NormalisableRange<float> range (lo, hi, 0.001f, skew);
            p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def));
        };
        auto addChoice = [&p](const char* id, const char* name, juce::StringArray choices, int def)
        {
            p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, def));
        };
        addFloat ("oscMix", "Osc Mix", 0.0f, 1.0f, 0.5f);
        addFloat ("sub", "Sub", 0.0f, 1.0f, 0.18f);
        addChoice ("wave1", "Osc 1 Wave", { "Saw", "Square", "Triangle" }, 0);
        addChoice ("wave2", "Osc 2 Wave", { "Saw", "Square", "Triangle" }, 1);
        addFloat ("detune", "Detune", -30.0f, 30.0f, 7.0f);
        addFloat ("cutoff", "Cutoff", 80.0f, 12000.0f, 2400.0f, 0.35f);
        addFloat ("resonance", "Resonance", 0.0f, 1.0f, 0.28f);
        addFloat ("filterEnv", "Filter Env", 0.0f, 1.0f, 0.45f);
        addFloat ("attack", "Attack", 0.001f, 2.0f, 0.015f, 0.35f);
        addFloat ("decay", "Decay", 0.005f, 3.0f, 0.22f, 0.4f);
        addFloat ("sustain", "Sustain", 0.0f, 1.0f, 0.72f);
        addFloat ("release", "Release", 0.01f, 4.0f, 0.22f, 0.4f);
        addFloat ("glide", "Glide", 0.0f, 0.5f, 0.045f, 0.5f);
        addFloat ("drive", "Drive", 0.0f, 1.0f, 0.22f);
        addFloat ("lfoRate", "LFO Rate", 0.05f, 12.0f, 5.0f, 0.4f);
        addFloat ("lfoDepth", "LFO Depth", 0.0f, 1.0f, 0.0f);
        addFloat ("sensitivity", "Sensitivity", -60.0f, -12.0f, -38.0f);
        addFloat ("dry", "Dry", 0.0f, 1.0f, 0.0f);
        addFloat ("output", "Output", 0.0f, 1.5f, 0.8f);
        return { p.begin(), p.end() };
    }
}

PhatTrackAudioProcessor::PhatTrackAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
{}

void PhatTrackAudioProcessor::PitchTracker::prepare (double sr) { sampleRate = sr; reset(); }
void PhatTrackAudioProcessor::PitchTracker::reset()
{
    ring.fill (0.0f); writePos = 0; sinceUpdate = 0; lastHz = 0.0f;
}
void PhatTrackAudioProcessor::PitchTracker::push (float x)
{
    ring[(size_t) writePos] = x;
    writePos = (writePos + 1) % frameSize;
    ++sinceUpdate;
}
bool PhatTrackAudioProcessor::PitchTracker::update (float& hz, float& confidence)
{
    if (sinceUpdate < 512) return false;
    sinceUpdate = 0;
    std::array<float, frameSize> frame {};
    for (int i = 0; i < frameSize; ++i) frame[(size_t)i] = ring[(size_t)((writePos + i) % frameSize)];
    float mean = 0.0f;
    for (float v : frame) mean += v;
    mean /= (float) frameSize;
    float energy = 0.0f;
    for (auto& v : frame) { v -= mean; energy += v * v; }
    if (energy < 0.00008f) { confidence = 0.0f; return true; }

    constexpr int minLag = 40;   // ~1100 Hz at 44.1 kHz
    const int maxLag = juce::jmin (900, (int)(sampleRate / 48.0));
    std::array<float, 901> difference {};
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        float sum = 0.0f;
        const int n = frameSize - lag;
        for (int i = 0; i < n; ++i)
        {
            const float d = frame[(size_t)i] - frame[(size_t)(i + lag)];
            sum += d * d;
        }
        difference[(size_t)lag] = sum;
    }
    float running = 0.0f;
    std::array<float, 901> cmnd {};
    for (int lag = 1; lag <= maxLag; ++lag)
    {
        running += difference[(size_t)lag];
        cmnd[(size_t)lag] = running > 0.0f ? difference[(size_t)lag] * (float)lag / running : 1.0f;
    }
    int best = -1;
    for (int lag = minLag; lag < maxLag; ++lag)
    {
        if (cmnd[(size_t)lag] < 0.18f && cmnd[(size_t)lag] <= cmnd[(size_t)(lag + 1)])
        { best = lag; break; }
    }
    if (best < 0)
    {
        float lowest = 1.0f;
        for (int lag = minLag; lag <= maxLag; ++lag)
            if (cmnd[(size_t)lag] < lowest) { lowest = cmnd[(size_t)lag]; best = lag; }
        if (best < 0 || lowest > 0.32f) { confidence = 0.0f; return true; }
    }
    const float a = cmnd[(size_t)juce::jmax (minLag, best - 1)];
    const float b = cmnd[(size_t)best];
    const float c = cmnd[(size_t)juce::jmin (maxLag, best + 1)];
    const float denom = a - 2.0f * b + c;
    const float shift = std::abs (denom) > 1.0e-8f ? 0.5f * (a - c) / denom : 0.0f;
    const float refinedLag = (float) best + juce::jlimit (-0.5f, 0.5f, shift);
    const float candidate = (float) (sampleRate / refinedLag);
    confidence = juce::jlimit (0.0f, 1.0f, 1.0f - b);
    if (candidate < 45.0f || candidate > 1200.0f || confidence < 0.45f) return true;
    // Reject implausible octave jumps unless the new estimate is clearly stronger.
    if (lastHz > 0.0f && (candidate > lastHz * 1.9f || candidate < lastHz * 0.52f) && confidence < 0.88f)
        return true;
    lastHz = candidate;
    hz = candidate;
    return true;
}

void PhatTrackAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    currentSampleRate = sr;
    tracker.prepare (sr);
    dryBuffer.setSize (getTotalNumInputChannels(), samplesPerBlock, false, true, true);
    ampEnv.setSampleRate (sr);
    resetSynth();
}
void PhatTrackAudioProcessor::releaseResources() {}
bool PhatTrackAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo()) && out == in;
}
float PhatTrackAudioProcessor::getParam (const char* id) const
{
    if (auto* v = parameters.getRawParameterValue (id)) return v->load();
    return 0.0f;
}
float PhatTrackAudioProcessor::oscillator (float phase, int shape) const
{
    const float p = phase - std::floor (phase);
    if (shape == 1) return p < 0.5f ? 1.0f : -1.0f;
    if (shape == 2) return 1.0f - 4.0f * std::abs (p - 0.5f);
    return 2.0f * p - 1.0f;
}
void PhatTrackAudioProcessor::resetSynth()
{
    phase1 = phase2 = lfoPhase = 0.0f;
    smoothedHz = 0.0f;
    gateEnvelope = 0.0f;
    gateOn = false;
    filter1 = filter2 = filter3 = filter4 = 0.0f;
    inputFollower = 0.0f;
    ampEnv.reset();
}

void PhatTrackAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels = getTotalNumInputChannels();
    const int samples = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch) buffer.clear (ch, 0, samples);
    if (channels == 0) { buffer.clear(); return; }

    dryBuffer.setSize (channels, samples, false, false, true);
    for (int ch = 0; ch < channels; ++ch) dryBuffer.copyFrom (ch, 0, buffer, ch, 0, samples);
    const float* inL = dryBuffer.getReadPointer (0);
    const float* inR = channels > 1 ? dryBuffer.getReadPointer (1) : inL;

    const float sensitivity = getParam ("sensitivity");
    const float threshold = juce::Decibels::decibelsToGain (sensitivity);
    const float glide = getParam ("glide");
    const float cutoff = getParam ("cutoff");
    const float resonance = getParam ("resonance");
    const float drive = getParam ("drive");
    const float mix = getParam ("oscMix");
    const float sub = getParam ("sub");
    const float detune = getParam ("detune");
    const float filterAmount = getParam ("filterEnv");
    const float dry = getParam ("dry");
    const float output = getParam ("output");
    const int wave1 = (int)getParam ("wave1");
    const int wave2 = (int)getParam ("wave2");
    envParams.attack = getParam ("attack");
    envParams.decay = getParam ("decay");
    envParams.sustain = getParam ("sustain");
    envParams.release = getParam ("release");
    ampEnv.setParameters (envParams);

    const float lfoRate = getParam ("lfoRate");
    const float lfoDepth = getParam ("lfoDepth");
    const float glideCoeff = glide <= 0.0001f ? 1.0f : 1.0f - std::exp (-1.0f / (float)(currentSampleRate * glide));
    const float feedback = resonance * 3.65f;
    float blockPeak = 0.0f;

    for (int i = 0; i < samples; ++i)
    {
        const float input = channels > 1 ? 0.5f * (inL[i] + inR[i]) : inL[i];
        tracker.push (input);
        blockPeak = juce::jmax (blockPeak, std::abs (input));
        float detected = 0.0f, conf = 0.0f;
        if (tracker.update (detected, conf))
        {
            trackingConfidence.store (conf);
            if (detected > 0.0f) detectedHz.store (detected);
        }
        const float level = std::abs (input);
        const float followerCoeff = level > inputFollower
            ? 1.0f - std::exp (-1.0f / (0.004f * (float)currentSampleRate))
            : 1.0f - std::exp (-1.0f / (0.055f * (float)currentSampleRate));
        inputFollower += followerCoeff * (level - inputFollower);
        const bool newGate = inputFollower > threshold;
        if (newGate && !gateOn) { gateOn = true; ampEnv.noteOn(); }
        else if (!newGate && gateOn) { gateOn = false; ampEnv.noteOff(); }

        const float targetHz = detectedHz.load();
        if (targetHz > 0.0f && trackingConfidence.load() > 0.4f)
            smoothedHz += (targetHz - smoothedHz) * glideCoeff;

        const float lfo = std::sin (lfoPhase * juce::MathConstants<float>::twoPi);
        lfoPhase += lfoRate / (float)currentSampleRate;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        const float vibrato = std::pow (2.0f, (lfo * lfoDepth * 0.35f) / 12.0f);
        const float f1 = smoothedHz * vibrato;
        const float f2 = f1 * std::pow (2.0f, detune / 1200.0f);
        const float inc1 = juce::jlimit (0.0f, 0.45f, f1 / (float)currentSampleRate);
        const float inc2 = juce::jlimit (0.0f, 0.45f, f2 / (float)currentSampleRate);
        phase1 += inc1; phase2 += inc2;
        if (phase1 >= 1.0f) phase1 -= 1.0f;
        if (phase2 >= 1.0f) phase2 -= 1.0f;
        float osc = smoothedHz > 0.0f ? oscillator (phase1, wave1) * (1.0f - mix) + oscillator (phase2, wave2) * mix : 0.0f;
        // A sub-octave square adds weight without changing the tracked note.
        if (smoothedHz > 0.0f) osc = osc * (1.0f - sub * 0.45f) + (phase1 < 0.5f ? 1.0f : -1.0f) * sub * 0.45f;
        const float env = ampEnv.getNextSample();
        const float filterEnv = 0.18f + env * filterAmount * 2.5f;
        const float fc = juce::jlimit (40.0f, 16000.0f, cutoff * filterEnv);
        const float g = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * fc / (float)currentSampleRate);
        float x = std::tanh (osc * (1.0f + drive * 7.0f) - feedback * filter4 * 0.22f);
        filter1 += g * (x - filter1);
        filter2 += g * (filter1 - filter2);
        filter3 += g * (filter2 - filter3);
        filter4 += g * (filter3 - filter4);
        const float synth = std::tanh (filter4 * (1.0f + drive * 1.5f)) * env * output;
        const float outL = synth * (1.0f - dry) + inL[i] * dry;
        const float outR = synth * (1.0f - dry) + inR[i] * dry;
        buffer.setSample (0, i, outL);
        if (buffer.getNumChannels() > 1) buffer.setSample (1, i, outR);
    }
    inputLevelDb.store (juce::Decibels::gainToDecibels (blockPeak, -100.0f));
}

juce::AudioProcessorEditor* PhatTrackAudioProcessor::createEditor() { return new PhatTrackAudioProcessorEditor (*this); }
void PhatTrackAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, dest);
}
void PhatTrackAudioProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml && xml->hasTagName (parameters.state.getType())) parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PhatTrackAudioProcessor(); }
