#pragma once
// PhatCore.h -- plain C++ DSP engine for PhatTrack (no JUCE dependency).
// Contains: pitch tracker, noise gate / onset detector, envelopes, oscillators,
// 4-pole ladder filter and the complete synth voice.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace phat
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float midiToHz (float m)  { return 440.0f * std::exp2 ((m - 69.0f) / 12.0f); }
inline float hzToMidi (float hz) { return 69.0f + 12.0f * std::log2 (hz / 440.0f); }
inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

// One-pole smoothing coefficient for a time constant in seconds.
inline float coefFromSeconds (float seconds, double sampleRate)
{
    if (seconds <= 1.0e-6f) return 1.0f;
    return 1.0f - std::exp (-1.0f / (seconds * (float) sampleRate));
}

//==============================================================================
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f, z1 = 0.0f, z2 = 0.0f;

    void setLowpass (double sr, double fc, double q)
    {
        const double w0 = 2.0 * 3.14159265358979323846 * fc / sr;
        const double cs = std::cos (w0), sn = std::sin (w0);
        const double alpha = sn / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (float) (((1.0 - cs) * 0.5) / a0);
        b1 = (float) ((1.0 - cs) / a0);
        b2 = b0;
        a1 = (float) ((-2.0 * cs) / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0.0f; }
};

//==============================================================================
// Pitch tracker: decimates to ~24 kHz, then runs a McLeod-style NSDF search.
//  * "free" mode scans geometric lag bands from high pitch to low, each with a window
//    sized for its longest lag, so high notes lock after ~2 periods while low notes
//    get the longer window they physically need.
//  * "locked" mode only searches +/-15% around the last period, every ~0.5 ms,
//    which follows bends, whammy dives and vibrato with very little lag.
class PitchTracker
{
public:
    struct Result { float hz = 0.0f; float clarity = 0.0f; };

    void prepare (double sr)
    {
        decim = sr >= 32000.0 ? std::max (1, (int) std::lround (sr / 24000.0)) : 1;
        fsd = (float) (sr / (double) decim);
        if (decim > 1)
        {
            lp1.setLowpass (sr, 5000.0, 0.5412);
            lp2.setLowpass (sr, 5000.0, 1.3066);
        }
        hpCoef = 1.0f - kTwoPi * 25.0f / fsd;
        minLag = std::max (4, (int) std::floor (fsd / 1400.0f));
        l1 = (int) std::lround (fsd / 270.0f);
        l2 = (int) std::lround (fsd / 133.0f);
        maxLag = (int) std::floor (fsd / 58.0f);
        hopLocked = std::max (6, (int) (fsd * 0.0005f));
        hopFree = std::max (8, (int) (fsd * 0.001f));
        reset();
    }

    void reset()
    {
        std::fill (ring, ring + kRing, 0.0f);
        writePos = 0; filled = 0; sinceHop = 0; decimCount = 0;
        locked = false; failCount = 0; wasActive = false; freeFails = 0;
        hpX1 = hpY1 = 0.0f;
        lp1.reset(); lp2.reset();
    }

    bool isLocked() const { return locked; }

    // Push one input-rate sample. Returns true when a new pitch estimate is in 'out'.
    bool push (float x, bool active, Result& out)
    {
        float v = x;
        if (decim > 1)
        {
            v = lp2.process (lp1.process (v));
            if (++decimCount < decim) return false;
            decimCount = 0;
        }
        const float hp = v - hpX1 + hpCoef * hpY1;
        hpX1 = v; hpY1 = hp;
        ring[writePos & kMask] = hp;
        ++writePos;
        if (filled < kRing) ++filled;

        if (! active)
        {
            if (wasActive) { locked = false; failCount = 0; freeFails = 0; }
            wasActive = false;
            sinceHop = 0;
            return false;
        }
        wasActive = true;
        ++sinceHop;
        const int hop = locked ? hopLocked : (freeFails > 40 ? hopFree * 3 : hopFree);
        if (sinceHop < hop) return false;
        sinceHop = 0;
        if (locked) return searchLocked (out);
        const bool ok = searchFree (out);
        if (ok) freeFails = 0; else ++freeFails;
        return ok;
    }

private:
    static constexpr int kRing = 4096;
    static constexpr int kMask = kRing - 1;
    static constexpr int kMaxWin = 2048;

    float ring[kRing] {};
    float win[kMaxWin] {};
    double sq[kMaxWin + 1] {};
    float nsdfBuf[1024] {};

