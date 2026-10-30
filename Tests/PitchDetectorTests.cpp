#include "../Source/PitchDetector.h"
#include "../Source/NoteQuantizer.h"
#include "../Source/PitchJumpGate.h"
#include "../Source/NoteTargetTracker.h"
#include <cmath>
#include <iostream>
#include <vector>

static bool testSine(double sr, double hz, float amplitude)
{
    PitchDetector d;
    d.prepare(sr, 60.0f, 1100.0f);
    constexpr double pi = 3.14159265358979323846;
    const int total = sr > 48000.0 ? PitchDetector::capacity : PitchDetector::capacity / 2;
    for (int i = 0; i < total; ++i)
        d.push(amplitude * (float)std::sin(2.0 * pi * hz * i / sr));
    const auto r = d.analyse();
    const float cents = r.hz > 0.0f ? 1200.0f * std::log2(r.hz / hz) : 9999.0f;
    std::cout << "sine sr=" << sr << " target=" << hz << " detected=" << r.hz
              << " confidence=" << r.confidence << " cents=" << cents << '\n';
    return r.voiced && std::abs(cents) < 35.0f;
}

static bool testHarmonicStack()
{
    PitchDetector d;
    const double sr = 48000.0, f0 = 146.832; // D3
    d.prepare(sr, 60.0f, 1100.0f);
    constexpr double pi = 3.14159265358979323846;
    for (int i = 0; i < 4096; ++i)
    {
        const double t = i / sr;
        // Deliberately stronger upper harmonics than fundamental.
        const float x = (float)(0.08 * std::sin(2*pi*f0*t)
                     + 0.22 * std::sin(2*pi*2*f0*t + 0.2)
                     + 0.18 * std::sin(2*pi*3*f0*t + 0.4)
                     + 0.08 * std::sin(2*pi*4*f0*t + 0.1));
        d.push(x);
    }
    const auto r = d.analyse();
    const float cents = r.hz > 0.0f ? 1200.0f * std::log2(r.hz / f0) : 9999.0f;
    std::cout << "harmonic-stack target=" << f0 << " detected=" << r.hz
              << " confidence=" << r.confidence << " cents=" << cents << '\n';
    return r.voiced && std::abs(cents) < 35.0f;
}

static bool testVibrato()
{
    PitchDetector d;
    const double sr = 48000.0, f0 = 220.0;
    d.prepare(sr, 60.0f, 1100.0f);
    constexpr double pi = 3.14159265358979323846;
    double phase = 0.0;
    for (int i = 0; i < 4096; ++i)
    {
        const double t = i / sr;
        const double hz = f0 * std::pow(2.0, (35.0 * std::sin(2*pi*5.0*t)) / 1200.0);
        phase += 2*pi*hz/sr;
        d.push((float)(0.2 * std::sin(phase)));
    }
    const auto r = d.analyse();
    std::cout << "vibrato detected=" << r.hz << " confidence=" << r.confidence << '\n';
    return r.voiced && r.hz > 200.0f && r.hz < 240.0f;
}


static bool testNoisyBass()
{
    PitchDetector d;
    const double sr = 48000.0, f0 = 55.0;
    d.prepare(sr, 45.0f, 400.0f);
    constexpr double pi = 3.14159265358979323846;
    unsigned state = 0x12345678u;
    for (int i = 0; i < 4096; ++i)
    {
        state = 1664525u * state + 1013904223u;
        const float noise = ((float)((state >> 8) & 0xffffu) / 32767.5f - 1.0f) * 0.025f;
        const float x = 0.2f * (float)std::sin(2.0 * pi * f0 * i / sr) + noise;
        d.push(x);
    }
    const auto r = d.analyse();
    const float cents = r.hz > 0.0f ? 1200.0f * std::log2(r.hz / f0) : 9999.0f;
    std::cout << "noisy-bass target=" << f0 << " detected=" << r.hz
              << " confidence=" << r.confidence << " cents=" << cents << '\n';
    return r.voiced && std::abs(cents) < 40.0f;
}


static bool testHighSampleRateBass()
{
    PitchDetector d;
    const double sr = 96000.0, f0 = 55.0;
    d.prepare(sr, 45.0f, 400.0f);
    constexpr double pi = 3.14159265358979323846;
    for (int i = 0; i < PitchDetector::capacity; ++i)
        d.push(0.2f * (float)std::sin(2.0 * pi * f0 * i / sr));
    const auto r = d.analyse();
    const float cents = r.hz > 0.0f ? 1200.0f * std::log2(r.hz / f0) : 9999.0f;
    std::cout << "96kHz bass target=" << f0 << " detected=" << r.hz
              << " confidence=" << r.confidence << " cents=" << cents << '\n';
    return r.voiced && std::abs(cents) < 35.0f;
}

static bool testDcOffset()
{
    PitchDetector d;
    const double sr = 48000.0, f0 = 196.0;
    d.prepare(sr, 60.0f, 1000.0f);
    constexpr double pi = 3.14159265358979323846;
    for (int i = 0; i < 4096; ++i)
        d.push(0.12f + 0.2f * (float)std::sin(2.0 * pi * f0 * i / sr));
    const auto r = d.analyse();
    const float cents = r.hz > 0.0f ? 1200.0f * std::log2(r.hz / f0) : 9999.0f;
    std::cout << "dc-offset target=" << f0 << " detected=" << r.hz << " cents=" << cents << '\n';
    return r.voiced && std::abs(cents) < 35.0f;
}

