#include "PluginEditor.h"
FRiKKTUNEAEditor::FRiKKTUNEAEditor(FRiKKTUNEAudioProcessor& p) : AudioProcessorEditor(&p), processor(p)
{
    setSize(960, 680); setResizable(true, true); setResizeLimits(820, 640, 1280, 900);
    brand.setText("#FRiKKTUNE", juce::dontSendNotification); brand.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold))); brand.setColour(juce::Label::textColourId, juce::Colour(0xffe8f4ff)); addAndMakeVisible(brand);
    note.setJustificationType(juce::Justification::centred); note.setFont(juce::Font(juce::FontOptions(42.0f, juce::Font::bold))); note.setColour(juce::Label::textColourId, juce::Colour(0xff54f5d2)); addAndMakeVisible(note);
    status.setJustificationType(juce::Justification::centred); status.setColour(juce::Label::textColourId, juce::Colour(0xffa6b5c8)); addAndMakeVisible(status);
    juce::Slider* ss[] = {&amount,&speed,&humanize,&formant,&wet,&gain,&referenceHz};
    const char* sliderTips[] = {
        "Amount of pitch correction",
        "Higher values retune faster",
        "Preserve expressive pitch movement",
        "Amount of spectral-envelope formant compensation",
        "Blend corrected and dry signal",
        "Output level in decibels",
        "Tuning reference for A4"
    };
    const char* ids[] = {"amount","speed","humanize","formant","wet","gain","reference"};
    for (int i=0;i<7;++i) {
        ss[i]->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        ss[i]->setTooltip(sliderTips[i]);
        ss[i]->setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,20);
        ss[i]->setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xff54f5d2));
        ss[i]->setColour(juce::Slider::thumbColourId,juce::Colour(0xffe8f4ff));
        addAndMakeVisible(*ss[i]);
        attachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,ids[i],*ss[i]);
    }
    const char* controlNames[] = {"CORRECTION", "RETUNE SPEED", "HUMANIZE", "FORMANT", "DRY / WET", "OUTPUT GAIN", "DETUNE / A4 (Hz)"};
    for (int i = 0; i < 7; ++i)
    {
        controlLabels[i].setText(controlNames[i], juce::dontSendNotification);
        controlLabels[i].setJustificationType(juce::Justification::centred);
        controlLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xffa6b5c8));
        controlLabels[i].setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        addAndMakeVisible(controlLabels[i]);
    }
    referenceHz.setNumDecimalPlacesToDisplay(1);
    referenceHz.setTextValueSuffix(" Hz");
    juce::ComboBox* cc[]={&key,&scale,&mode,&range};
    const char* cids[]={"key","scale","mode","range"};
    const char* choiceNames[]={"KEY","SCALE","MODE","INPUT RANGE"};
    for (int i=0; i<4; ++i)
    {
        // A ComboBoxAttachment binds an existing ComboBox; it does not reliably
        // create the visible choice list for every JUCE version. Populate from
        // the parameter itself so displayed indices always match the APVTS choices.
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(processor.parameters.getParameter(cids[i])))
        {
            const auto values = choice->choices;
            for (int item = 0; item < values.size(); ++item)
                cc[i]->addItem(values[item], item + 1);
        }
        cc[i]->setTooltip(juce::String(choiceNames[i]) + " parameter");
        cc[i]->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff151d28));
        cc[i]->setColour(juce::ComboBox::textColourId, juce::Colour(0xffe8f4ff));
        cc[i]->setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff344354));
        cc[i]->setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff54f5d2));
        choiceLabels[i].setText(choiceNames[i], juce::dontSendNotification);
        choiceLabels[i].setJustificationType(juce::Justification::centredLeft);
        choiceLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xffa6b5c8));
        choiceLabels[i].setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        addAndMakeVisible(choiceLabels[i]);
        addAndMakeVisible(*cc[i]);
        combos[i]=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters,cids[i],*cc[i]);
    }
    bypass.setButtonText("BYPASS"); bypass.setTooltip("Switch to the latency-aligned dry signal");
    bypass.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffe8f4ff));
    bypass.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff54f5d2));
    bypass.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff536274));
    addAndMakeVisible(bypass); bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"bypass",bypass);
    startTimerHz(20);
}
void FRiKKTUNEAEditor::paint(juce::Graphics& g)
{
    const auto bg = juce::Colour(0xff0a0f16);
    const auto panel = juce::Colour(0xff111a25);
    const auto panelRaised = juce::Colour(0xff172331);
    const auto line = juce::Colour(0xff2b3b4d);
    const auto accent = juce::Colour(0xff54f5d2);
    const auto text = juce::Colour(0xffe8f4ff);
    const auto muted = juce::Colour(0xff91a4b9);

    g.fillAll(bg);
    auto bounds = getLocalBounds().toFloat().reduced(14.0f);
    g.setColour(panel);
    g.fillRoundedRectangle(bounds, 18.0f);

    // Header panel and status dashboard
    auto header = juce::Rectangle<float>(28.0f, 24.0f, (float)getWidth()-56.0f, 92.0f);
    g.setColour(panelRaised);
    g.fillRoundedRectangle(header, 13.0f);
    g.setColour(accent.withAlpha(0.85f));
    g.fillRoundedRectangle(28.0f, 24.0f, 4.0f, 92.0f, 2.0f);

    g.setColour(text);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText("REAL-TIME VOCAL TUNING", 48, 31, 280, 18, juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(10.5f)));
    g.drawText("PITCH ENGINE  /  FORMANT CONTROL", 48, 52, 300, 16, juce::Justification::centredLeft);

    // Main pitch readout card
    g.setColour(panelRaised.withAlpha(0.78f));
    g.fillRoundedRectangle((float)getWidth()-230.0f, 35.0f, 178.0f, 70.0f, 11.0f);
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.drawText("DETECTED NOTE", getWidth()-218, 41, 154, 15, juce::Justification::centred);
    const float midi = processor.detectedMidi();
    static const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    const auto noteText = midi >= 0.0f
        ? juce::String(names[((int)std::lround(midi)%12+12)%12]) + juce::String((int)std::floor(midi/12.0f)-1)
        : juce::String("--");
    note.setText(noteText, juce::dontSendNotification);
    g.setColour(accent);
    g.setFont(juce::Font(juce::FontOptions(27.0f, juce::Font::bold)));
    g.drawText(noteText, getWidth()-218, 56, 154, 35, juce::Justification::centred);

    // Section titles, subtle separators and live telemetry
    g.setColour(text);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("PITCH & FORMANT CONTROLS", 32, 132, 260, 20, juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.drawText("Set correction strength, response, formant preservation, mix and output level.", 32, 151, getWidth()-64, 17, juce::Justification::centredLeft);

    g.setColour(line);
    g.drawRoundedRectangle(28.0f, 177.0f, (float)getWidth()-56.0f, 215.0f, 12.0f, 1.0f);
    g.drawRoundedRectangle(28.0f, 407.0f, (float)getWidth()-56.0f, 106.0f, 12.0f, 1.0f);
    g.setColour(text);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("MUSICAL CONTEXT", 42, 414, 180, 18, juce::Justification::centredLeft);

    // Live analysis strip
    const float conf = juce::jlimit(0.0f, 1.0f, processor.confidence());
    const float peak = juce::jlimit(0.0f, 1.0f, processor.inputPeak());
    g.setColour(panelRaised);
    g.fillRoundedRectangle(28.0f, (float)getHeight()-83.0f, (float)getWidth()-56.0f, 42.0f, 10.0f);
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.drawText("PITCH CONFIDENCE", 42, getHeight()-75, 135, 14, juce::Justification::centredLeft);
    g.drawText("INPUT LEVEL", getWidth()/2-45, getHeight()-75, 105, 14, juce::Justification::centredLeft);
    g.setColour(line);
    g.fillRoundedRectangle(42.0f, (float)getHeight()-57.0f, 150.0f, 6.0f, 3.0f);
    g.fillRoundedRectangle((float)getWidth()/2-45.0f, (float)getHeight()-57.0f, 150.0f, 6.0f, 3.0f);
    g.setColour(accent);
    g.fillRoundedRectangle(42.0f, (float)getHeight()-57.0f, 150.0f*conf, 6.0f, 3.0f);
    g.setColour(juce::Colour(0xff77a9ff));
    g.fillRoundedRectangle((float)getWidth()/2-45.0f, (float)getHeight()-57.0f, 150.0f*peak, 6.0f, 3.0f);

    status.setText("CONFIDENCE  " + juce::String(conf*100.0f,0) + "%     TARGET OFFSET  " +
                   juce::String(processor.correctionCents(),1) + " cents",
                   juce::dontSendNotification);
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.drawText("FRiKK AUDIO   •   EXPERIMENTAL DSP   •   VST3", 32, getHeight()-31, getWidth()-64, 14, juce::Justification::centredLeft);
}
void FRiKKTUNEAEditor::resized()
{
    auto area = getLocalBounds();
    const int margin = 32;
    // Keep the visual hierarchy consistent while resizing.
    auto controls = juce::Rectangle<int>(margin, 188, getWidth()-2*margin, 190);
    const int sliderCount = 7;
    const int cellWidth = controls.getWidth() / sliderCount;
    juce::Slider* sliders[] = {&amount,&speed,&humanize,&formant,&wet,&gain,&referenceHz};
    for (int i = 0; i < sliderCount; ++i)
    {
        auto cell = juce::Rectangle<int>(controls.getX()+i*cellWidth, controls.getY(), i == sliderCount-1 ? controls.getRight()-(controls.getX()+i*cellWidth) : cellWidth, controls.getHeight()).reduced(5, 0);
        controlLabels[i].setBounds(cell.removeFromTop(22));
        sliders[i]->setBounds(cell);
    }

    const int comboY = 459;
    const int gap = 10;
    const int totalWidth = getWidth()-2*margin;
    const int widths[] = {totalWidth*15/100, totalWidth*23/100, totalWidth*24/100, totalWidth*38/100};
    juce::ComboBox* combosList[] = {&key,&scale,&mode,&range};
    juce::Label* labels[] = {&choiceLabels[0],&choiceLabels[1],&choiceLabels[2],&choiceLabels[3]};
    int x = margin;
    for (int i = 0; i < 4; ++i)
    {
        const int w = widths[i] - (i < 3 ? gap : 0);
        labels[i].setBounds(x, comboY-18, w, 15);
        combosList[i]->setBounds(x, comboY, w, 30);
        x += widths[i];
    }
    bypass.setBounds(getWidth()-margin-110, 486, 110, 26);
}
juce::AudioProcessorEditor* FRiKKTUNEAudioProcessor::createEditor() { return new FRiKKTUNEAEditor(*this); }