    Biquad lp1, lp2;
    int decim = 2;
    int decimCount = 0;
    float fsd = 24000.0f;
    float hpCoef = 0.99f, hpX1 = 0.0f, hpY1 = 0.0f;
    uint32_t writePos = 0;
    int filled = 0;
    int sinceHop = 0;
    int hopLocked = 12, hopFree = 24;
    int minLag = 17, l1 = 89, l2 = 180, maxLag = 413;
    bool locked = false, wasActive = false;
    int failCount = 0;
    int freeFails = 0;
    float lockedLag = 100.0f;

    void loadWindow (int W)
    {
        const uint32_t start = writePos - (uint32_t) W;
        for (int i = 0; i < W; ++i) win[i] = ring[(start + (uint32_t) i) & (uint32_t) kMask];
    }

    // Normalised square difference for lags t0..t1 (inclusive) over win[0..W-1].
    void computeNsdf (int W, int t0, int t1)
    {
        sq[0] = 0.0;
        for (int i = 0; i < W; ++i) sq[i + 1] = sq[i] + (double) win[i] * (double) win[i];
        for (int t = t0; t <= t1; ++t)
        {
            const int n = W - t;
            if (n <= 8) { nsdfBuf[t - t0] = 0.0f; continue; }
            const float* a = win;
            const float* b = win + t;
            float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;
            int i = 0;
            for (; i + 3 < n; i += 4)
            {
                s0 += a[i] * b[i];
                s1 += a[i + 1] * b[i + 1];
                s2 += a[i + 2] * b[i + 2];
                s3 += a[i + 3] * b[i + 3];
            }
            for (; i < n; ++i) s0 += a[i] * b[i];
            const double r = (double) s0 + (double) s1 + (double) s2 + (double) s3;
            const double m = sq[n] + (sq[W] - sq[t]);
            nsdfBuf[t - t0] = m > 1.0e-9 ? (float) (2.0 * r / m) : 0.0f;
        }
    }

    // NSDF value at a single lag for the window currently loaded (computeNsdf must have run for this W).
    float nsdfAtLag (int W, int t) const
    {
        const int n = W - t;
        if (t < 1 || n <= 8) return 1.0f;
        const float* a = win;
        const float* b = win + t;
        float s0 = 0.0f;
        for (int i = 0; i < n; ++i) s0 += a[i] * b[i];
        const double m = sq[n] + (sq[W] - sq[t]);
        return m > 1.0e-9 ? (float) (2.0 * (double) s0 / m) : 1.0f;
    }

    bool refinePeak (int t, int t0, float& lagOut, float& clarOut) const
    {
        const float y0 = nsdfBuf[t - 1 - t0], y1 = nsdfBuf[t - t0], y2 = nsdfBuf[t + 1 - t0];
        const float den = y0 - 2.0f * y1 + y2;
        float d = std::fabs (den) > 1.0e-9f ? 0.5f * (y0 - y2) / den : 0.0f;
        d = clampf (d, -1.0f, 1.0f);
        lagOut = (float) t + d;
        clarOut = y1;
        return lagOut > 1.0f;
    }

    // nsdfBuf must hold lags [lo-1 .. hi+1]. Picks a local maximum in [lo, hi].
    bool pickPeak (int lo, int hi, bool mpmRule, float fraction, float minClarity,
                   float& lagOut, float& clarOut) const
    {
        const int t0 = lo - 1;
        float best = -1.0f;
        for (int t = lo; t <= hi; ++t)
        {
            const float y = nsdfBuf[t - t0];
            if (y > nsdfBuf[t - 1 - t0] && y >= nsdfBuf[t + 1 - t0] && y > best) best = y;
        }
        if (best < minClarity) return false;
        const float level = mpmRule ? std::max (fraction * best, minClarity) : best;
        for (int t = lo; t <= hi; ++t)
        {
            const float y = nsdfBuf[t - t0];
            if (y > nsdfBuf[t - 1 - t0] && y >= nsdfBuf[t + 1 - t0] && y >= level)
                return refinePeak (t, t0, lagOut, clarOut);
        }
        return false;
    }

