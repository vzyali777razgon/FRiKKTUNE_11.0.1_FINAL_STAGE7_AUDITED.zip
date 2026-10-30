# FRiKKTUNE validation plan

## F0 estimator
- Sine sweeps: 70–1000 Hz at -36, -18, -6 dBFS.
- Harmonic stacks with varying 2nd/3rd harmonic levels.
- Add white/pink noise at several SNRs.
- Vibrato: ±20–100 cents at 4–8 Hz.
- Portamento and semitone/octave transitions.
- Unvoiced consonants, breath and silence.
- Metrics: voiced recall/false-positive rate, median absolute cents error, octave-error rate, confidence calibration.

## Correction engine (not implemented)
- Render same mono vocal through Natural and Hard Tune.
- Measure target-note error, attack-to-target time, pitch variance, latency, CPU.
- ABX and loudness-matched listening for transients, phase/metallic artifacts, formants and vibrato.
- Compare against licensed/current Antares and Waves versions with identical input, key/scale and settings.
- Do not claim superiority without repeatable objective and listening results.


## Note target / trajectory
- Verify nearest note selection in every key and major/minor scale.
- Verify custom disabled-note masks across octave boundaries.
- Sweep slowly across semitone boundaries to check target hysteresis.
- Step between notes and measure correction-envelope settling for Natural and Hard Tune.
- Verify no state jumps after silence or low-confidence regions.

## Phase-vocoder synthetic audio checks (0.4.10)

Run with `-DFRIKKTUNE_BUILD_PLUGIN=OFF -DFRIKKTUNE_BUILD_SHIFTER_TESTS=ON`. The standalone target uses `Tests/juce_stub/JuceHeader.h` only for algorithm-level tests when JUCE is unavailable. It checks unity ratio, octave shifts, silence, ratio transitions, finite output, and identical stereo channels. Repeat the tests in a build using actual JUCE and add real-vocal/DAW listening tests before release.
