#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <algorithm>

// Streaming, stereo-linked phase-vocoder pitch shifter for monophonic sources.
// Fixed-size buffers; no allocation or locking in processSample().
// Deliberate latency: fftSize samples. Formant preservation is not claimed here.
class FRiKKTUNEPitchShifter
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder; // 2048
    static constexpr int hopSize = 256;
    static constexpr int bins = fftSize / 2 + 1;
    static constexpr int ringSize = fftSize * 8;
    static constexpr int maxChannels = 2;

    FRiKKTUNEPitchShifter() : fft(fftOrder)
    {
        // sqrt-Hann analysis/synthesis window; 8x overlap has a constant sum of
        // squared windows of approximately 4, corrected during overlap-add.
        for (int i = 0; i < fftSize; ++i)
            window[(size_t)i] = std::sqrt(0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * (float)i / (float)fftSize));
        reset();
    }

    void prepare(double sr, int channels) noexcept
    {
        sampleRate = sr > 1.0 ? sr : 44100.0;
        numChannels = juce::jlimit(1, maxChannels, channels);
        reset();
    }

    void reset() noexcept
    {
        inputWrite = 0;
        outputRead = 0;
        samplesSinceFrame = 0;
        for (auto& channel : inputRing)
            channel.fill(0.0f);
        for (auto& channel : outputRing)
            channel.fill(0.0f);
        for (auto& channel : overlapRing)
            channel.fill(0.0f);
        for (auto& channel : previousPhase)
            channel.fill(0.0f);
        for (auto& channel : synthesisPhase)
            channel.fill(0.0f);
        for (auto& channel : magnitudeSmooth)
            channel.fill(0.0f);
        for (auto& channel : previousMagnitude)
            channel.fill(0.0f);
        for (auto& channel : phaseDeviation)
            channel.fill(0.0f);
        for (auto& channel : spectralEnvelope)
            channel.fill(0.0f);
        for (auto& channel : envelopeSmooth)
            channel.fill(0.0f);
        for (auto& channel : phaseLock)
            channel.fill(0.0f);
        for (auto& channel : transientHold)
            channel.fill(0.0f);
        for (auto& channel : transientMix)
            channel.fill(0.0f);
        for (auto& channel : frameHistory)
            channel.fill(0.0f);
        for (auto& channel : frameSpectrum)
            channel.fill({});
        for (auto& channel : shiftedSpectrum)
            channel.fill({});
        for (auto& channel : fftInput)
            channel.fill({});
        for (auto& channel : fftOutput)
            channel.fill({});
        pitchRatio = 1.0f;
        targetPitchRatio = 1.0f;
        voicedness = 0.0f;
        envelopeConfidence = 0.0f;
        transientStrength = 0.0f;
        phaseCoherence = 0.0f;
        frameReady = false;
        processedFrames = 0;
    }

    void setPitchRatio(float ratio) noexcept
    {
        targetPitchRatio = juce::jlimit(0.25f, 4.0f, ratio);
    }

    void setFormantPreservation(float amount) noexcept
    {
        formantAmount = juce::jlimit(0.0f, 1.0f, amount);
    }

    void setTransientProtection(float amount) noexcept
    {
        transientProtection = juce::jlimit(0.0f, 1.0f, amount);
    }

    void setPhaseCoherence(float amount) noexcept
    {
        phaseCoherenceAmount = juce::jlimit(0.0f, 1.0f, amount);
    }

    float getVoicedness() const noexcept { return voicedness; }
    float getEnvelopeConfidence() const noexcept { return envelopeConfidence; }
    float getTransientStrength() const noexcept { return transientStrength; }
    float getPhaseCoherence() const noexcept { return phaseCoherence; }
    int getLatencySamples() const noexcept { return fftSize; }

    void processSample(const float* input, float* output, int channels, float ratio) noexcept
    {
        if (output == nullptr || channels <= 0)
            return;

        if (input == nullptr)
        {
            for (int ch = 0; ch < channels; ++ch)
                output[ch] = 0.0f;
            return;
        }

        setPitchRatio(ratio);
        const int supportedChannels = juce::jmin(channels, numChannels);
        for (int ch = 0; ch < supportedChannels; ++ch)
            output[ch] = processSample(ch, input[ch]);

        for (int ch = supportedChannels; ch < channels; ++ch)
            output[ch] = input[ch];
    }

    float processSample(int channel, float input) noexcept
    {
        channel = juce::jlimit(0, numChannels - 1, channel);
        inputRing[(size_t)channel][(size_t)inputWrite] = input;

        float output = outputRing[(size_t)channel][(size_t)outputRead];
        outputRing[(size_t)channel][(size_t)outputRead] = 0.0f;

        if (channel == numChannels - 1)
        {
            ++inputWrite;
            if (inputWrite >= ringSize)
                inputWrite = 0;

            ++outputRead;
            if (outputRead >= ringSize)
                outputRead = 0;

            ++samplesSinceFrame;
            if (samplesSinceFrame >= hopSize)
            {
                samplesSinceFrame = 0;
                processFrame(outputRead);
            }
        }

        return output;
    }

