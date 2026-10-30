#include "../Source/PitchShifter.h"
#include <cmath>
#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <limits>

namespace {
constexpr double sr = 48000.0;
constexpr double pi = 3.14159265358979323846;
struct Result { double rms=0, peak=0, frequency=0, correlation=0, stereoDifference=0; int nonFinite=0; };
Result runTone(float ratio, double inputHz, float formant, bool stereo=false)
{
    FRiKKTUNEPitchShifter shifter;
    shifter.prepare(sr, stereo ? 2 : 1);
    shifter.setFormantPreservation(formant);
    constexpr int count = 48000;
    std::vector<float> in((size_t)count), out((size_t)count);
    std::vector<float> outR(stereo ? (size_t)count : 0);
    for (int i=0;i<count;++i) {
        in[(size_t)i]=(float)(0.5*std::sin(2*pi*inputHz*i/sr));
        float ins[2]{in[(size_t)i], in[(size_t)i]}; float outs[2]{};
        shifter.processSample(ins,outs,stereo?2:1,ratio);
        out[(size_t)i]=outs[0]; if(stereo) outR[(size_t)i]=outs[1];
    }
    Result r; double sum=0; int crossings=0; int prev=0; double corr=0, xx=0, yy=0;
    const int begin=8000;
    for(int i=begin;i<count;++i){ const float y=out[(size_t)i]; if(!std::isfinite(y)) ++r.nonFinite; sum+=(double)y*y; r.peak=std::max(r.peak,std::abs((double)y)); const int s=(y>0)-(y<0); if(s && prev && s!=prev) ++crossings; if(s) prev=s; const float x=in[(size_t)(i-FRiKKTUNEPitchShifter::fftSize)]; corr+=(double)x*y; xx+=(double)x*x; yy+=(double)y*y; }
    const int n=count-begin; r.rms=std::sqrt(sum/n); r.frequency=(double)crossings*sr/(2.0*n); r.correlation=(xx>0&&yy>0)?corr/std::sqrt(xx*yy):0;
    if(stereo){ double maxDiff=0; for(int i=begin;i<count;++i) maxDiff=std::max(maxDiff,std::abs((double)out[(size_t)i]-outR[(size_t)i])); r.stereoDifference=maxDiff; }
    return r;
}
bool expect(bool ok,const char* name){ std::cout<<(ok?"PASS ":"FAIL ")<<name<<"\n"; return ok; }

bool testRatioTransition()
{
    FRiKKTUNEPitchShifter shifter; shifter.prepare(sr, 1); shifter.setFormantPreservation(0.0f);
    constexpr int count=48000; float maxStep=0.0f, peak=0.0f, previous=0.0f; int nonFinite=0;
    for(int i=0;i<count;++i){ const float x=(float)(0.4*std::sin(2*pi*220.0*i/sr)); const float ratio=i<count/3?1.0f:(i<2*count/3?1.5f:0.75f); float in[1]{x},out[1]{}; shifter.processSample(in,out,1,ratio); if(!std::isfinite(out[0])) ++nonFinite; peak=std::max(peak,std::abs(out[0])); if(i>0) maxStep=std::max(maxStep,std::abs(out[0]-previous)); previous=out[0]; }
    std::cout<<"transition metrics peak="<<peak<<" max-step="<<maxStep<<" nonfinite="<<nonFinite<<"\n";
    bool ok=true; ok &= expect(nonFinite==0,"ratio transitions: no NaN/Inf"); ok &= expect(peak<2.0f,"ratio transitions: bounded output"); ok &= expect(maxStep<1.0f,"ratio transitions: no extreme single-sample discontinuity"); return ok;
}

bool testTransientBurstStability()
{
    FRiKKTUNEPitchShifter shifter;
    shifter.prepare(sr, 1);
    shifter.setFormantPreservation(0.0f);
    constexpr int count = 24000;
    int nonFinite = 0;
    float peak = 0.0f;
    double postBurstEnergy = 0.0;
    int postBurstCount = 0;
    for (int i = 0; i < count; ++i) {
        // Quiet voiced lead-in followed by a sharp amplitude attack, approximating
        // a plosive/onset. The test checks stability, not subjective transient quality.
        const double amp = i < 6000 ? 0.03 : 0.45;
        const float x = (float)(amp * std::sin(2.0 * pi * 220.0 * i / sr));
        float input[1] = { x }, output[1] = {};
        shifter.processSample(input, output, 1, 1.5f);
        if (!std::isfinite(output[0])) ++nonFinite;
        peak = std::max(peak, std::abs(output[0]));
        if (i > 10000) { postBurstEnergy += (double)output[0] * output[0]; ++postBurstCount; }
    }
    const double rms = postBurstCount > 0 ? std::sqrt(postBurstEnergy / postBurstCount) : 0.0;
    bool ok = true;
    ok &= expect(nonFinite == 0, "transient burst: no NaN/Inf");
    ok &= expect(peak < 2.0f, "transient burst: bounded peak");
    ok &= expect(rms > 0.03, "transient burst: sustained output remains audible");
    return ok;
}

bool testSilence()
{
    FRiKKTUNEPitchShifter shifter; shifter.prepare(sr,1); float maxAbs=0.0f;
    for(int i=0;i<12000;++i){float in[1]{0.0f},out[1]{};shifter.processSample(in,out,1,1.8f);maxAbs=std::max(maxAbs,std::abs(out[0]));}
    return expect(maxAbs<1.0e-8f,"silence: no self-generated signal");
}
}
bool testMonoBufferSafety()
{
    FRiKKTUNEPitchShifter shifter;
    shifter.prepare(sr, 1);
    float peak = 0.0f;
    int nonFinite = 0;
    for (int i = 0; i < 16000; ++i) {
        float input[1] = {(float)(0.3 * std::sin(2.0 * pi * 196.0 * i / sr))};
        float output[1] = {};
        shifter.processSample(input, output, 1, 1.25f);
        if (!std::isfinite(output[0])) ++nonFinite;
        peak = std::max(peak, std::abs(output[0]));
    }
    bool ok = true;
    ok &= expect(nonFinite == 0, "mono one-float output: no NaN/Inf");
    ok &= expect(peak < 2.0f, "mono one-float output: bounded output");
    return ok;
}

