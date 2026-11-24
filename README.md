# PhatTrack — guitar-controlled analog monosynth (prototype 0.1)

PhatTrack is an original JUCE audio-effect plugin inspired by the *general sound and workflow* of classic compact analog monophonic synthesizers. It is not affiliated with Moog Music and does not use Moog trademarks, logos, panel artwork, or copied product graphics.

## What it does

- Insert it on a mono or stereo guitar audio track in Logic Pro.
- It analyzes the incoming guitar audio directly; no MIDI Guitar, MIDI controller, or external pitch-to-MIDI utility is required.
- A monophonic YIN-style pitch detector drives a two-oscillator synth voice.
- Includes oscillator waveshapes, sub-oscillator, detune, four-stage resonant low-pass approximation, filter envelope amount, amp envelope, glide, LFO vibrato, drive, input threshold, dry blend, and output level.
- AU, VST3, and Standalone targets are configured.

## Important prototype limitations

This is a first playable-code prototype, not a finished commercial-quality instrument. The pitch tracker is monophonic and performs best with a clean, single-note guitar signal. Chords, heavy distortion, strong octave harmonics, palm-muted transients, and noisy pickups can confuse pitch detection. The filter is a nonlinear four-stage ladder-style approximation, not a circuit-accurate model of any Moog instrument. There is currently no latency-compensated look-ahead or dedicated noise gate. Expect to tune the input threshold and use the neck pickup / cleanest practical input for best tracking.

The default sound is synth-only (Dry = 0). Raise Dry to blend the original guitar back in. Keep the Scarlett input from clipping; start with a moderate instrument-level input and disable direct monitoring if you only want to hear the processed plugin return.

## Build on macOS for Logic Pro

1. Install Xcode and the Command Line Tools (`xcode-select --install`), plus CMake (`brew install cmake`).
2. Run `./build_au.sh` in this folder. It clones JUCE to `~/JUCE` if you don't have it (or set `JUCE_PATH` to your own JUCE folder), builds the AU, installs it to `~/Library/Audio/Plug-Ins/Components/`, and runs `auval`.
3. Restart Logic Pro. If it doesn't show up, use Logic Pro > Settings > Plug-in Manager > Reset & Rescan Selection.
4. It appears under Audio FX > Open Circuit Audio > PhatTrack.

Manual build:

```bash
cmake -B build -S . -DJUCE_PATH="$HOME/JUCE" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target PhatTrack_AU
```

For a quick test without Logic, build the `PhatTrack_Standalone` target.

## First-use setup in Logic

1. Create an audio track and choose the input receiving your guitar from the Scarlett 2i2.
2. Insert **PhatTrack** as an Audio FX plugin.
3. Set the Scarlett input to Instrument/Hi-Z for a direct guitar connection. Avoid clipping.
4. Start with Sensitivity around -38 dB, Cutoff around 2.4 kHz, Resonance around 0.25, Drive around 0.2, and a short attack.
5. Play single notes cleanly. Watch the note/frequency readout. If it does not trigger, lower the threshold (more negative dB); if it triggers from noise, raise the threshold (less negative dB).
6. Use Glide for legato slides. Set Dry to zero for synth-only output.

## Design note

The UI uses an original dark hardware-panel layout, warm amber accents, rotary controls, and a small green status display as a broad visual homage to vintage analog monosynths. It intentionally avoids reproducing the Little Phatty's exact faceplate, typography, branding, or graphics.

## No Xcode? Build on GitHub

1. Create a free GitHub account and a new repository, then upload this whole folder (including the hidden `.github` folder).
2. Open the repo's **Actions** tab > **Build PhatTrack AU** > **Run workflow**.
3. When it finishes (about 5-10 min), download the `PhatTrack-AU` artifact and unzip it.
4. In Terminal, run: `xattr -dr com.apple.quarantine ~/Downloads/PhatTrack.component`
5. Move `PhatTrack.component` to `~/Library/Audio/Plug-Ins/Components/` and restart Logic.
