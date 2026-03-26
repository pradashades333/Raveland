#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "WaveformDisplay.h"
#include "FancyKnob.h"

class RavelandAudioProcessor;

// ─────────────────────────────────────────────────────────────────────────────
// Unified toggle look-and-feel: teal tick, gold text, consistent pill shape.
// Applied to every ToggleButton in the plugin.
// ─────────────────────────────────────────────────────────────────────────────
class RavelandToggleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override
    {
        const auto bounds  = button.getLocalBounds().toFloat();
        const float h      = bounds.getHeight();
        const float boxSz  = juce::jmin (h * 0.65f, 16.0f);
        const float boxX   = 4.0f;
        const float boxY   = (h - boxSz) * 0.5f;

        static const juce::Colour kAccent  = juce::Colour::fromString ("0xff00d4ff");
        static const juce::Colour kGold    = juce::Colour::fromString ("0xffd4af37");
        static const juce::Colour kSurface = juce::Colour::fromRGB (22, 22, 28);

        // Box background
        g.setColour (button.getToggleState() ? kAccent.withAlpha (0.18f) : kSurface);
        g.fillRoundedRectangle (boxX, boxY, boxSz, boxSz, 3.0f);

        // Box border — teal when on, muted gold when off
        g.setColour (button.getToggleState()
                         ? kAccent
                         : kGold.withAlpha (shouldDrawButtonAsHighlighted ? 0.7f : 0.35f));
        g.drawRoundedRectangle (boxX, boxY, boxSz, boxSz, 3.0f, 1.5f);

        // Tick (check mark) when toggled on
        if (button.getToggleState())
        {
            g.setColour (kAccent);
            const float m = boxSz * 0.18f;
            juce::Path tick;
            tick.startNewSubPath (boxX + m,           boxY + boxSz * 0.55f);
            tick.lineTo          (boxX + boxSz * 0.4f, boxY + boxSz - m);
            tick.lineTo          (boxX + boxSz - m,    boxY + m);
            g.strokePath (tick, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        // Label text
        g.setColour (button.getToggleState() ? kAccent : kGold.withAlpha (0.85f));
        g.setFont (juce::Font (juce::jmin (h * 0.55f, 11.0f), juce::Font::bold)
                       .withExtraKerningFactor (0.04f));
        g.drawText (button.getButtonText(),
                    juce::Rectangle<float> (boxX + boxSz + 6.0f, 0.0f,
                                            bounds.getWidth() - boxX - boxSz - 8.0f, h),
                    juce::Justification::centredLeft, false);
    }
};

class RavelandAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::Timer
{
public:
    explicit RavelandAudioProcessorEditor(RavelandAudioProcessor&);
    ~RavelandAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override { repaint(); }

private:
    RavelandAudioProcessor& processor;

    // Shared look-and-feel for all toggle buttons
    RavelandToggleLookAndFeel toggleLnf;

    // Animated glow effect
    float glowPhase = 0.0f;

    // Logos
    juce::Image ravelandLogoImage;
    juce::Image nsAudioLogoImage;

    // Waveform displays
    std::array<std::unique_ptr<WaveformDisplay>, 3> oscWaveforms;
    std::array<std::unique_ptr<WaveformDisplay>, 3> layerWaveforms;
    juce::Label oscWaveformLabel1, oscWaveformLabel2, oscWaveformLabel3;
    juce::Label layerWaveformLabel1, layerWaveformLabel2, layerWaveformLabel3;

    // Preset browser
    juce::ComboBox presetCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAttachment;

    // Master
    FancyKnob masterGainSlider;
    juce::Label masterGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterGainAttachment;

    // Oscillators (3)
    struct OscControls
    {
        juce::ToggleButton enabled;
        FancyKnob voices, detune, level;
        juce::Label voicesLabel, detuneLabel, levelLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> voicesAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> detuneAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
    };
    std::array<OscControls, 3> oscControls;

    // Layers (3)
    struct LayerControls
    {
        juce::ToggleButton enabled;
        juce::TextButton   loadButton { "LOAD" };
        juce::Label        folderLabel;
        FancyKnob gain, startRand;
        juce::Label gainLabel, startRandLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> startRandAttachment;
    };
    std::array<LayerControls, 3> layerControls;

    // FX
    FancyKnob reverbMixSlider, delayMixSlider, chorusMixSlider, distMixSlider;
    FancyKnob reverbSizeSlider, reverbDampSlider, delayTimeSlider, delayFeedbackSlider;
    FancyKnob chorusRateSlider, chorusDepthSlider, distDriveSlider, distToneSlider;
    juce::Label reverbMixLabel, delayMixLabel, chorusMixLabel, distMixLabel;
    juce::Label reverbSizeLabel, reverbDampLabel, delayTimeLabel, delayFeedbackLabel;
    juce::Label chorusRateLabel, chorusDepthLabel, distDriveLabel, distToneLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chorusMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbSizeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbDampAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayTimeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayFeedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chorusRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chorusDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distDriveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distToneAttachment;

    // Mono/Legato
    juce::ToggleButton monoButton, legatoButton;
    FancyKnob portamentoSlider;
    juce::Label portamentoLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> legatoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> portamentoAttachment;

    // Retrigger
    juce::ToggleButton retriggerButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> retriggerAttachment;

    void setupToggle(juce::ToggleButton& button);
    void loadLogos();
    void drawNeonGlow(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawPanelWithGlow(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& title);
    void drawRaveLandLogo(juce::Graphics& g, juce::Rectangle<float> bounds);

    // Layout functions
    void layoutLayerSection(juce::Rectangle<float> area);
    void layoutFXSection(juce::Rectangle<float> area);
    void layoutFXBlock(juce::Rectangle<float>& area, int blockIndex, float height,
                      std::array<FancyKnob*, 3> knobs, std::array<juce::Label*, 3> labels);
    void layoutOscillatorSection(juce::Rectangle<float> area);
    void layoutFooterSection(juce::Rectangle<float> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RavelandAudioProcessorEditor)
};
