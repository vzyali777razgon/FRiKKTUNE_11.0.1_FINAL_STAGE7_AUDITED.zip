#pragma once
#include <cmath>
#include <algorithm>

// Rejects isolated large F0 jumps without permanently suppressing legitimate
// melodic leaps. Octave-like jumps receive special treatment because harmonic
// trackers can report them with deceptively high confidence. Those jumps must be
// confirmed by a second consistent frame; other high-confidence leaps pass at once.
class PitchJumpGate
{
public:
    bool accept(float midi, float confidence, float previousMidi) noexcept
    {
        if (!std::isfinite(midi) || !std::isfinite(confidence))
        {
            resetPending();
            return false;
        }
        const float jump = std::abs(midi - previousMidi);
        const bool havePrevious = std::isfinite(previousMidi) && previousMidi >= 0.0f;
        const bool octaveLikeJump = havePrevious && isOctaveLikeJump(jump);
        if (!havePrevious || jump <= largeJumpSemitones
            || (confidence >= immediateConfidence && !octaveLikeJump))
        {
            resetPending();
            return true;
        }

        if (pendingMidi >= 0.0f && std::abs(midi - pendingMidi) <= consistencySemitones)
        {
            ++pendingCount;
            pendingMidi = 0.5f * (pendingMidi + midi);
            if (pendingCount >= requiredConsistentFrames)
            {
                resetPending();
                return true;
            }
        }
        else
        {
            pendingMidi = midi;
            pendingCount = 1;
        }
        return false;
    }

    void resetPending() noexcept { pendingMidi = -1.0f; pendingCount = 0; }

private:
    static bool isOctaveLikeJump(float semitones) noexcept
    {
        // Fundamental/second harmonic confusion is usually +/-12 semitones;
        // third-harmonic confusion is near +/-19 and two-octave errors near +/-24.
        return std::abs(semitones - 12.0f) <= 1.25f
            || std::abs(semitones - 19.0f) <= 1.25f
            || std::abs(semitones - 24.0f) <= 1.25f;
    }

    static constexpr float largeJumpSemitones = 7.0f;
    static constexpr float immediateConfidence = 0.92f;
    static constexpr float consistencySemitones = 1.5f;
    static constexpr int requiredConsistentFrames = 2;
    float pendingMidi = -1.0f;
    int pendingCount = 0;
};