bool testInvalidInputsAndRatios()
{
    FRiKKTUNEPitchShifter shifter;
    shifter.prepare(sr, 1);
    const float badValues[] = { std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::infinity(),
                                -std::numeric_limits<float>::infinity() };
    int nonFinite = 0;
    float peak = 0.0f;
    for (int i = 0; i < 12000; ++i) {
        const float x = (i < 3) ? badValues[i] : (float)(0.2 * std::sin(2.0 * pi * 196.0 * i / sr));
        float input[1] = {x};
        float output[1] = {};
        const float ratio = (i == 4) ? std::numeric_limits<float>::quiet_NaN()
                           : (i == 5) ? std::numeric_limits<float>::infinity()
                           : (i == 6) ? -std::numeric_limits<float>::infinity() : 1.0f;
        shifter.processSample(input, output, 1, ratio);
        if (!std::isfinite(output[0])) ++nonFinite;
        peak = std::max(peak, std::abs(output[0]));
    }
    bool ok = true;
    ok &= expect(nonFinite == 0, "invalid input/ratio: output remains finite");
    ok &= expect(peak < 2.0f, "invalid input/ratio: output remains bounded");
    return ok;
}

bool testResetRestartsCleanly()
{
    FRiKKTUNEPitchShifter shifter;
    shifter.prepare(sr, 1);
    float peakBefore = 0.0f;
    for (int i = 0; i < 7000; ++i) {
        float input[1] = {(float)(0.4 * std::sin(2.0 * pi * 220.0 * i / sr))};
        float output[1] = {};
        shifter.processSample(input, output, 1, 1.8f);
        peakBefore = std::max(peakBefore, std::abs(output[0]));
    }
    shifter.reset();
    float peakAfter = 0.0f;
    for (int i = 0; i < 6000; ++i) {
        float input[1] = {0.0f};
        float output[1] = {};
        shifter.processSample(input, output, 1, 1.0f);
        peakAfter = std::max(peakAfter, std::abs(output[0]));
    }
    bool ok = true;
    ok &= expect(peakBefore > 0.01f, "reset test: pre-reset signal processed");
    ok &= expect(peakAfter < 1.0e-7f, "reset test: stale audio cleared after reset");
    return ok;
}