int main()
{
    int failures = 0;
    failures += !testSine(44100.0, 110.0, 0.2f);
    failures += !testSine(48000.0, 220.0, 0.2f);
    failures += !testSine(96000.0, 440.0, 0.2f);
    failures += !testSine(48000.0, 880.0, 0.001f);
    failures += !testHarmonicStack();
    failures += !testVibrato();
    failures += !testNoisyBass();
    failures += !testHighSampleRateBass();
    failures += !testDcOffset();
    PitchDetector silence;
    silence.prepare(48000.0);
    for (int i=0; i<4096; ++i) silence.push(0.0f);
    const auto s = silence.analyse();
    std::cout << "silence voiced=" << s.voiced << " confidence=" << s.confidence << '\n';
    failures += s.voiced;
    std::array<bool, 12> enabled; enabled.fill(true);
    const int majorC = NoteQuantizer::nearestAllowedMidi(61.0f, 0, NoteQuantizer::Scale::Major, enabled);
    const int minorA = NoteQuantizer::nearestAllowedMidi(68.0f, 9, NoteQuantizer::Scale::Minor, enabled);
    std::cout << "quantizer C major midi61 -> " << majorC << " (expected 60 or 62)\\n";
    std::cout << "quantizer A minor midi68 -> " << minorA << " (expected 69 or 67)\\n";
    failures += !(majorC == 60 || majorC == 62);
    failures += !(minorA == 69 || minorA == 67);
    enabled[0] = false;
    const int custom = NoteQuantizer::nearestAllowedMidi(60.0f, 0, NoteQuantizer::Scale::Chromatic, enabled);
    std::cout << "custom disabled C -> " << custom << " (must not be C)\\n";
    failures += ((custom % 12 + 12) % 12) == 0;
    const float fast = NoteQuantizer::smoothCents(0.0f, 100.0f, 1.0f, 48000.0, 1024);
    const float slow = NoteQuantizer::smoothCents(0.0f, 100.0f, 80.0f, 48000.0, 1024);
    std::cout << "retune response fast=" << fast << " slow=" << slow << "\n";
    failures += !(fast > slow && fast <= 100.0f && slow > 0.0f);
    // Absolute tuning-reference values must map the selected A4 frequency to MIDI 69.
    for (const float tuningHz : {440.0f, 445.0f, 450.0f})
    {
        const float midiA4 = NoteQuantizer::frequencyToMidi(tuningHz, tuningHz);
        const float roundTrip = NoteQuantizer::midiToFrequency(69.0f, tuningHz);
        std::cout << "tuning reference=" << tuningHz << " Hz midiA4=" << midiA4
                  << " roundTrip=" << roundTrip << " Hz\n";
        failures += !(std::abs(midiA4 - 69.0f) < 1.0e-4f && std::abs(roundTrip - tuningHz) < 1.0e-3f);
    }
    failures += NoteQuantizer::frequencyToMidi(-1.0f, 440.0f) >= 0.0f;

    // A single low-confidence octave flip is rejected, but a consistent real leap
    // is accepted on confirmation rather than being blocked indefinitely.
    PitchJumpGate gate;
    const bool firstLeap = gate.accept(81.0f, 0.80f, 69.0f);
    const bool confirmedLeap = gate.accept(81.2f, 0.81f, 69.0f);
    const bool isolatedConfidentOctave = gate.accept(57.0f, 0.99f, 69.0f);
    const bool confirmedConfidentOctave = gate.accept(57.1f, 0.99f, 69.0f);
    const bool confidentNonOctaveLeap = gate.accept(79.0f, 0.99f, 69.0f);
    std::cout << "pitch jump gate first=" << firstLeap << " confirmed=" << confirmedLeap
              << " isolated-confident-octave=" << isolatedConfidentOctave
              << " confirmed-confident-octave=" << confirmedConfidentOctave
              << " confident-non-octave=" << confidentNonOctaveLeap << "\n";
    failures += firstLeap || !confirmedLeap || isolatedConfidentOctave
              || !confirmedConfidentOctave || !confidentNonOctaveLeap;
    gate.resetPending();
    failures += gate.accept(81.0f, 0.80f, 69.0f);

    // Note-target hysteresis: midpoint jitter must not chatter, while a sustained
    // move to the next note is accepted after the configured confirmation frames.
    NoteTargetTracker tracker;
    failures += tracker.update(60.10f, 60, 3) != 60;
    failures += tracker.update(60.54f, 61, 3) != 60;
    failures += tracker.update(60.47f, 60, 3) != 60;
    failures += tracker.update(60.56f, 61, 3) != 60;
    failures += tracker.update(60.80f, 61, 3) != 60;
    failures += tracker.update(60.82f, 61, 3) != 60;
    failures += tracker.update(60.84f, 61, 3) != 61;
    failures += tracker.update(60.50f, 60, 3) != 61;
    tracker.reset();
    failures += tracker.current() != -1;

    // A high-confidence, clearly nearer note transition can switch immediately
    // after the distance hysteresis is satisfied, while ordinary boundary
    // wobble continues to require confirmation.
    NoteTargetTracker fastTracker;
    failures += fastTracker.update(60.0f, 60, 3) != 60;
    failures += fastTracker.update(61.2f, 61, 3, 0.30f, true) != 61;
    failures += fastTracker.current() != 61;
    const float fineUp = NoteQuantizer::correctionTargetCents(69.0f, 69.0f, 1.0f, 25.0f);
    const float fineDown = NoteQuantizer::correctionTargetCents(69.0f, 69.0f, 1.0f, -25.0f);
    std::cout << "fine tune sign +25c=" << fineUp << " -25c=" << fineDown << "\n";
    failures += !(fineUp > 0.0f && fineDown < 0.0f);
    std::cout << (failures == 0 ? "PASS" : "FAIL") << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
