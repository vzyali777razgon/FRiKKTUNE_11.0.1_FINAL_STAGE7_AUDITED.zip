#pragma once
#include <cmath>
#include <algorithm>
#include <array>

// Hysteretic, time-confirmed note selection. Prevents target-note chatter when
// a tracked pitch wobbles around the midpoint between two allowed notes.
class NoteTargetTracker
{
public:
    int update(float midi, int proposedNote, int confirmationFrames,
               float switchingMarginSemitones = 0.30f,
               bool decisiveTransition = false) noexcept
    {
        if (!std::isfinite(midi) || proposedNote < 0)
            return targetNote;

        confirmationFrames = std::clamp(confirmationFrames, 1, 12);
        switchingMarginSemitones = std::clamp(switchingMarginSemitones, 0.0f, 1.0f);

        if (targetNote < 0)
        {
            targetNote = proposedNote;
            resetCandidate();
            return targetNote;
        }

        if (proposedNote == targetNote)
        {
            resetCandidate();
            return targetNote;
        }

        const float oldDistance = std::abs(midi - static_cast<float>(targetNote));
        const float newDistance = std::abs(midi - static_cast<float>(proposedNote));
        // Require the new note to be meaningfully closer, not merely one rounding
        // fluctuation across the midpoint.
        if (newDistance + switchingMarginSemitones >= oldDistance)
        {
            resetCandidate();
            return targetNote;
        }

        if (candidateNote == proposedNote)
            ++candidateFrames;
        else
        {
            candidateNote = proposedNote;
            candidateFrames = 1;
        }

        // A confident, clearly established melodic transition may bypass the
        // normal multi-frame confirmation. The distance/margin test above still
        // has to prove that the new target is substantially closer, preventing
        // a high-confidence vibrato excursion from flipping the target.
        if (decisiveTransition || candidateFrames >= confirmationFrames)
        {
            targetNote = candidateNote;
            resetCandidate();
        }
        return targetNote;
    }

    int current() const noexcept { return targetNote; }
    void reset() noexcept { targetNote = -1; resetCandidate(); }

private:
    void resetCandidate() noexcept { candidateNote = -1; candidateFrames = 0; }
    int targetNote = -1;
    int candidateNote = -1;
    int candidateFrames = 0;
};
