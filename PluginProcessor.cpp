#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        auto addFloat = [&params] (const char* id, const char* name, float lo, float hi, float def,
                                   float skew = 1.0f, float step = 0.001f)
        {
            juce::NormalisableRange<float> range (lo, hi, step, skew);
            params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def));
        };
        auto addChoice = [&params] (const char* id, const char* name, juce::StringArray choices, int def)
        {
            params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, def));
        };
        auto addBool = [&params] (const char* id, const char* name, bool def)
        {
            params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
        };

        // input / trigger
        addFloat  (ids::gate,   "Noise Gate", -80.0f, -20.0f, -52.0f, 1.0f, 0.1f);
        addFloat  (ids::sens,   "Sensitivity", -12.0f, 24.0f, 0.0f, 1.0f, 0.1f);
        addFloat  (ids::dry,    "Dry Mix", 0.0f, 1.0f, 0.0f);
        addBool   (ids::retrig, "Envelope Retrigger", true);

        // master
        addChoice (ids::octave,  "Octave", { "-2", "-1", "0", "+1", "+2" }, 2);
        addFloat  (ids::tune,    "Fine Tune", -100.0f, 100.0f, 0.0f, 1.0f, 0.1f);
        addFloat  (ids::glide,   "Glide", 0.0f, 2.0f, 0.12f, 0.45f, 0.001f);
        addBool   (ids::glideOn, "Glide On", false);
        addFloat  (ids::volume,  "Volume", 0.0f, 1.5f, 0.8f);

        // oscillator 1
        addChoice (ids::o1Oct,  "Osc 1 Octave", { "16'", "8'", "4'", "2'" }, 1);
        addFloat  (ids::o1Wave, "Osc 1 Wave", 0.0f, 1.0f, 0.34f);
        addFloat  (ids::o1Semi, "Osc 1 Semitone", -24.0f, 24.0f, 0.0f, 1.0f, 1.0f);
        addFloat  (ids::o1Det,  "Osc 1 Detune", -50.0f, 50.0f, 0.0f, 1.0f, 0.1f);
        addFloat  (ids::o1Lvl,  "Osc 1 Level", 0.0f, 1.0f, 0.7f);

        // oscillator 2
        addChoice (ids::o2Oct,  "Osc 2 Octave", { "16'", "8'", "4'", "2'" }, 1);
        addFloat  (ids::o2Wave, "Osc 2 Wave", 0.0f, 1.0f, 0.34f);
        addFloat  (ids::o2Semi, "Osc 2 Semitone", -24.0f, 24.0f, 0.0f, 1.0f, 1.0f);
        addFloat  (ids::o2Det,  "Osc 2 Detune", -50.0f, 50.0f, 7.0f, 1.0f, 0.1f);
        addFloat  (ids::o2Lvl,  "Osc 2 Level", 0.0f, 1.0f, 0.5f);

        // mixer extras
        addBool   (ids::sync,  "Osc Sync", false);
        addFloat  (ids::sub,   "Sub Level", 0.0f, 1.0f, 0.2f);
        addFloat  (ids::noise, "Noise Level", 0.0f, 1.0f, 0.0f);

        // filter
        addFloat  (ids::cutoff,  "Cutoff", 20.0f, 18000.0f, 2400.0f, 0.3f, 0.1f);
        addFloat  (ids::res,     "Resonance", 0.0f, 1.0f, 0.25f);
        addFloat  (ids::keyTrk,  "Key Track", 0.0f, 1.0f, 0.5f);
        addFloat  (ids::fEnvAmt, "Filter Env Amount", -1.0f, 1.0f, 0.45f);
        addFloat  (ids::drive,   "Drive", 0.0f, 1.0f, 0.2f);

        // envelopes
        addFloat  (ids::fA, "Filter Attack",  0.001f, 8.0f, 0.015f, 0.35f);
        addFloat  (ids::fD, "Filter Decay",   0.002f, 8.0f, 0.35f,  0.4f);
        addFloat  (ids::fS, "Filter Sustain", 0.0f,   1.0f, 0.35f);
        addFloat  (ids::fR, "Filter Release", 0.005f, 8.0f, 0.25f,  0.4f);
        addFloat  (ids::aA, "Amp Attack",     0.001f, 8.0f, 0.008f, 0.35f);
        addFloat  (ids::aD, "Amp Decay",      0.002f, 8.0f, 0.3f,   0.4f);
        addFloat  (ids::aS, "Amp Sustain",    0.0f,   1.0f, 0.8f);
        addFloat  (ids::aR, "Amp Release",    0.005f, 8.0f, 0.2f,   0.4f);

        // modulation
        addFloat  (ids::lfoRate, "Mod Rate", 0.05f, 60.0f, 5.0f, 0.4f, 0.01f);
        addChoice (ids::modSrc,  "Mod Source",
                   { "Tri", "Square", "Saw", "Ramp", "Filt Env", "Osc 2", "S&H", "Noise" }, 0);
        addChoice (ids::modDst,  "Mod Destination", { "Pitch", "Osc 2", "Filter", "Wave" }, 0);
        addFloat  (ids::modAmt,  "Mod Amount", 0.0f, 1.0f, 0.0f);

        return { params.begin(), params.end() };
    }
}