bool testExtremeFiniteInputRecovery()
{
    FRiKKTUNEPitchShifter shifter;
    shifter.prepare(sr, 1);
    int nonFinite = 0;
    float peak = 0.0f;
    for (int i = 0; i < 12000; ++i) {
        const float x = i < 4 ? 1.0e30f : (float)(0.25 * std::sin(2.0 * pi * 220.0 * i / sr));
        float input[1] = { x }, output[1] = {};
        shifter.processSample(input, output, 1, 1.25f);
        if (!std::isfinite(output[0])) ++nonFinite;
        peak = std::max(peak, std::abs(output[0]));
    }
    bool ok = true;
    ok &= expect(nonFinite == 0, "extreme finite input: output remains finite");
    ok &= expect(peak < 2.0f, "extreme finite input: output remains bounded");
    return ok;
}

bool testChannelMismatchSafety()
{
    FRiKKTUNEPitchShifter shifter;
    shifter.prepare(sr, 1);
    float input[2] = { 0.2f, 0.8f };
    float output[2] = { 99.0f, 99.0f };
    shifter.processSample(input, output, 2, 1.0f);
    bool ok = true;
    ok &= expect(std::isfinite(output[0]), "channel mismatch: prepared channel remains finite");
    ok &= expect(output[1] == 0.0f, "channel mismatch: extra output channel explicitly cleared");
    // A zero-channel request must be a no-op and must not touch caller memory.
    float sentinel = 123.0f;
    shifter.processSample(nullptr, &sentinel, 0, 1.0f);
    ok &= expect(sentinel == 123.0f, "zero channels: safe no-op");
    return ok;
}