    bool searchLocked (Result& out)
    {
        const float L = lockedLag;
        const int lo = std::max (minLag, (int) std::floor (L * 0.85f));
        const int hi = std::min (maxLag + 8, (int) std::ceil (L * 1.15f));
        const int W = std::min (kMaxWin, std::max (64, (int) (2.05f * L) + 16));
        if (filled >= W && hi > lo + 2)
        {
            loadWindow (W);
            computeNsdf (W, lo - 1, hi + 1);
            float lag = 0.0f, clar = 0.0f;
            if (pickPeak (lo, hi, false, 1.0f, 0.55f, lag, clar))
            {
                lockedLag = lag;
                failCount = 0;
                out.hz = fsd / lag;
                out.clarity = clar;
                return true;
            }
        }
        if (++failCount >= 3) { locked = false; failCount = 0; }
        return false;
    }

    // Free search: geometric lag bands, each with a window sized for its longest lag,
    // scanned from high pitch to low.
    bool searchFree (Result& out)
    {
        int lo = minLag;
        while (lo < maxLag)
        {
            int hi = std::min (maxLag, (int) std::ceil ((float) lo * 1.38f));
            if (hi <= lo + 3) hi = std::min (maxLag, lo + 4);
            const int W = std::min (kMaxWin, 2 * hi + 20);
            if (filled >= W)
            {
                loadWindow (W);
                computeNsdf (W, lo - 1, hi + 1);
                float lag = 0.0f, clar = 0.0f;
                const float thr = hi > l2 ? 0.75f : 0.90f;
                if (pickPeak (lo, hi, true, 0.92f, thr, lag, clar))
                {
                    // A real period has a clear valley between lag 0 and the peak. A smooth,
                    // low-pitched waveform seen through a short window does not -- that is how
                    // the pluck transient used to cause brief false locks, so reject those.
                    float valley = 1.0f;
                    for (float frac : { 0.4f, 0.5f, 0.6f })
                        valley = std::min (valley, nsdfAtLag (W, std::max (1, (int) std::lround (lag * frac))));
                    if (clar - valley >= 0.2f)
                    {
                        locked = true; lockedLag = lag; failCount = 0;
                        out.hz = fsd / lag;
                        out.clarity = clar;
                        return true;
                    }
                }
            }
            lo = std::max (lo + 1, (int) ((float) hi * 0.92f));
        }
        return false;
    }
};

//==============================================================================
// Noise gate + onset detector. The gate opens fast, closes with hysteresis and a
// hold time (so decaying notes are not chopped), and reports "onset" events for
// fresh plucks while a note is already ringing.
class GateTrigger
{
public:
    struct Out
    {
        bool open = false, opened = false, closed = false, onset = false, trackerActive = false;
    };

    void prepare (double sr)
    {
        sampleRate = sr;
        dt = 1.0f / (float) sr;
        fastA = coefFromSeconds (0.0002f, sr);
        fastR = coefFromSeconds (0.015f, sr);
        slowA = coefFromSeconds (0.004f, sr);
        slowR = coefFromSeconds (0.080f, sr);
        reset();
    }
    void reset() { fast = slow = 0.0f; isOpen = false; closeTimer = 0.0f; cooldown = 0.0f; }
    void setThresholdDb (float db) { thrOpen = dbToGain (db); thrClose = thrOpen * 0.5f; }

    Out process (float x)
    {
        Out o;
        const float a = std::fabs (x);
        fast += (a > fast ? fastA : fastR) * (a - fast);
        slow += (a > slow ? slowA : slowR) * (a - slow);
        cooldown -= dt;

        if (! isOpen)
        {
            if (fast > thrOpen)
            {
                isOpen = true; o.opened = true; o.onset = true;
                closeTimer = 0.0f; cooldown = 0.07f;
            }
        }
        else
        {
            if (slow < thrClose)
            {
                closeTimer += dt;
                if (closeTimer >= 0.04f) { isOpen = false; o.closed = true; closeTimer = 0.0f; }
            }
            else closeTimer = 0.0f;

            if (cooldown <= 0.0f && fast > thrOpen * 1.5f && fast > 1.5f * slow)
            {
                o.onset = true;
                cooldown = 0.07f;
            }
        }
        o.open = isOpen;
        o.trackerActive = isOpen || slow > thrOpen * 0.3f;
        return o;
    }

    bool isGateOpen() const { return isOpen; }

private:
    double sampleRate = 48000.0;
    float dt = 1.0f / 48000.0f;
    float fastA = 0.1f, fastR = 0.01f, slowA = 0.01f, slowR = 0.001f;
    float fast = 0.0f, slow = 0.0f;
    float thrOpen = 0.003f, thrClose = 0.0015f;
    bool isOpen = false;
    float closeTimer = 0.0f, cooldown = 0.0f;
};

