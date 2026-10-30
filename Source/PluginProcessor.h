#pragma once
#include <JuceHeader.h>
#include "PitchDetector.h"
#include "NoteQuantizer.h"
#include "PitchShifter.h"
#include "PitchJumpGate.h"
#include "NoteTargetTracker.h"
#include <atomic>

class FRiKKTUNEAudioProcessor final : public juce::AudioProcessor
{
public:
    FRiKKTUNEAudioProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "FRiKKTUNE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return sampleRate > 0.0 ? (double)FRiKKTUNEPitchShifter::fftSize / sampleRate : 0.0; }
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float detectedMidi() const noexcept { return midiNote.load(); }
    float confidence() const noexcept { return pitchConfidence.load(); }
    float correctionCents() const noexcept { return cents.load(); }
    float inputPeak() const noexcept { return inPeak.load(); }
private:
    PitchDetector detector;
    FRiKKTUNEPitchShifter pitchShifter;
    std::atomic<float> midiNote{-1.0f}, pitchConfidence{0.0f}, cents{0.0f}, inPeak{0.0f};
    double sampleRate = 44100.0;
    int analysisCounter = 0;
    int unvoicedFrames = 0;
    float lastAcceptedConfidence = 0.0f;
    float heldTargetMidi = -1.0f;
    float smoothedMidi = -1.0f;
    float smoothedCorrectionCents = 0.0f;
    float humanizeCenterMidi = -1.0f;
    float humanizeDeviation = 0.0f;
    PitchJumpGate pitchJumpGate;
    NoteTargetTracker noteTargetTracker;
    juce::SmoothedValue<float> outputGain;
    juce::SmoothedValue<float> pitchRatio;
    juce::SmoothedValue<float> wetMix;
    juce::SmoothedValue<float> bypassMix;
    std::array<std::array<float, FRiKKTUNEPitchShifter::fftSize>, FRiKKTUNEPitchShifter::maxChannels> dryDelay{};
    int dryDelayPosition = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FRiKKTUNEAudioProcessor)
};
