#pragma once
#include <array>
#include <cmath>
#include <algorithm>

// Musical note targeting kept separate from pitch estimation and audio rendering.
class NoteQuantizer
{
public:
    enum class Scale { Chromatic = 0, Major = 1, Minor = 2 };

    // Frequency-based tuning reference: 440 Hz is conventional A4, but users may
    // enter values such as 445 or 450 Hz directly. This avoids ambiguous cents UI.
    static float frequencyToMidi(float hz, float referenceHz) noexcept
    {
        if (!std::isfinite(hz) || !std::isfinite(referenceHz) || hz <= 0.0f || referenceHz <= 0.0f)
            return -1.0f;
        return 69.0f + 12.0f * std::log2(hz / referenceHz);
    }

    static float midiToFrequency(float midi, float referenceHz) noexcept
    {
        if (!std::isfinite(midi) || !std::isfinite(referenceHz) || referenceHz <= 0.0f)
            return 0.0f;
        const float hz = referenceHz * std::exp2((midi - 69.0f) / 12.0f);
        return std::isfinite(hz) ? hz : 0.0f;
    }

    static int nearestAllowedMidi(float midi, int key, Scale scale,
                                  const std::array<bool, 12>& enabled) noexcept
    {
        static constexpr std::array<bool, 12> major {{
            true, false, true, false, true, true, false, true, false, true, false, true
        }};
        static constexpr std::array<bool, 12> minor {{
            true, false, true, true, false, true, false, true, true, false, true, false
        }};
        const int center = (int)std::lround(midi);
        int best = center;
        float bestDistance = 1.0e9f;
        for (int note = center - 7; note <= center + 7; ++note)
        {
            const int pc = ((note - key) % 12 + 12) % 12;
            const bool inScale = scale == Scale::Chromatic
                || (scale == Scale::Major ? major[(size_t)pc] : minor[(size_t)pc]);
            if (!enabled[(size_t)((note % 12 + 12) % 12)] || !inScale)
                continue;
            const float distance = std::abs((float)note - midi);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = note;
            }
        }
        return best;
    }

    // Fine tune is an offset to the target pitch: positive values must raise the output.
    static float correctionTargetCents(float inputMidi, float targetMidi,
                                       float amount01, float fineTuneCents) noexcept
    {
        if (!std::isfinite(inputMidi) || !std::isfinite(targetMidi)
            || !std::isfinite(amount01) || !std::isfinite(fineTuneCents))
            return 0.0f;
        return ((targetMidi - inputMidi) * 100.0f + fineTuneCents)
             * std::clamp(amount01, 0.0f, 1.0f);
    }

    // One-pole smoothing of target correction in cents. Retune time is milliseconds.
    static float smoothCents(float current, float target, float retuneMs,
                             double sampleRate, int samples) noexcept
    {
        const float ms = std::clamp(retuneMs, 0.0f, 100.0f);
        if (ms <= 0.1f || sampleRate <= 0.0)
            return target;
        const float coefficient = std::exp((float)(-samples / (sampleRate * (ms * 0.001))));
        return target + coefficient * (current - target);
    }
};