//==============================================================================
class Adsr
{
public:
    void prepare (double sr) { sampleRate = sr; setParams (0.01f, 0.2f, 0.7f, 0.2f); reset(); }
    void setParams (float a, float d, float s, float r)
    {
        sustain = clampf (s, 0.0f, 1.0f);
        attCoef = coefFromSeconds (std::max (a, 0.0005f) / 1.8f, sampleRate);
        decCoef = coefFromSeconds (std::max (d, 0.001f) / 4.6f, sampleRate);
        relCoef = coefFromSeconds (std::max (r, 0.002f) / 4.6f, sampleRate);
    }
    void trigger() { state = State::attack; }
    void release() { if (state != State::idle) state = State::release; }
    void reset() { state = State::idle; env = 0.0f; }
    float value() const { return env; }

    float tick()
    {
        switch (state)
        {
            case State::idle: break;
            case State::attack:
                env += (1.25f - env) * attCoef;
                if (env >= 1.0f) { env = 1.0f; state = State::decay; }
                break;
            case State::decay:
                env += (sustain - env) * decCoef;
                if (std::fabs (env - sustain) < 1.0e-4f) { env = sustain; state = State::sustain; }
                break;
            case State::sustain:
                env += (sustain - env) * decCoef;
                break;
            case State::release:
                env += (0.0f - env) * relCoef;
                if (env < 1.0e-5f) { env = 0.0f; state = State::idle; }
                break;
        }
        return env;
    }

private:
    enum class State { idle, attack, decay, sustain, release };
    State state = State::idle;
    double sampleRate = 48000.0;
    float env = 0.0f, sustain = 0.7f, attCoef = 0.01f, decCoef = 0.001f, relCoef = 0.001f;
};

//==============================================================================
// Zero-delay-feedback 4-pole low-pass ladder with tanh saturation in the loop.
class LadderFilter
{
public:
    void reset() { s1 = s2 = s3 = s4 = 0.0f; }

    // g = tan(pi * fc / fs), k = feedback amount 0..4 (4 = self-oscillation)
    float process (float u, float g, float k)
    {
        const float G = g / (1.0f + g);
        const float a = 1.0f - G;
        const float G2 = G * G, G3 = G2 * G, G4 = G2 * G2;
        const float S = a * (G3 * s1 + G2 * s2 + G * s3 + s4);
        const float y4lin = (G4 * u + S) / (1.0f + k * G4);
        const float x = std::tanh (u - k * y4lin);

        const float y1 = G * x + a * s1;   s1 = 2.0f * y1 - s1;
        const float y2 = G * y1 + a * s2;  s2 = 2.0f * y2 - s2;
        const float y3 = G * y2 + a * s3;  s3 = 2.0f * y3 - s3;
        const float y4 = G * y3 + a * s4;  s4 = 2.0f * y4 - s4;
        return y4;
    }

private:
    float s1 = 0.0f, s2 = 0.0f, s3 = 0.0f, s4 = 0.0f;
};

//==============================================================================
inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

inline float pulseWave (float t, float dt, float pw)
{
    float v = t < pw ? 1.0f : -1.0f;
    v += polyBlep (t, dt);
    float t2 = t - pw;
    if (t2 < 0.0f) t2 += 1.0f;
    v -= polyBlep (t2, dt);
    const float dc = 2.0f * pw - 1.0f;
    return (v - dc) / (1.0f + std::fabs (dc));
}

// Continuously variable wave: triangle -> saw -> square -> narrow pulse (w in 0..1)
inline float morphWave (float t, float dt, float w)
{
    w = clampf (w, 0.0f, 1.0f);
    dt = clampf (dt, 1.0e-5f, 0.49f);
    if (w < 0.34f)
    {
        const float a = w / 0.34f;
        const float tri = 1.0f - 4.0f * std::fabs (t - 0.5f);
        const float saw = 2.0f * t - 1.0f - polyBlep (t, dt);
        return tri * (1.0f - a) + saw * a;
    }
    if (w < 0.67f)
    {
        const float a = (w - 0.34f) / 0.33f;
        const float saw = 2.0f * t - 1.0f - polyBlep (t, dt);
        return saw * (1.0f - a) + pulseWave (t, dt, 0.5f) * a;
    }
    const float a = (w - 0.67f) / 0.33f;
    return pulseWave (t, dt, 0.5f - 0.42f * a);
}