//==============================================================================
PhatTrackAudioProcessor::PhatTrackAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PhatTrackState", createParameterLayout())
{
    auto P = [this] (const char* id) { return parameters.getRawParameterValue (id); };
    p.gate = P (ids::gate);       p.dry = P (ids::dry);         p.retrig = P (ids::retrig);
    p.sens = P (ids::sens);
    p.octave = P (ids::octave);   p.tune = P (ids::tune);       p.glide = P (ids::glide);
    p.glideOn = P (ids::glideOn); p.volume = P (ids::volume);
    p.o1Oct = P (ids::o1Oct);     p.o1Wave = P (ids::o1Wave);   p.o1Semi = P (ids::o1Semi);
    p.o1Det = P (ids::o1Det);     p.o1Lvl = P (ids::o1Lvl);
    p.o2Oct = P (ids::o2Oct);     p.o2Wave = P (ids::o2Wave);   p.o2Semi = P (ids::o2Semi);
    p.o2Det = P (ids::o2Det);     p.o2Lvl = P (ids::o2Lvl);
    p.sync = P (ids::sync);       p.sub = P (ids::sub);         p.noise = P (ids::noise);
    p.cutoff = P (ids::cutoff);   p.res = P (ids::res);         p.keyTrk = P (ids::keyTrk);
    p.fEnvAmt = P (ids::fEnvAmt); p.drive = P (ids::drive);
    p.fA = P (ids::fA); p.fD = P (ids::fD); p.fS = P (ids::fS); p.fR = P (ids::fR);
    p.aA = P (ids::aA); p.aD = P (ids::aD); p.aS = P (ids::aS); p.aR = P (ids::aR);
    p.lfoRate = P (ids::lfoRate); p.modSrc = P (ids::modSrc);
    p.modDst = P (ids::modDst);   p.modAmt = P (ids::modAmt);
}

bool PhatTrackAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (in == out)
        return true;
    return in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo();   // mono guitar track, stereo synth
}

void PhatTrackAudioProcessor::resetState()
{
    tracker.reset();
    gateTrigger.reset();
    voice.reset();
    rawMidi = slowMidi = smoothMidi = glideMidi = 60.0f;
    lastClarity = 0.0f; lastHz = 0.0f;
    haveRaw = false; pitchReady = false; needSnap = true;
    samplesSinceResult = 1000000;
    jumpCooldown = 0;
    validGain = 0.0f; dryGate = 0.0f;
    detectedHz.store (0.0f);
    trackingConfidence.store (0.0f);
    playedMidi.store (-1.0f);
    gateOpenFlag.store (0.0f);
}

void PhatTrackAudioProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    tracker.prepare (sr);
    gateTrigger.prepare (sr);
    voice.prepare (sr);

    smoothCoefSlow = phat::coefFromSeconds (0.003f, sr);   // calm pitch smoothing for steady notes
    smoothCoefFast = phat::coefFromSeconds (0.0005f, sr);  // fast smoothing while bending / diving
    slowFollowCoef = phat::coefFromSeconds (0.005f, sr);   // reference for note-jump detection
    validCoef      = phat::coefFromSeconds (0.001f, sr);
    dryAtk         = phat::coefFromSeconds (0.002f, sr);
    dryRel         = phat::coefFromSeconds (0.040f, sr);

    resetState();
    setLatencySamples (0);   // no look-ahead: the plugin adds no latency of its own
}

void PhatTrackAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);
    if (numIn < 1 || buffer.getNumChannels() < 1)
        return;

    // ---- parameters (once per block) ----
    gateTrigger.setThresholdDb (p.gate->load());

    phat::VoiceParams vp;
    vp.osc1Oct = (int) p.o1Oct->load();      vp.osc2Oct = (int) p.o2Oct->load();
    vp.osc1Semi = p.o1Semi->load();          vp.osc2Semi = p.o2Semi->load();
    vp.osc1Detune = p.o1Det->load();         vp.osc2Detune = p.o2Det->load();
    vp.osc1Wave = p.o1Wave->load();          vp.osc2Wave = p.o2Wave->load();
    vp.osc1Level = p.o1Lvl->load();          vp.osc2Level = p.o2Lvl->load();
    vp.sub = p.sub->load();                  vp.noise = p.noise->load();
    vp.sync = p.sync->load() > 0.5f;
    vp.cutoff = p.cutoff->load();            vp.resonance = p.res->load();
    vp.keyTrack = p.keyTrk->load();          vp.filterEnvAmt = p.fEnvAmt->load();
    vp.drive = p.drive->load();
    vp.fA = p.fA->load(); vp.fD = p.fD->load(); vp.fS = p.fS->load(); vp.fR = p.fR->load();
    vp.aA = p.aA->load(); vp.aD = p.aD->load(); vp.aS = p.aS->load(); vp.aR = p.aR->load();
    vp.lfoRate = p.lfoRate->load();
    vp.modSource = (int) p.modSrc->load();   vp.modDest = (int) p.modDst->load();
    vp.modAmount = p.modAmt->load();
    vp.fineTune = p.tune->load();
    vp.octave = (int) p.octave->load() - 2;
    vp.volume = p.volume->load();
    voice.setParams (vp);

    const bool retrig = p.retrig->load() > 0.5f;
    const float sensGain = phat::dbToGain (p.sens->load());   // boosts / trims what the gate and tracker hear
    const bool stereoIn = numIn > 1;
    const float dryLevel = p.dry->load();
    const bool glideOn = p.glideOn->load() > 0.5f;
    const float glideTime = p.glide->load();
    const float glideCoef = (glideOn && glideTime > 0.001f) ? phat::coefFromSeconds (glideTime * 0.35f, sampleRate) : 1.0f;
    const int resultFreshSamples = (int) (0.03 * sampleRate);
    const int jumpCooldownSamples = (int) (0.04 * sampleRate);

    float* left = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    float peak = 0.0f;
    bool gateIsOpen = false;
    phat::PitchTracker::Result result;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = left[i];
        const float inR = stereoIn ? right[i] : inL;
        const float mono = stereoIn ? 0.5f * (inL + inR) : inL;
        const float detect = mono * sensGain;
        peak = std::max (peak, std::fabs (detect));

        // ---- gate / onset / tracker ----
        const auto g = gateTrigger.process (detect);
        gateIsOpen = g.open;
        if (samplesSinceResult < 1000000) ++samplesSinceResult;

        if (tracker.push (detect, g.trackerActive, result))
        {
            rawMidi = phat::hzToMidi (result.hz);
            lastHz = result.hz;
            lastClarity = result.clarity;
            haveRaw = true;
            samplesSinceResult = 0;
            if (! g.open || ! pitchReady)
                smoothMidi = glideMidi = slowMidi = rawMidi;   // no smoothing between notes
            if (g.open)
                pitchReady = true;
        }

        if (g.opened)
        {
            pitchReady = haveRaw && samplesSinceResult < resultFreshSamples;
            jumpCooldown = jumpCooldownSamples;
            voice.noteOn();
        }
        else if (g.onset && retrig)
        {
            voice.noteOn();   // fresh pluck while a note is ringing
        }
        if (g.closed)
            voice.noteOff();

        // ---- discrete note change (hammer-on / pull-off / new string) ----
        slowMidi += slowFollowCoef * (rawMidi - slowMidi);
        if (jumpCooldown > 0)
        {
            --jumpCooldown;
        }
        else if (g.open && pitchReady && std::fabs (rawMidi - slowMidi) > 0.6f)
        {
            slowMidi = rawMidi;
            jumpCooldown = jumpCooldownSamples;
            if (retrig) voice.noteOn();
        }

        // ---- pitch smoothing: calm for steady notes, quick for bends / dives ----
        const float err = rawMidi - smoothMidi;
        const float aerr = std::fabs (err);
        if (aerr > 1.5f)
        {
            smoothMidi = rawMidi;
        }
        else
        {
            const float t = phat::clampf ((aerr - 0.03f) / 0.30f, 0.0f, 1.0f);
            smoothMidi += (smoothCoefSlow + (smoothCoefFast - smoothCoefSlow) * t) * err;
        }
        glideMidi += glideCoef * (smoothMidi - glideMidi);

        // ---- synth + dry ----
        const float validTarget = (pitchReady && haveRaw) ? 1.0f : 0.0f;
        validGain += validCoef * (validTarget - validGain);
        dryGate += (g.open ? dryAtk : dryRel) * ((g.open ? 1.0f : 0.0f) - dryGate);

        const float synth = voice.tick (glideMidi) * validGain;
        const float dryAmt = dryLevel * dryGate;
        left[i] = synth + inL * dryAmt;
        if (right != nullptr)
            right[i] = synth + inR * dryAmt;
    }

    // ---- values for the editor ----
    inputLevelDb.store (juce::Decibels::gainToDecibels (peak, -100.0f));
    trackingConfidence.store (lastClarity);
    gateOpenFlag.store (gateIsOpen ? 1.0f : 0.0f);
    detectedHz.store ((gateIsOpen && pitchReady) ? lastHz : 0.0f);
    playedMidi.store ((gateIsOpen && pitchReady) ? rawMidi : -1.0f);
}

//==============================================================================
juce::AudioProcessorEditor* PhatTrackAudioProcessor::createEditor()
{
    return new PhatTrackAudioProcessorEditor (*this);
}

void PhatTrackAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PhatTrackAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhatTrackAudioProcessor();
}
