#include "PluginProcessor.h"
#include "PluginEditor.h"

FRiKKTUNEAudioProcessor::FRiKKTUNEAudioProcessor()
 : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
   parameters(*this, nullptr, "FRiKKTUNE_STATE", createParameterLayout()) {}

bool FRiKKTUNEAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    // This is a monophonic pitch tracker but it can process mono or stereo audio.
    // Keep input/output channel counts matched so the dry alignment path is well-defined.
    if (input != output) return false;
    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

juce::AudioProcessorValueTreeState::ParameterLayout FRiKKTUNEAudioProcessor::createParameterLayout()
{
    using F = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<F>("amount", "Correction Amount", juce::NormalisableRange<float>(0,100,0.1f), 100));
    p.add(std::make_unique<F>("speed", "Retune Speed", juce::NormalisableRange<float>(0,100,0.1f), 35));
    p.add(std::make_unique<F>("humanize", "Humanize", juce::NormalisableRange<float>(0,100,0.1f), 20));
    p.add(std::make_unique<F>("formant", "Formant", juce::NormalisableRange<float>(0,100,0.1f), 75));
    p.add(std::make_unique<F>("reference", "Detune / A4 Reference (Hz)", juce::NormalisableRange<float>(400, 480, 0.1f), 440));
    // Fine Tune (cents) was intentionally removed from the public parameter layout.
    // The reference-frequency control above is the single, unambiguous tuning reference.
    p.add(std::make_unique<F>("wet", "Dry Wet", juce::NormalisableRange<float>(0,100,0.1f), 100));
    p.add(std::make_unique<F>("gain", "Output Gain", juce::NormalisableRange<float>(-24,12,0.1f), 0));
    p.add(std::make_unique<C>("mode", "Mode", juce::StringArray{"Natural", "Modern / Hard Tune"}, 0));
    p.add(std::make_unique<C>("key", "Key", juce::StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, 0));
    p.add(std::make_unique<C>("scale", "Scale", juce::StringArray{"Chromatic","Major","Minor"}, 1));
    p.add(std::make_unique<C>("range", "Input Range", juce::StringArray{"Bass (45–400 Hz)","Tenor (65–700 Hz)","Alto (90–1000 Hz)","High (140–1400 Hz)"}, 1));
    p.add(std::make_unique<B>("bypass", "Bypass", false));
    return p;
}

void FRiKKTUNEAudioProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
    sampleRate = sr;
    detector.prepare(sr);
    pitchShifter.prepare(sr, getTotalNumOutputChannels());
    for (auto& channel : dryDelay) channel.fill(0.0f);
    dryDelayPosition = 0;
    setLatencySamples(FRiKKTUNEPitchShifter::fftSize);
    pitchRatio.reset(sr, 0.015);
    pitchRatio.setCurrentAndTargetValue(1.0f);
    outputGain.reset(sr, 0.02);
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(parameters.getRawParameterValue("gain")->load()));
    // Smooth mix and bypass automation to avoid clicks when controls move or the
    // host toggles bypass between adjacent audio blocks.
    wetMix.reset(sr, 0.005);
    wetMix.setCurrentAndTargetValue(parameters.getRawParameterValue("wet")->load() * 0.01f);
    bypassMix.reset(sr, 0.005);
    bypassMix.setCurrentAndTargetValue(parameters.getRawParameterValue("bypass")->load() > 0.5f ? 1.0f : 0.0f);
    analysisCounter = 0; unvoicedFrames = 0; lastAcceptedConfidence = 0.0f; heldTargetMidi = -1.0f; noteTargetTracker.reset(); smoothedMidi = -1.0f; smoothedCorrectionCents = 0.0f; humanizeCenterMidi = -1.0f; humanizeDeviation = 0.0f; pitchJumpGate.resetPending();
}

void FRiKKTUNEAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels = juce::jmin(getTotalNumInputChannels(), FRiKKTUNEPitchShifter::maxChannels);
    const int n = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, n);

    float peak = 0.0f;
    const float ref = parameters.getRawParameterValue("reference")->load();
    const int key = (int)parameters.getRawParameterValue("key")->load();
    const int scale = (int)parameters.getRawParameterValue("scale")->load();
    const int range = juce::jlimit(0, 3, (int)parameters.getRawParameterValue("range")->load());
    static constexpr float minHz[] = {45.0f, 65.0f, 90.0f, 140.0f};
    static constexpr float maxHz[] = {400.0f, 700.0f, 1000.0f, 1400.0f};
    detector.setFrequencyRange(minHz[range], maxHz[range]);

    const float amount = parameters.getRawParameterValue("amount")->load() * 0.01f;
    const float speed = parameters.getRawParameterValue("speed")->load();
    const float human = parameters.getRawParameterValue("humanize")->load();
    pitchShifter.setFormantPreservation(parameters.getRawParameterValue("formant")->load() * 0.01f);
    bypassMix.setTargetValue(parameters.getRawParameterValue("bypass")->load() > 0.5f ? 1.0f : 0.0f);
    wetMix.setTargetValue(parameters.getRawParameterValue("wet")->load() * 0.01f);
    outputGain.setTargetValue(juce::Decibels::decibelsToGain(parameters.getRawParameterValue("gain")->load()));
    const bool hardTune = parameters.getRawParameterValue("mode")->load() > 0.5f;

    // Retune Speed controls the trajectory smoothing, not the detector's note choice.
    // Humanize is implemented below as vibrato/deviation preservation rather than
    // merely making the correction slower (which used to be a misleading control).
    const float retuneMs = hardTune ? juce::jmap(speed, 0.0f, 100.0f, 12.0f, 0.5f)
                                    : juce::jmap(speed, 0.0f, 100.0f, 90.0f, 4.0f);

    for (int i = 0; i < n; ++i)
    {
        float in[FRiKKTUNEPitchShifter::maxChannels] { 0.0f, 0.0f };
        float shifted[FRiKKTUNEPitchShifter::maxChannels] { 0.0f, 0.0f };
        float mono = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            in[ch] = buffer.getReadPointer(ch)[i];
            if (!std::isfinite(in[ch])) in[ch] = 0.0f;
            mono += in[ch];
            peak = juce::jmax(peak, std::abs(in[ch]));
        }
        mono /= (float)juce::jmax(1, channels);
        detector.push(mono);

        if (++analysisCounter >= 1024)
        {
            analysisCounter = 0;
            const auto r = detector.analyse();
            pitchConfidence.store(r.confidence);
            // A short unvoiced grace period bridges consonants/breath gaps. Resetting
            // the target on every weak frame makes the correction pump and can create
            // audible pitch jumps at syllable boundaries.
            bool accepted = r.voiced && r.confidence >= 0.65f && r.hz > 0.0f
                         && std::isfinite(r.hz);
            float midi = accepted ? NoteQuantizer::frequencyToMidi(r.hz, juce::jmax(1.0f, ref)) : -1.0f;
            if (accepted)
                accepted = pitchJumpGate.accept(midi, r.confidence, smoothedMidi);
            else
                pitchJumpGate.resetPending();

            if (accepted)
            {
                unvoicedFrames = 0;
                lastAcceptedConfidence = r.confidence;
                if (smoothedMidi < 0.0f) smoothedMidi = midi;
                else
                {
                    const float baseTrackingMs = hardTune ? 3.0f : juce::jmap(speed, 0.0f, 100.0f, 55.0f, 10.0f);
                    // Less certain estimates are tracked more cautiously; confident
                    // estimates respond faster without changing the Retune Speed control.
                    const float confidenceScale = juce::jmap(juce::jlimit(0.0f, 1.0f, r.confidence), 1.25f, 0.80f);
                    const float trackingMs = baseTrackingMs * confidenceScale;
                    const float alpha = 1.0f - std::exp((float)(-1024.0 / (sampleRate * trackingMs * 0.001)));
                    smoothedMidi += alpha * (midi - smoothedMidi);
                }
                midiNote.store(smoothedMidi);
                // Track a slow local pitch center and the residual movement around it.
                // At higher Humanize values, small sustained deviations (vibrato,
                // expressive scoops) are preserved by reducing correction locally.
                // Large errors still receive full correction. Analysis runs every
                // 1024 samples, so these coefficients are expressed per analysis frame.
                if (humanizeCenterMidi < 0.0f)
                    humanizeCenterMidi = smoothedMidi;
                const float centerAlpha = 1.0f - std::exp(-1024.0f / (float)(sampleRate * 0.085));
                humanizeCenterMidi += centerAlpha * (smoothedMidi - humanizeCenterMidi);
                const float deviation = std::abs(smoothedMidi - humanizeCenterMidi) * 100.0f;
                const float deviationAlpha = 1.0f - std::exp(-1024.0f / (float)(sampleRate * 0.045));
                humanizeDeviation += deviationAlpha * (deviation - humanizeDeviation);
                std::array<bool, 12> enabled; enabled.fill(true);
                const auto selectedScale = scale == 0 ? NoteQuantizer::Scale::Chromatic
                    : (scale == 1 ? NoteQuantizer::Scale::Major : NoteQuantizer::Scale::Minor);
                const int best = NoteQuantizer::nearestAllowedMidi(smoothedMidi, key, selectedScale, enabled);
                // Hard mode confirms a real note change quickly; natural mode requires
                // an extra analysis frame to avoid switching targets on vibrato/noisy F0.
                const int confirmationFrames = hardTune ? 2 : 3;
                const bool decisiveTransition = r.confidence >= (hardTune ? 0.90f : 0.97f)
                    && std::abs(smoothedMidi - static_cast<float>(noteTargetTracker.current())) >= 0.65f;
                const int stableTarget = noteTargetTracker.update(smoothedMidi, best,
                                                                  confirmationFrames, 0.30f,
                                                                  decisiveTransition);
                heldTargetMidi = stableTarget >= 0 ? (float)stableTarget : (float)best;
                // Humanize attenuation is strongest for small, repeated pitch
                // excursions and fades out for substantial off-note errors.
                // This keeps expressive vibrato without leaving a badly sung note
                // uncorrected. Amount=0 and Humanize=0 remain transparent.
                const float vibratoPreserve = juce::jlimit(0.0f, 1.0f,
                    1.0f - juce::jmax(0.0f, humanizeDeviation - 8.0f) / 42.0f);
                // Confidence-weight the correction instead of applying full correction
                // immediately at the voiced/unvoiced boundary. This reduces false-note
                // grabs and audible pumping on breathy, noisy, or consonant-heavy frames.
                // The voiced gate remains at 0.65; full correction is reached at 0.90.
                const float confidenceWeight = juce::jlimit(0.0f, 1.0f,
                    (r.confidence - 0.65f) / 0.25f);
                const float effectiveAmount = amount * confidenceWeight;
                float targetCents = NoteQuantizer::correctionTargetCents(
                    smoothedMidi, heldTargetMidi, effectiveAmount, 0.0f);
                // Add back a controlled portion of the current local deviation so
                // the correction follows the note center but retains natural motion.
                // Static off-note pitch is still corrected at the requested amount.
                targetCents += (smoothedMidi - humanizeCenterMidi) * 100.0f
                             * 0.80f * (human / 100.0f) * vibratoPreserve * confidenceWeight;
                smoothedCorrectionCents = NoteQuantizer::smoothCents(smoothedCorrectionCents, targetCents,
                    retuneMs, sampleRate, 1024);
                const float ratio = std::exp2(smoothedCorrectionCents / 1200.0f);
                pitchRatio.setTargetValue(juce::jlimit(0.5f, 2.0f, ratio));
                cents.store(smoothedCorrectionCents);
            }
            else
            {
                pitchConfidence.store(0.0f);
                ++unvoicedFrames;
                if (unvoicedFrames <= 3 && smoothedMidi >= 0.0f)
                {
                    // Hold the last musical target briefly through unvoiced consonants.
                    // Do not feed a noisy measurement into the pitch trajectory.
                    pitchRatio.setTargetValue(juce::jlimit(0.5f, 2.0f,
                        std::exp2(smoothedCorrectionCents / 1200.0f)));
                }
                else
                {
                    smoothedCorrectionCents = NoteQuantizer::smoothCents(smoothedCorrectionCents, 0.0f,
                        hardTune ? 8.0f : 45.0f, sampleRate, 1024);
                    pitchRatio.setTargetValue(1.0f);
                    cents.store(smoothedCorrectionCents);
                    smoothedMidi = -1.0f;
                    heldTargetMidi = -1.0f;
                    humanizeCenterMidi = -1.0f;
                    humanizeDeviation = 0.0f;
                    noteTargetTracker.reset();
                    lastAcceptedConfidence = 0.0f;
                }
            }
        }

        float alignedDry[FRiKKTUNEPitchShifter::maxChannels] { 0.0f, 0.0f };
        for (int ch = 0; ch < channels; ++ch)
        {
            alignedDry[ch] = dryDelay[(size_t)ch][(size_t)dryDelayPosition];
            dryDelay[(size_t)ch][(size_t)dryDelayPosition] = in[ch];
        }
        dryDelayPosition = (dryDelayPosition + 1) % FRiKKTUNEPitchShifter::fftSize;
        pitchShifter.processSample(in, shifted, channels, pitchRatio.getNextValue());
        const float g = outputGain.getNextValue();
        const float wet = wetMix.getNextValue();
        const float bypass = bypassMix.getNextValue();
        for (int ch = 0; ch < channels; ++ch)
        {
            const float dry = alignedDry[ch];
            const float corrected = dry + wet * (shifted[ch] - dry);
            const float processed = corrected * g;
            // Crossfade between latency-aligned dry and processed paths. This
            // avoids a discontinuity on bypass changes and preserves host latency.
            const float y = processed * (1.0f - bypass) + dry * bypass;
            buffer.getWritePointer(ch)[i] = std::isfinite(y) ? y : 0.0f;
        }
    }
    inPeak.store(peak);
}
void FRiKKTUNEAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto xml = parameters.copyState().createXml();
    copyXmlToBinary(*xml, dest);
}
void FRiKKTUNEAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FRiKKTUNEAudioProcessor(); }