private:
    using Complex = juce::dsp::Complex<float>;
    using Spectrum = std::array<Complex, fftSize>;
    using BinArray = std::array<float, bins>;

    static constexpr float epsilon = 1.0e-9f;

    static float wrapPhase(float phase) noexcept
    {
        while (phase > juce::MathConstants<float>::pi)
            phase -= juce::MathConstants<float>::twoPi;
        while (phase < -juce::MathConstants<float>::pi)
            phase += juce::MathConstants<float>::twoPi;
        return phase;
    }

    static float smoothstep(float a, float b, float x) noexcept
    {
        const float t = juce::jlimit(0.0f, 1.0f, (x - a) / (b - a));
        return t * t * (3.0f - 2.0f * t);
    }

    static float median3(float a, float b, float c) noexcept
    {
        return std::max(std::min(a, b), std::min(std::max(a, b), c));
    }

    static bool estimateLpcEnvelope(const Spectrum& frame,
                                     BinArray& envelope,
                                     float& confidence) noexcept
    {
        constexpr int order = 16;
        constexpr int start = 32;
        constexpr int end = fftSize / 2;
        std::array<double, order + 1> autocorrelation{};
        std::array<double, order + 1> coefficients{};
        std::array<double, order + 1> previous{};

        for (int lag = 0; lag <= order; ++lag)
        {
            double sum = 0.0;
            for (int n = start + lag; n < end; ++n)
                sum += (double)frame[(size_t)n].real() * (double)frame[(size_t)(n - lag)].real();
            autocorrelation[(size_t)lag] = sum;
        }

        if (autocorrelation[0] < 1.0e-12)
        {
            confidence = 0.0f;
            envelope.fill(1.0f);
            return false;
        }

        double error = autocorrelation[0];
        coefficients[0] = 1.0;

        for (int i = 1; i <= order; ++i)
        {
            double reflection = autocorrelation[(size_t)i];
            for (int j = 1; j < i; ++j)
                reflection += coefficients[(size_t)j] * autocorrelation[(size_t)(i - j)];

            reflection = -reflection / std::max(error, 1.0e-12);
            reflection = juce::jlimit(-0.98, 0.98, reflection);

            previous = coefficients;
            for (int j = 1; j < i; ++j)
                coefficients[(size_t)j] = previous[(size_t)j] + reflection * previous[(size_t)(i - j)];
            coefficients[(size_t)i] = reflection;

            error *= (1.0 - reflection * reflection);
            if (error < 1.0e-12)
                break;
        }

        confidence = (float)juce::jlimit(0.0, 1.0, 1.0 - error / autocorrelation[0]);

        for (int k = 0; k < bins; ++k)
        {
            const double omega = juce::MathConstants<double>::twoPi * (double)k / (double)fftSize;
            double real = 1.0;
            double imag = 0.0;

            for (int i = 1; i <= order; ++i)
            {
                real += coefficients[(size_t)i] * std::cos(omega * (double)i);
                imag -= coefficients[(size_t)i] * std::sin(omega * (double)i);
            }

            const double power = real * real + imag * imag;
            envelope[(size_t)k] = (float)(1.0 / std::sqrt(std::max(power, 1.0e-12)));
        }

        const float reference = std::max(envelope[8], 1.0e-6f);
        for (int k = 0; k < bins; ++k)
            envelope[(size_t)k] = juce::jlimit(0.1f, 10.0f, envelope[(size_t)k] / reference);

        return true;
    }

    float estimateVoicedness(const Spectrum& frame) const noexcept
    {
        double energy = 0.0;
        double difference = 0.0;
        constexpr int start = 40;
        constexpr int end = fftSize - 40;

        for (int n = start; n < end; ++n)
        {
            const double x = frame[(size_t)n].real();
            energy += x * x;
        }

        for (int lag = 40; lag <= 400; ++lag)
        {
            double correlation = 0.0;
            double localEnergy = 0.0;
            for (int n = start + lag; n < end; ++n)
            {
                const double a = frame[(size_t)n].real();
                const double b = frame[(size_t)(n - lag)].real();
                correlation += a * b;
                localEnergy += a * a + b * b;
            }

            if (localEnergy > 1.0e-10)
                difference = std::max(difference, correlation / (0.5 * localEnergy));
        }

        const double rms = std::sqrt(energy / (double)(end - start));
        const float level = smoothstep(0.0005f, 0.02f, (float)rms);
        return juce::jlimit(0.0f, 1.0f, (float)difference) * level;
    }

    void processFrame(int writePosition) noexcept
    {
        pitchRatio += 0.08f * (targetPitchRatio - pitchRatio);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& in = fftInput[(size_t)ch];
            auto& out = fftOutput[(size_t)ch];

            for (int i = 0; i < fftSize; ++i)
            {
                const int position = (writePosition - fftSize + i + ringSize) % ringSize;
                const float x = inputRing[(size_t)ch][(size_t)position] * window[(size_t)i];
                in[(size_t)i].real(x);
                in[(size_t)i].imag(0.0f);
            }

            fft.perform(in.data(), out.data(), false);

            auto& spectrum = frameSpectrum[(size_t)ch];
            for (int k = 0; k < bins; ++k)
                spectrum[(size_t)k] = out[(size_t)k];

            if (ch == 0)
            {
                voicedness = estimateVoicedness(spectrum);
                estimateLpcEnvelope(spectrum, spectralEnvelope[(size_t)ch], envelopeConfidence);
            }
        }

        estimateTransient();
        estimatePhaseCoherence();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& in = fftInput[(size_t)ch];
            auto& out = fftOutput[(size_t)ch];
            auto& spectrum = frameSpectrum[(size_t)ch];
            auto& shifted = shiftedSpectrum[(size_t)ch];

            for (int k = 0; k < bins; ++k)
            {
                const float re = out[(size_t)k].real();
                const float im = out[(size_t)k].imag();
                const float magnitude = std::sqrt(re * re + im * im);
                const float phase = std::atan2(im, re);

                const float oldPhase = previousPhase[(size_t)ch][(size_t)k];
                float delta = wrapPhase(phase - oldPhase -
                    juce::MathConstants<float>::twoPi * (float)k * (float)hopSize / (float)fftSize);
                const float frequency = (float)k + delta * (float)fftSize /
                    (juce::MathConstants<float>::twoPi * (float)hopSize);

                previousPhase[(size_t)ch][(size_t)k] = phase;

                const float destination = (float)k * pitchRatio;
                const int lower = (int)destination;
                const float fraction = destination - (float)lower;

                if (lower >= 0 && lower < bins)
                {
                    magnitudeSmooth[(size_t)ch][(size_t)lower] += magnitude * (1.0f - fraction);
                    if (lower + 1 < bins)
                        magnitudeSmooth[(size_t)ch][(size_t)(lower + 1)] += magnitude * fraction;

                    phaseDeviation[(size_t)ch][(size_t)lower] = frequency;
                }
            }

            for (int k = 0; k < bins; ++k)
            {
                const float magnitude = magnitudeSmooth[(size_t)ch][(size_t)k];
                const float frequency = phaseDeviation[(size_t)ch][(size_t)k];

                const float expected = juce::MathConstants<float>::twoPi *
                    (float)k * (float)hopSize / (float)fftSize;
                const float phaseIncrement = expected +
                    (frequency - (float)k) * juce::MathConstants<float>::twoPi *
                    (float)hopSize / (float)fftSize;

                synthesisPhase[(size_t)ch][(size_t)k] += phaseIncrement;
                const float phase = synthesisPhase[(size_t)ch][(size_t)k];

                shifted[(size_t)k].real(magnitude * std::cos(phase));
                shifted[(size_t)k].imag(magnitude * std::sin(phase));

                magnitudeSmooth[(size_t)ch][(size_t)k] = 0.0f;
                phaseDeviation[(size_t)ch][(size_t)k] = 0.0f;
            }

            for (int k = 1; k < fftSize / 2; ++k)
            {
                shifted[(size_t)(fftSize - k)].real(shifted[(size_t)k].real());
                shifted[(size_t)(fftSize - k)].imag(-shifted[(size_t)k].imag());
            }

            shifted[0].imag(0.0f);
            shifted[(size_t)(fftSize / 2)].imag(0.0f);

            fft.perform(shifted.data(), in.data(), true);

            for (int i = 0; i < fftSize; ++i)
            {
                const int position = (writePosition + i) % ringSize;
                const float y = in[(size_t)i].real() * window[(size_t)i] * 0.25f;
                outputRing[(size_t)ch][(size_t)position] += y;
            }
        }

        ++processedFrames;
        frameReady = true;
    }

    void estimateTransient() noexcept
    {
        double flux = 0.0;
        double total = 0.0;

        for (int k = 1; k < bins; ++k)
        {
            const float magnitude = std::abs(frameSpectrum[0][(size_t)k].real());
            const float previous = previousMagnitude[0][(size_t)k];
            flux += std::max(0.0f, magnitude - previous);
            total += magnitude;
            previousMagnitude[0][(size_t)k] = magnitude;
        }

        const float normalized = (float)(flux / std::max(total, 1.0e-9));
        transientStrength = 0.8f * transientStrength + 0.2f * normalized;
    }

    void estimatePhaseCoherence() noexcept
    {
        double coherence = 0.0;
        double count = 0.0;

        for (int k = 2; k < bins - 2; ++k)
        {
            const float a = std::atan2(frameSpectrum[0][(size_t)k].imag(),
                                       frameSpectrum[0][(size_t)k].real());
            const float b = std::atan2(frameSpectrum[0][(size_t)(k - 1)].imag(),
                                       frameSpectrum[0][(size_t)(k - 1)].real());
            const float c = std::atan2(frameSpectrum[0][(size_t)(k + 1)].imag(),
                                       frameSpectrum[0][(size_t)(k + 1)].real());
            const float deviation = std::abs(wrapPhase(a - 0.5f * (b + c)));
            coherence += 1.0 - juce::jlimit(0.0f, 1.0f, deviation / juce::MathConstants<float>::pi);
            count += 1.0;
        }

        phaseCoherence = count > 0.0 ? (float)(coherence / count) : 0.0f;
    }

    juce::dsp::FFT fft;
    double sampleRate = 44100.0;
    int numChannels = 2;
    int inputWrite = 0;
    int outputRead = 0;
    int samplesSinceFrame = 0;
    int processedFrames = 0;

    float pitchRatio = 1.0f;
    float targetPitchRatio = 1.0f;
    float formantAmount = 0.0f;
    float transientProtection = 0.5f;
    float phaseCoherenceAmount = 0.5f;
    float voicedness = 0.0f;
    float envelopeConfidence = 0.0f;
    float transientStrength = 0.0f;
    float phaseCoherence = 0.0f;
    bool frameReady = false;

    std::array<float, fftSize> window{};

    std::array<std::array<float, ringSize>, maxChannels> inputRing{};
    std::array<std::array<float, ringSize>, maxChannels> outputRing{};
    std::array<std::array<float, ringSize>, maxChannels> overlapRing{};

    std::array<std::array<float, bins>, maxChannels> previousPhase{};
    std::array<std::array<float, bins>, maxChannels> synthesisPhase{};
    std::array<std::array<float, bins>, maxChannels> magnitudeSmooth{};
    std::array<std::array<float, bins>, maxChannels> previousMagnitude{};
    std::array<std::array<float, bins>, maxChannels> phaseDeviation{};
    std::array<std::array<float, bins>, maxChannels> spectralEnvelope{};
    std::array<std::array<float, bins>, maxChannels> envelopeSmooth{};
    std::array<std::array<float, bins>, maxChannels> phaseLock{};
    std::array<std::array<float, bins>, maxChannels> transientHold{};
    std::array<std::array<float, bins>, maxChannels> transientMix{};

    std::array<std::array<float, fftSize>, maxChannels> frameHistory{};
    std::array<Spectrum, maxChannels> frameSpectrum{};
    std::array<Spectrum, maxChannels> shiftedSpectrum{};
    std::array<Spectrum, maxChannels> fftInput{};
    std::array<Spectrum, maxChannels> fftOutput{};
};