bool testSyntheticVowelNoiseAndTransitions()
{
    // Deterministic stress corpus: harmonic vowel-like source, broadband frication,
    // then a voiced/noisy transition. Stability only; not a perceptual quality score.
    FRiKKTUNEPitchShifter shifter;
    shifter.prepare(sr, 1);
    shifter.setFormantPreservation(1.0f);
    constexpr int count = 48000;
    unsigned int rng = 0x12345678u;
    double vowelEnergy = 0.0, fricationEnergy = 0.0, transitionEnergy = 0.0;
    int nonFinite = 0;
    float peak = 0.0f, maxStep = 0.0f, previous = 0.0f;
    auto noise = [&]() {
        rng = 1664525u * rng + 1013904223u;
        return (float)((double)(rng >> 8) / 8388607.5 - 1.0);
    };
    for (int i = 0; i < count; ++i) {
        const double t = (double)i / sr;
        float x = 0.0f;
        if (i < 16000 || i >= 32000) {
            // Harmonic source with broad spectral peaks approximating vocal resonances.
            for (int h = 1; h <= 35; ++h) {
                const double f = 120.0 * h;
                const auto gauss = [](double f, double center, double width) {
                    const double z = (f - center) / width;
                    return std::exp(-0.5 * z * z);
                };
                const double envelope = 0.12
                    + 0.85 * gauss(f, 750.0, 180.0)
                    + 0.55 * gauss(f, 1250.0, 260.0)
                    + 0.28 * gauss(f, 2600.0, 500.0);
                x += (float)((0.28 / h) * envelope * std::sin(2.0 * pi * f * t));
            }
        }
        if (i >= 16000 && i < 32000) {
            const float n = noise() * 0.16f;
            const float fade = (i < 18000) ? (float)(i - 16000) / 2000.0f
                              : (i >= 30000) ? (float)(32000 - i) / 2000.0f : 1.0f;
            x = n * std::clamp(fade, 0.0f, 1.0f);
        }
        // 1.5x pitch shift across the corpus; ratio changes near segment boundaries.
        const float ratio = i < 24000 ? 1.5f : 0.75f;
        float in[1]{x}, out[1]{};
        shifter.processSample(in, out, 1, ratio);
        if (!std::isfinite(out[0])) ++nonFinite;
        peak = std::max(peak, std::abs(out[0]));
        if (i > 0) maxStep = std::max(maxStep, std::abs(out[0] - previous));
        previous = out[0];
        if (i >= 4000 && i < 14000) vowelEnergy += (double)out[0] * out[0];
        if (i >= 20000 && i < 28000) fricationEnergy += (double)out[0] * out[0];
        if (i >= 34000 && i < 44000) transitionEnergy += (double)out[0] * out[0];
    }
    const double vowelRms = std::sqrt(vowelEnergy / 10000.0);
    const double fricationRms = std::sqrt(fricationEnergy / 8000.0);
    const double transitionRms = std::sqrt(transitionEnergy / 10000.0);
    std::cout << "synthetic-corpus vowel-rms=" << vowelRms
              << " frication-rms=" << fricationRms
              << " transition-rms=" << transitionRms
              << " peak=" << peak << " max-step=" << maxStep << "\n";
    bool ok = true;
    ok &= expect(nonFinite == 0, "synthetic corpus: no NaN/Inf");
    ok &= expect(peak < 2.0f, "synthetic corpus: bounded peak");
    ok &= expect(maxStep < 1.0f, "synthetic corpus: no extreme sample discontinuity");
    ok &= expect(vowelRms > 0.005 && vowelRms < 1.0, "synthetic corpus: vowel segment remains bounded and audible");
    ok &= expect(fricationRms < 0.5, "synthetic corpus: frication segment remains bounded");
    ok &= expect(transitionRms > 0.002 && transitionRms < 1.0, "synthetic corpus: post-transition voiced segment remains stable");
    return ok;
}


bool testHarmonicRichEnergyInterpolation()
{
    // Harmonic-rich vowel-like source whose partials generally fall between FFT bins.
    // This specifically exercises power-domain magnitude interpolation under pitch shift.
    FRiKKTUNEPitchShifter shifter;
    shifter.prepare(sr, 1);
    shifter.setFormantPreservation(0.0f);
    constexpr int count = 12000;
    int nonFinite = 0;
    float peak = 0.0f, maxStep = 0.0f, previous = 0.0f;
    double energy = 0.0;
    for (int i = 0; i < count; ++i) {
        const double t = (double)i / sr;
        const double vibrato = 1.0 + 0.012 * std::sin(2.0 * pi * 5.2 * t);
        const double phase = 2.0 * pi * 127.3 * vibrato * t;
        double x = 0.0;
        for (int h = 1; h <= 18; ++h) {
            const double formant1 = std::exp(-0.5 * std::pow((127.3 * h - 780.0) / 230.0, 2.0));
            const double formant2 = std::exp(-0.5 * std::pow((127.3 * h - 1450.0) / 360.0, 2.0));
            const double weight = (0.10 + 0.8 * formant1 + 0.45 * formant2) / h;
            x += weight * std::sin(h * phase + 0.13 * h);
        }
        x *= 0.22;
        float input[1] = {(float)x}, output[1] = {};
        shifter.processSample(input, output, 1, 1.25f);
        if (!std::isfinite(output[0])) ++nonFinite;
        peak = std::max(peak, std::abs(output[0]));
        if (i > 0) maxStep = std::max(maxStep, std::abs(output[0] - previous));
        previous = output[0];
        if (i > 4000) energy += (double)output[0] * output[0];
    }
    const double rms = std::sqrt(energy / (count - 4001));
    std::cout << "harmonic-vibrato rms=" << rms << " peak=" << peak
              << " max-step=" << maxStep << " nonfinite=" << nonFinite << "\\n";
    bool ok = true;
    ok &= expect(nonFinite == 0, "harmonic/vibrato: no NaN/Inf");
    ok &= expect(peak < 2.0f, "harmonic/vibrato: bounded peak");
    ok &= expect(maxStep < 1.0f, "harmonic/vibrato: no extreme sample discontinuity");
    ok &= expect(rms > 0.005 && rms < 1.0, "harmonic/vibrato: audible bounded output");
    return ok;
}


