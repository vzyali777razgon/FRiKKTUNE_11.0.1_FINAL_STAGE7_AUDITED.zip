# FRiKKTUNE 10.0.0

FRiKKTUNE is an experimental JUCE-based vocal pitch-correction VST3 source project.

## Features in the source project
- Monophonic YIN-style pitch detection and scale-aware note quantization.
- Natural and hard-tune correction modes, retune speed, humanize control, A4 reference in Hz, formant-envelope compensation control, dry/wet, output gain and bypass.
- Streaming stereo-linked phase-vocoder pitch shifter with fixed declared latency.
- Standalone CMake regression tests for the pitch detector and pitch-shifter DSP.
- Regression coverage for non-finite input samples, invalid ratio values, and reset/restart behavior.

## Build standalone tests

```sh
cmake -S . -B build -DFRIKKTUNE_BUILD_PLUGIN=OFF -DFRIKKTUNE_BUILD_DETECTOR_TESTS=ON -DFRIKKTUNE_BUILD_SHIFTER_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## Build the VST3

The default CMake configuration fetches JUCE 8.0.6. A working compiler, CMake, Git/network access (or a local JUCE checkout configured by the developer), and the platform's audio/plugin SDK requirements are needed. Set `FRIKKTUNE_BUILD_PLUGIN=ON` and configure with JUCE available.

## Important status notice

This is **not yet a verified production-ready commercial plugin**. The standalone pitch-shifter tests compile against `Tests/juce_stub/JuceHeader.h`, a test FFT shim, not JUCE's production FFT implementation. The complete JUCE VST3 target, plugin validation in a DAW, real-vocal listening, and comparative listening against commercial pitch-correction software have not been verified in this environment. The formant control is spectral-envelope compensation, not a dedicated high-quality LPC formant-preserving engine. Use the report for the exact verification status.

## 0.4.18 — transient-aware phase reset

Added conservative spectral-flux detection to reset synthesis phase during abrupt spectral attacks, aiming to reduce smeared transients at non-unity pitch ratios. Added a transient-burst regression test. This is an experimental DSP refinement; subjective improvement and the full JUCE/VST3 path still require validation on real vocal material.


## 5.0.0 — octave-error rejection

Pitch-jump gating now treats octave-like jumps (12, 19, or 24 semitones, within 1.25 semitones) as a special risk even when the detector reports high confidence. Such a jump must be repeated consistently on the next analysis frame before the tracking state accepts it. High-confidence non-octave leaps still pass immediately. This targets a common vocal-correction failure mode: a strong second/third harmonic briefly being mistaken for the fundamental.

This is a source/test release, not a claim of production readiness. The full JUCE VST3 build, real-vocal listening, and comparison against commercial products remain unverified.


## 10.0.0 — detector cost and frame-statistics correction

The YIN pitch detector now analyzes a pair-averaged 2:1 decimated stream, reducing the quadratic difference-function work while retaining the supported vocal pitch range. The active analysis window is selected to maintain roughly 85–95 ms across common host sample rates. Mean and energy are now calculated only over active frame samples, fixing a fixed-buffer statistics error for smaller windows. See `ENGINEERING_REPORT_10.0.0.md` for test results and limitations.


## Version 10.0.8
LPC envelope blending now uses frame-level prediction-confidence gating to reduce the influence of unreliable LPC models. See `ENGINEERING_REPORT_10.0.8_LPC_CONFIDENCE.md` for scope, validation, and limitations.


## Version 10.0.9
The formant path now expresses spectral-envelope preservation as an explicit frequency-domain inverse-filter/re-filter approximation: remapped magnitudes are normalized by the source envelope and recoloured with the output-bin envelope, with floors, gain limits, log-domain temporal smoothing, and transient bypass. This is **not** a time-domain LPC residual/filter engine and is not claimed as production-grade formant preservation. Standalone test results and limitations are in `ENGINEERING_REPORT_10.0.9_SPECTRAL_INVERSE_REFILTER.md`.


## 10.1.0 — LPC envelope temporal stabilisation
Adds per-channel log-domain smoothing of the normalized LPC envelope and holds the prior LPC shape during transient frames. The LPC contribution remains confidence-gated and bounded. See `ENGINEERING_REPORT_10.1.0_LPC_TEMPORAL_STABILISATION.md` for scope and validation limits.


## 10.2.0 — Voiced/Unvoiced Adaptive Formant Control

Introduces smoothed autocorrelation-based voicedness gating for LPC envelope blending and formant compensation. See `ENGINEERING_REPORT_10.2.0_VOICED_UNVOICED_ADAPTIVE.md` for scope, test results, and limitations. Experimental; real-vocal and full VST3 validation remain outstanding.


## Version 10.3.0 — Synthetic Vocal Regression Corpus

Added deterministic stress coverage for a vowel-like harmonic signal, broadband frication, voiced/noisy transitions, and a pitch-ratio change. The checks cover finite output, peak/sample-step bounds, and segment RMS only; they are not perceptual formant-quality metrics. See `ENGINEERING_REPORT_10.3.0_SYNTHETIC_VOCAL_REGRESSION.md` for validation and limitations.


## 10.4.0 — DSP Benchmark Stage 1
Added a reproducible 12-case pure-sine pitch-shift benchmark covering 110/220/440 Hz inputs and ratios 0.75x/1.0x/1.25x/1.5x. It reports measured frequency, relative error, RMS, peak and non-finite sample count. The benchmark is a deterministic sanity check only; it is not a perceptual vocal-quality benchmark. See `ENGINEERING_REPORT_10.4.0_DSP_BENCHMARK.md` for exact results and limitations.


## 10.5.0 — Algorithm Refinement (Stage 2)

- Spectral remapping now interpolates magnitude in power space.
- Voicedness autocorrelation workload reduced using sparse lag/sample evaluation; still a heuristic control signal.
- Added harmonic-rich vibrato-like regression scenario.
- See `ENGINEERING_REPORT_10.5.0_ALGORITHM_REFINEMENT.md` for validation and limitations.
\n\n## 10.6.0 — Musical Transition Smoothing (Stage 3/7)\n\nAdaptive pitch-ratio smoothing uses a 3–12 ms time constant based on target interval size. Small moves remain responsive; larger moves glide more gently. This is experimental and requires real-vocal listening validation. See `ENGINEERING_REPORT_10.6.0_MUSICAL_TRANSITIONS_STAGE3.md`.\n

## Version 10.7.0 — UI and Parameter Workflow (Stage 4)

- Explicitly populate Key, Scale, Mode, and Input Range selectors from APVTS choice parameters.
- Add visible selector labels and helpful control tooltips.
- Keep existing parameter IDs and state model.
- See `ENGINEERING_REPORT_10.7.0_UI_PARAMETER_WORKFLOW_STAGE4.md` for validation and limitations.


## Version 10.9.0 — VST3 Build Pipeline (Stage 6/7)

- Project version advanced to 10.9.0.
- Added `BUILD_VST3.md` with build prerequisites and reproducible CMake commands.
- Attempted JUCE VST3 configure; blocked because this environment cannot resolve `github.com` to fetch JUCE 8.0.6.
- Standalone Release build and CTest still pass (2/2). No VST3 binary is included or claimed.
- See `ENGINEERING_REPORT_10.9.0_VST3_BUILD_STAGE6.md`.
\n\n## 11.0.0 — Final UI and handoff\n\nThe editor now uses a more structured dark dashboard layout with live note and signal telemetry, tuning controls, musical-context selectors, and styled bypass. Parameter IDs remain unchanged. See `ENGINEERING_REPORT_11.0.0_FINAL_HANDOFF.md` and `BUILD_VST3.md`. Standalone DSP tests pass (2/2); the VST3/editor target still needs to be built and verified in a JUCE-enabled environment.\n\n\n## 11.0.1 — Final handoff (Stage 7/7)\n\n- Final editor spacing pass: clearer separation between pitch/formant controls and musical-context selectors; section-heading overlap removed.\n- Kept the dark dashboard / teal accent style, live note display, confidence and input-level meters, and existing APVTS parameter IDs.\n- Standalone Release build and CTest pass 2/2. VST3/editor compilation and DAW visual/audio checks remain unverified in this environment.\n- See `ENGINEERING_REPORT_11.0.1_FINAL_HANDOFF_STAGE7.md` and `BUILD_VST3.md`. The ZIP is source/build instructions, not a compiled VST3 binary.\n