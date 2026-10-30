#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
class FRiKKTUNEAEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FRiKKTUNEAEditor(FRiKKTUNEAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override { repaint(); }
    FRiKKTUNEAudioProcessor& processor;
    juce::Label brand, note, status;
    juce::Label choiceLabels[4];
    juce::Label controlLabels[7];
    juce::Slider amount, speed, humanize, formant, wet, gain, referenceHz;
    juce::ComboBox key, scale, mode, range;
    juce::ToggleButton bypass;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachments[7];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> combos[4];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FRiKKTUNEAEditor)
};