bool testMalformedInputsAndResetRecovery()
{
    FRiKKTUNEPitchShifter shifter;
    shifter.prepare(sr, 1);
    bool ok = true;
    float out[3] = { 9.0f, 9.0f, 9.0f };
    // Null input is treated as silence; a non-finite ratio must not poison state.
    for (int i = 0; i < 5000; ++i) {
        shifter.processSample(nullptr, out, 1, std::numeric_limits<float>::quiet_NaN());
        if (!std::isfinite(out[0])) ok = false;
    }
    ok &= expect(out[0] == 0.0f, "malformed input: null source safely behaves as silence");

    // Calls wider than the prepared layout must silence extra outputs.
    float in[3] = { 0.1f, 0.2f, 0.3f };
    out[0] = out[1] = out[2] = 9.0f;
    shifter.processSample(in, out, 3, 1.25f);
    ok &= expect(out[1] == 0.0f && out[2] == 0.0f,
                 "malformed layout: extra channels are explicitly silenced");

    // Non-finite and very large samples should be sanitised before FFT state.
    for (int i = 0; i < 10000; ++i) {
        float x[1] = { (i % 3 == 0) ? std::numeric_limits<float>::infinity()
                         : (i % 3 == 1) ? std::numeric_limits<float>::quiet_NaN() : 1.0e20f };
        float y[1] = {};
        shifter.processSample(x, y, 1, (i % 2 == 0) ? 2.0f : 0.5f);
        ok &= std::isfinite(y[0]);
    }
    ok &= expect(ok, "malformed input: non-finite/extreme samples do not poison output");

    shifter.reset();
    bool finiteAfterReset = true;
    float peakAfterReset = 0.0f;
    for (int i = 0; i < 8000; ++i) {
        float x[1] = { 0.25f * (float)std::sin(2.0 * pi * 220.0 * i / sr) };
        float y[1] = {};
        shifter.processSample(x, y, 1, 1.0f);
        finiteAfterReset &= std::isfinite(y[0]);
        peakAfterReset = std::max(peakAfterReset, std::abs(y[0]));
    }
    ok &= expect(finiteAfterReset && peakAfterReset < 2.0f,
                 "reset recovery: finite bounded output after hostile input");
    return ok;
}

