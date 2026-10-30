#pragma once
#include <array>
#include <cmath>
#include <algorithm>

// Allocation-free YIN-style monophonic F0 estimator.
// Analysis is intentionally performed off the audio-rate inner loop by the caller.
class PitchDetector
{
public:
    static constexpr int capacity = 8192;
    struct Result { float hz = 0.0f; float confidence = 0.0f; bool voiced = false; };

    void prepare(double sr, float minHz = 70.0f, float maxHz = 1000.0f) noexcept
    {
        // Decimate by two before YIN analysis. The pitch range tops out at 1.8 kHz,
        // so a properly pair-averaged 0.5x stream retains ample bandwidth while
        // reducing the quadratic difference-function cost by roughly 4x.
        const double hostRate = std::isfinite(sr) && sr > 1000.0 ? sr : 44100.0;
        sampleRate = hostRate * 0.5;
        setFrequencyRange(minHz, maxHz);
        reset();
    }
    void setFrequencyRange(float minHz, float maxHz) noexcept
    {
        minFrequency = std::clamp(std::isfinite(minHz) ? minHz : 70.0f, 35.0f, 500.0f);
        maxFrequency = std::clamp(std::isfinite(maxHz) ? maxHz : 1000.0f, minFrequency + 20.0f, 1800.0f);
    }
    void reset() noexcept
    {
        history.fill(0.0f);
        write = 0;
        filled = 0;
        haveFirstDecimationSample = false;
        firstDecimationSample = 0.0f;
    }
    void push(float x) noexcept
    {
        const float sample = std::isfinite(x) ? std::clamp(x, -16.0f, 16.0f) : 0.0f;
        if (!haveFirstDecimationSample)
        {
            firstDecimationSample = sample;
            haveFirstDecimationSample = true;
            return;
        }
        // Two-tap box low-pass before downsampling. Pitch tracking uses the
        // fundamental/low harmonics; this suppresses content above the new Nyquist.
        const float decimated = 0.5f * (firstDecimationSample + sample);
        firstDecimationSample = 0.0f;
        haveFirstDecimationSample = false;
        history[(size_t)write] = decimated;
        write = (write + 1) % capacity;
        filled = std::min(filled + 1, capacity);
    }

    Result analyse() const noexcept
    {
        Result result;
        // sampleRate is the effective (2x decimated) rate. Keep approximately
        // 85–95 ms of history: 2048 points up to 24 kHz, 4096 points above it.
        // This still spans >=4 periods near 45 Hz and avoids the old ~100M-op/s path.
        const int window = sampleRate > 24000.0 ? capacity / 2 : capacity / 4;
        if (filled < window || !std::isfinite(sampleRate) || sampleRate <= 0.0) return result;
        const int minLag = std::max(2, (int)std::floor(sampleRate / maxFrequency));
        const int maxLag = std::min(window / 2 - 1, (int)std::ceil(sampleRate / minFrequency));
        if (maxLag <= minLag + 2) return result;

        // Copy the circular history once into chronological storage. The original
        // inner YIN loop called at() (including modulo arithmetic) millions of times
        // per analysis. A linear frame materially reduces audio-thread CPU cost.
        std::array<float, capacity> frame{};
        const int oldest = (write - window + capacity) % capacity;
        for (int i = 0; i < window; ++i)
            frame[(size_t)i] = history[(size_t)((oldest + i) % capacity)];

        // Remove DC offset and reject silence/very low-level noise before the quadratic work.
        double mean = 0.0;
        for (int i = 0; i < window; ++i) mean += frame[(size_t)i];
        mean /= window;
        double energy = 0.0;
        for (int i = 0; i < window; ++i)
        {
            const double x = (double)frame[(size_t)i] - mean;
            energy += x * x;
        }
        const double rms = std::sqrt(energy / window);
        if (!std::isfinite(rms) || rms < 2.0e-5) return result;

        std::array<float, capacity / 2> difference{};
        std::array<float, capacity / 2> cmnd{};
        // Correct YIN cumulative mean normalized difference: every lag from 1 onward
        // contributes to the running sum, not only lags inside the search range.
        double running = 0.0;
        cmnd[0] = 1.0f;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            double sum = 0.0;
            const int count = window - maxLag;
            for (int i = 0; i < count; ++i)
            {
                const double a = (double)frame[(size_t)i] - mean;
                const double b = (double)frame[(size_t)(i + tau)] - mean;
                const double d = a - b;
                sum += d * d;
            }
            difference[(size_t)tau] = (float)sum;
            running += sum;
            cmnd[(size_t)tau] = running > 1.0e-20
                ? (float)(sum * tau / running) : 1.0f;
        }

        int best = -1;
        constexpr float threshold = 0.15f;
        for (int tau = minLag; tau <= maxLag; ++tau)
        {
            if (cmnd[(size_t)tau] < threshold)
            {
                while (tau + 1 <= maxLag && cmnd[(size_t)(tau + 1)] < cmnd[(size_t)tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
        {
            best = minLag;
            for (int tau = minLag + 1; tau <= maxLag; ++tau)
                if (cmnd[(size_t)tau] < cmnd[(size_t)best]) best = tau;
        }

        // Parabolic interpolation around the local minimum gives sub-sample lag accuracy.
        float refined = (float)best;
        if (best > minLag && best < maxLag)
        {
            const float a = cmnd[(size_t)(best - 1)], b = cmnd[(size_t)best], c = cmnd[(size_t)(best + 1)];
            const float denom = a - 2.0f * b + c;
            if (std::abs(denom) > 1.0e-9f)
                refined += std::clamp(0.5f * (a - c) / denom, -0.5f, 0.5f);
        }
        if (!(refined > 0.0f) || !std::isfinite(refined)) return result;
        result.confidence = std::clamp(1.0f - cmnd[(size_t)best], 0.0f, 1.0f);
        result.hz = (float)(sampleRate / refined);
        // A separate periodicity threshold prevents low-energy noise being called voiced.
        result.voiced = result.confidence >= 0.65f && result.hz >= minFrequency * 0.90f
                     && result.hz <= maxFrequency * 1.10f && std::isfinite(result.hz);
        if (!result.voiced) result.hz = 0.0f;
        return result;
    }
private:
    float at(int i) const noexcept
    {
        const int oldest = (write - capacity + capacity) % capacity;
        return history[(oldest + i) % capacity];
    }
    std::array<float, capacity> history{};
    int write = 0, filled = 0;
    bool haveFirstDecimationSample = false;
    float firstDecimationSample = 0.0f;
    double sampleRate = 22050.0;
    float minFrequency = 70.0f, maxFrequency = 1000.0f;
};