//==============================================================================
struct VoiceParams
{
    int   osc1Oct = 1, osc2Oct = 1;          // 0..3 = 16', 8', 4', 2'
    float osc1Semi = 0.0f, osc2Semi = 0.0f;  // -24..+24 semitones
    float osc1Detune = 0.0f, osc2Detune = 7.0f; // cents
    float osc1Wave = 0.34f, osc2Wave = 0.34f;   // 0..1 triangle..saw..square..pulse
    float osc1Level = 0.7f, osc2Level = 0.5f, sub = 0.2f, noise = 0.0f;
    bool  sync = false;
    float cutoff = 2400.0f, resonance = 0.25f, keyTrack = 0.5f, filterEnvAmt = 0.45f, drive = 0.2f;
    float fA = 0.015f, fD = 0.35f, fS = 0.35f, fR = 0.25f;
    float aA = 0.008f, aD = 0.3f, aS = 0.8f, aR = 0.2f;
    float lfoRate = 5.0f; int modSource = 0, modDest = 0; float modAmount = 0.0f;
    float fineTune = 0.0f;   // cents
    int   octave = 0;        // -2..+2
    float volume = 0.8f;
};

class Voice
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        smoothCoef = coefFromSeconds (0.005f, sr);
        ampEg.prepare (sr);
        filtEg.prepare (sr);
        reset();
        firstParams = true;
    }

    void reset()
    {
        phase1 = phase2 = subPhase = lfoPhase = 0.0f;
        ladder.reset();
        ampEg.reset(); filtEg.reset();
        lastOsc2 = 0.0f; shValue = 0.0f;
        pink0 = pink1 = pink2 = pink3 = pink4 = pink5 = pink6 = 0.0f;
    }

    void setParams (const VoiceParams& np)
    {
        p = np;
        ampEg.setParams (p.aA, p.aD, p.aS, p.aR);
        filtEg.setParams (p.fA, p.fD, p.fS, p.fR);
        const float vals[kNumSmoothed] = {
            std::log2 (clampf (p.cutoff, 16.0f, 20000.0f)), p.resonance, p.osc1Level, p.osc2Level,
            p.sub, p.noise, p.drive, p.filterEnvAmt, p.keyTrack, p.modAmount, p.volume,
            p.osc1Wave, p.osc2Wave, p.osc1Semi + p.osc1Detune * 0.01f, p.osc2Semi + p.osc2Detune * 0.01f,
            p.fineTune * 0.01f };
        for (int i = 0; i < kNumSmoothed; ++i)
        {
            sm[i].target = vals[i];
            if (firstParams) sm[i].cur = vals[i];
        }
        firstParams = false;
    }

    void noteOn()  { ampEg.trigger(); filtEg.trigger(); }
    void noteOff() { ampEg.release(); filtEg.release(); }
    float ampLevel() const { return ampEg.value(); }

    // midiNote: tracked guitar pitch after smoothing / glide.
    float tick (float midiNote)
    {
        for (int i = 0; i < kNumSmoothed; ++i) sm[i].cur += smoothCoef * (sm[i].target - sm[i].cur);
        const float logCut = sm[0].cur, res = sm[1].cur, lvl1 = sm[2].cur, lvl2 = sm[3].cur;
        const float subLvl = sm[4].cur, noiseLvl = sm[5].cur, drv = sm[6].cur, envAmt = sm[7].cur;
        const float kt = sm[8].cur, modAmt = sm[9].cur, vol = sm[10].cur;
        const float wave1 = sm[11].cur, wave2 = sm[12].cur;
        const float tune1 = sm[13].cur, tune2 = sm[14].cur, fine = sm[15].cur;

        // modulation sources
        lfoPhase += p.lfoRate / (float) sampleRate;
        if (lfoPhase >= 1.0f) { lfoPhase -= 1.0f; shValue = nextRandom(); }
        const float fenv = filtEg.tick();
        const float aenv = ampEg.tick();
        const float white = nextRandom();
        float mod = 0.0f;
        switch (p.modSource)
        {
            case 0: mod = 1.0f - 4.0f * std::fabs (lfoPhase - 0.5f); break;     // triangle
            case 1: mod = lfoPhase < 0.5f ? 1.0f : -1.0f; break;                  // square
            case 2: mod = 2.0f * lfoPhase - 1.0f; break;                          // saw (rising)
            case 3: mod = 1.0f - 2.0f * lfoPhase; break;                          // ramp (falling)
            case 4: mod = fenv; break;                                            // filter envelope
            case 5: mod = lastOsc2; break;                                        // oscillator 2
            case 6: mod = shValue; break;                                         // sample & hold
            default: mod = white; break;                                          // noise
        }
        float pitchMod = 0.0f, osc2Mod = 0.0f, filtModOct = 0.0f, waveMod = 0.0f;
        switch (p.modDest)
        {
            case 0: pitchMod = modAmt * modAmt * 7.0f * mod; break;
            case 1: osc2Mod  = modAmt * modAmt * 7.0f * mod; break;
            case 2: filtModOct = modAmt * 4.0f * mod; break;
            default: waveMod = modAmt * 0.5f * mod; break;
        }

        // oscillators
        static const float octOffset[4] = { -12.0f, 0.0f, 12.0f, 24.0f };
        const float base = midiNote + 12.0f * (float) p.octave + fine + pitchMod;
        const float m1 = base + octOffset[clampi (p.osc1Oct, 0, 3)] + tune1;
        const float m2 = base + octOffset[clampi (p.osc2Oct, 0, 3)] + tune2 + osc2Mod;
        const float nyq = 0.45f * (float) sampleRate;
        const float f1 = clampf (midiToHz (m1), 8.0f, nyq);
        const float f2 = clampf (midiToHz (m2), 8.0f, nyq);
        const float inc1 = f1 / (float) sampleRate;
        const float inc2 = f2 / (float) sampleRate;

        phase1 += inc1;
        bool wrapped = false;
        if (phase1 >= 1.0f) { phase1 -= 1.0f; wrapped = true; }
        phase2 += inc2;
        if (phase2 >= 1.0f) phase2 -= 1.0f;
        if (p.sync && wrapped) phase2 = clampf (inc2 * (phase1 / inc1), 0.0f, 0.999f);
        subPhase += inc1 * 0.5f;
        if (subPhase >= 1.0f) subPhase -= 1.0f;

        const float o1 = morphWave (phase1, inc1, wave1 + waveMod);
        const float o2 = morphWave (phase2, inc2, wave2 + waveMod);
        lastOsc2 = o2;
        const float sub = pulseWave (subPhase, inc1 * 0.5f, 0.5f);

        // pink-ish noise
        pink0 = 0.99886f * pink0 + white * 0.0555179f;
        pink1 = 0.99332f * pink1 + white * 0.0750759f;
        pink2 = 0.96900f * pink2 + white * 0.1538520f;
        pink3 = 0.86650f * pink3 + white * 0.3104856f;
        pink4 = 0.55000f * pink4 + white * 0.5329522f;
        pink5 = -0.7616f * pink5 - white * 0.0168980f;
        const float pink = (pink0 + pink1 + pink2 + pink3 + pink4 + pink5 + pink6 + white * 0.5362f) * 0.11f;
        pink6 = white * 0.115926f;

        // mixer + drive ("overload")
        const float mix = 0.55f * (o1 * lvl1 + o2 * lvl2 + sub * subLvl + pink * noiseLvl);
        const float x = mix * (1.0f + 7.0f * drv);
        const float soft = std::tanh (x);
        const float hard = clampf (x, -1.0f, 1.0f);
        const float driven = (soft + (hard - soft) * (0.6f * drv)) * 0.9f;

        // filter
        const float kbOct = (midiNote - 60.0f) / 12.0f * (2.0f * kt);
        const float fcOct = logCut + kbOct + envAmt * 5.0f * fenv + filtModOct;
        const float fc = clampf (std::exp2 (fcOct), 16.0f, nyq);
        const float g = std::tan (kPi * fc / (float) sampleRate);
        const float k = clampf (res, 0.0f, 1.0f) * 4.0f;
        float y = ladder.process (driven, g, k);
        y *= 1.0f + 0.5f * res;

        return std::tanh (y * aenv * vol);
    }

private:
    static constexpr int kNumSmoothed = 16;
    struct Smoothed { float cur = 0.0f, target = 0.0f; };

    static int clampi (int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
    float nextRandom()
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (int32_t) rng * (1.0f / 2147483648.0f);
    }

    VoiceParams p;
    Smoothed sm[kNumSmoothed];
    bool firstParams = true;
    double sampleRate = 48000.0;
    float smoothCoef = 0.01f;
    float phase1 = 0.0f, phase2 = 0.0f, subPhase = 0.0f, lfoPhase = 0.0f;
    float lastOsc2 = 0.0f, shValue = 0.0f;
    float pink0 = 0, pink1 = 0, pink2 = 0, pink3 = 0, pink4 = 0, pink5 = 0, pink6 = 0;
    uint32_t rng = 0x12345678u;
    Adsr ampEg, filtEg;
    LadderFilter ladder;
};

} // namespace phat