int main(){
 bool ok=true;
 ok &= testMalformedInputsAndResetRecovery();
 ok &= testExtremeFiniteInputRecovery();
 ok &= testTransientBurstStability();
 ok &= testChannelMismatchSafety();
 ok &= testSilence();
 ok &= testMonoBufferSafety();
 ok &= testInvalidInputsAndRatios();
 ok &= testResetRestartsCleanly();
 ok &= testRatioTransition();
 ok &= testSyntheticVowelNoiseAndTransitions();
 const auto unity=runTone(1.0f,220.0,0.0f);
 ok &= expect(unity.nonFinite==0,"unity: no NaN/Inf");
 ok &= expect(unity.rms>0.20 && unity.rms<0.55,"unity: non-silent bounded output");
 ok &= expect(unity.correlation>0.90,"unity: correlated with latency-aligned input");
 const auto up=runTone(2.0f,220.0,0.0f);
 ok &= expect(up.nonFinite==0,"octave up: no NaN/Inf");
 ok &= expect(std::abs(up.frequency-440.0)<8.0,"octave up: output frequency near 440 Hz");
 const auto down=runTone(0.5f,440.0,0.0f);
 ok &= expect(down.nonFinite==0,"octave down: no NaN/Inf");
 ok &= expect(std::abs(down.frequency-220.0)<8.0,"octave down: output frequency near 220 Hz");
 // Regression matrix: cover the supported correction interval, not only octave extremes.
 // Zero-crossing estimates are intentionally used as a lightweight standalone check;
 // this is not a perceptual or formant-quality metric.
 const float ratios[] = {0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
 for (float ratio : ratios) {
   const auto r = runTone(ratio, 220.0, 0.0f);
   const double expected = 220.0 * (double)ratio;
   const double tolerance = std::max(5.0, expected * 0.025);
   const bool finiteAndBounded = r.nonFinite == 0 && r.peak < 2.0 && r.rms > 0.01;
   const bool frequencyOk = std::abs(r.frequency - expected) < tolerance;
   std::cout << "ratio-matrix ratio=" << ratio << " measured=" << r.frequency
             << "Hz expected=" << expected << "Hz rms=" << r.rms << " peak=" << r.peak << "\n";
   ok &= expect(finiteAndBounded, "ratio matrix: finite, audible, bounded output");
   ok &= expect(frequencyOk, "ratio matrix: frequency within tolerance");
 }
 // Exercise the envelope inverse/re-filter path at several strengths. These are
 // numerical stability checks, not perceptual formant-preservation measurements.
 for (float formantAmount : {0.0f, 0.5f, 1.0f}) {
   const auto f = runTone(1.5f, 220.0, formantAmount);
   std::cout << "formant-matrix amount=" << formantAmount << " rms=" << f.rms
             << " peak=" << f.peak << " measured=" << f.frequency << "Hz\n";
   ok &= expect(f.nonFinite == 0, "formant matrix: no NaN/Inf");
   ok &= expect(f.peak < 2.0 && f.rms > 0.01, "formant matrix: bounded audible output");
   ok &= expect(std::abs(f.frequency - 330.0) < 9.0, "formant matrix: pitch ratio retained");
 }

 // Expanded deterministic pitch benchmark: several source registers and ratios.
 // Zero-crossing pitch estimates are valid here because the stimulus is a pure sine.
 const double sourceFrequencies[] = {110.0, 220.0, 440.0};
 const float benchmarkRatios[] = {0.75f, 1.0f, 1.25f, 1.5f};
 int benchmarkCases = 0;
 double worstRelativePitchError = 0.0;
 for (double sourceHz : sourceFrequencies) {
   for (float ratio : benchmarkRatios) {
     const auto b = runTone(ratio, sourceHz, 0.0f);
     const double expected = sourceHz * (double)ratio;
     const double relativeError = std::abs(b.frequency - expected) / expected;
     worstRelativePitchError = std::max(worstRelativePitchError, relativeError);
     ++benchmarkCases;
     std::cout << "pitch-benchmark source=" << sourceHz << "Hz ratio=" << ratio
               << " measured=" << b.frequency << "Hz expected=" << expected
               << "Hz rel-error=" << (100.0 * relativeError) << "%"
               << " rms=" << b.rms << " peak=" << b.peak
               << " nonfinite=" << b.nonFinite << "\n";
     ok &= expect(b.nonFinite == 0, "pitch benchmark: finite output");
     ok &= expect(b.peak < 2.0 && b.rms > 0.005, "pitch benchmark: bounded audible output");
     ok &= expect(relativeError < 0.035, "pitch benchmark: frequency error under 3.5%");
   }
 }
 std::cout << "pitch-benchmark-summary cases=" << benchmarkCases
           << " worst-relative-error=" << (100.0 * worstRelativePitchError) << "%\n";

 ok &= testHarmonicRichEnergyInterpolation();
 const auto stereo=runTone(1.5f,220.0,0.0f,true);
 ok &= expect(stereo.stereoDifference<1.0e-6,"stereo: matched channels remain phase/timing linked");
 ok &= expect(stereo.nonFinite==0,"stereo: no NaN/Inf");
 std::cout<<"metrics unity rms="<<unity.rms<<" correlation="<<unity.correlation<<" up="<<up.frequency<<"Hz down="<<down.frequency<<"Hz\n";
 return ok?0:1;
}
