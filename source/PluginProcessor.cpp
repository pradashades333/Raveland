#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthVoice.h"

namespace
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        // Master
        params.push_back(std::make_unique<juce::AudioParameterFloat>("masterGain", "Master Gain",
                                                                     juce::NormalisableRange<float>(-24.0f, 6.0f), 0.0f));

        // Oscillators (3 oscillators)
        for (int i = 1; i <= 3; ++i)
        {
            auto prefix = "osc" + juce::String(i);
            params.push_back(std::make_unique<juce::AudioParameterBool>(prefix + "Enabled", prefix + " Enabled", i <= 2));
            params.push_back(std::make_unique<juce::AudioParameterInt>(prefix + "Voices", prefix + " Voices", 1, 32, i == 1 ? 16 : 12));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(prefix + "Detune", prefix + " Detune",
                                                                         juce::NormalisableRange<float>(0.0f, 100.0f), i == 1 ? 55.0f : 45.0f));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(prefix + "Level", prefix + " Level",
                                                                         juce::NormalisableRange<float>(0.0f, 1.0f), i == 1 ? 0.85f : 0.75f));
        }

        // Layers (3 sample layers)
        for (int i = 1; i <= 3; ++i)
        {
            auto prefix = "layer" + juce::String(i);
            params.push_back(std::make_unique<juce::AudioParameterBool>(prefix + "Enabled", prefix + " Enabled", i <= 2));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(prefix + "Gain", prefix + " Gain",
                                                                         juce::NormalisableRange<float>(0.0f, 1.0f), i == 1 ? 0.8f : 0.7f));
            params.push_back(std::make_unique<juce::AudioParameterInt>(prefix + "StartRand", prefix + " Start Rand", 0, 100, i == 1 ? 35 : 45));
        }

        // FX
        params.push_back(std::make_unique<juce::AudioParameterFloat>("reverbMix", "Reverb Mix",
                                                                     juce::NormalisableRange<float>(0.0f, 1.0f), 0.22f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("reverbSize", "Reverb Size",
                                                                     juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("reverbDamp", "Reverb Damp",
                                                                     juce::NormalisableRange<float>(0.0f, 1.0f), 0.35f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("delayMix", "Delay Mix",
                                                                     juce::NormalisableRange<float>(0.0f, 1.0f), 0.18f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("delayTime", "Delay Time",
                                                                     juce::NormalisableRange<float>(1.0f, 800.0f), 250.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("delayFeedback", "Delay Feedback",
                                                                     juce::NormalisableRange<float>(0.0f, 0.95f), 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusMix", "Chorus Mix",
                                                                     juce::NormalisableRange<float>(0.0f, 1.0f), 0.30f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusRate", "Chorus Rate",
                                                                     juce::NormalisableRange<float>(0.05f, 10.0f), 1.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("chorusDepth", "Chorus Depth",
                                                                     juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("distMix", "Distortion Mix",
                                                                     juce::NormalisableRange<float>(0.0f, 1.0f), 0.26f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("distDrive", "Distortion Drive",
                                                                     juce::NormalisableRange<float>(0.0f, 1.0f), 0.4f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("distTone", "Distortion Tone",
                                                                     juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));

        // Mono/Legato
        params.push_back(std::make_unique<juce::AudioParameterBool>("monoEnabled", "Mono Enabled", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>("legatoEnabled", "Legato Enabled", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>("portamento", "Portamento",
                                                                     juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

        // Oscillator retrigger (phase reset on each note-on)
        params.push_back(std::make_unique<juce::AudioParameterBool>("retrigger", "Retrigger", false));

        return { params.begin(), params.end() };
    }
}

RavelandAudioProcessor::RavelandAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMS", createParameterLayout())
{
    for (auto& flag : layerLoading)
        flag.store(false);
    struct SimpleSound : public juce::SynthesiserSound
    {
        bool appliesToNote (int) override      { return true; }
        bool appliesToChannel (int) override   { return true; }
    };

    constexpr int numVoices = 16;
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice(new RavelandVoice());

    synth.addSound(new SimpleSound());

    createFactoryPresets();
    loadPreset(0);
}

void RavelandAudioProcessor::createFactoryPresets()
{
    presetNames.clear();
    presetNames.add("INIT - Clean Saw Lead");
    presetNames.add("Rave - Wide SuperSaw Stack");
    presetNames.add("Trance - Tight JP-ish Pluck");
    presetNames.add("Hard Dance - Aggressive Stack");
}

int RavelandAudioProcessor::getNumPrograms()
{
    return presetNames.size();
}

int RavelandAudioProcessor::getCurrentProgram()
{
    return currentPresetIndex;
}

void RavelandAudioProcessor::setCurrentProgram(int index)
{
    if (index >= 0 && index < presetNames.size())
        loadPreset(index);
}

const juce::String RavelandAudioProcessor::getProgramName(int index)
{
    if (index >= 0 && index < presetNames.size())
        return presetNames[index];
    return {};
}

void RavelandAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    if (index >= 0 && index < presetNames.size())
        presetNames.set(index, newName);
}

juce::StringArray RavelandAudioProcessor::getPresetNames() const
{
    return presetNames;
}

void RavelandAudioProcessor::loadPreset(int index)
{
    currentPresetIndex = juce::jlimit(0, presetNames.size() - 1, index);

    // Factory presets
    if (index == 0) // INIT
    {
        *parameters.getRawParameterValue("masterGain") = 0.0f;
        *parameters.getRawParameterValue("osc1Enabled") = 1.0f;
        *parameters.getRawParameterValue("osc1Voices") = 16.0f;
        *parameters.getRawParameterValue("osc1Detune") = 50.0f;
        *parameters.getRawParameterValue("osc1Level") = 0.85f;
        *parameters.getRawParameterValue("osc2Enabled") = 1.0f;
        *parameters.getRawParameterValue("osc2Voices") = 12.0f;
        *parameters.getRawParameterValue("osc2Detune") = 45.0f;
        *parameters.getRawParameterValue("osc2Level") = 0.75f;
        *parameters.getRawParameterValue("osc3Enabled") = 0.0f;
        *parameters.getRawParameterValue("reverbMix") = 0.22f;
        *parameters.getRawParameterValue("delayMix") = 0.18f;
        *parameters.getRawParameterValue("chorusMix") = 0.30f;
    }
    else if (index == 1) // Rave
    {
        *parameters.getRawParameterValue("masterGain") = 0.0f;
        *parameters.getRawParameterValue("osc1Enabled") = 1.0f;
        *parameters.getRawParameterValue("osc1Voices") = 24.0f;
        *parameters.getRawParameterValue("osc1Detune") = 72.0f;
        *parameters.getRawParameterValue("osc1Level") = 0.88f;
        *parameters.getRawParameterValue("osc2Enabled") = 1.0f;
        *parameters.getRawParameterValue("osc2Voices") = 24.0f;
        *parameters.getRawParameterValue("osc2Detune") = 78.0f;
        *parameters.getRawParameterValue("osc2Level") = 0.78f;
        *parameters.getRawParameterValue("osc3Enabled") = 1.0f;
        *parameters.getRawParameterValue("osc3Voices") = 16.0f;
        *parameters.getRawParameterValue("osc3Detune") = 60.0f;
        *parameters.getRawParameterValue("osc3Level") = 0.62f;
        *parameters.getRawParameterValue("reverbMix") = 0.28f;
        *parameters.getRawParameterValue("delayMix") = 0.26f;
        *parameters.getRawParameterValue("chorusMix") = 0.55f;
    }
    else if (index == 2) // Trance
    {
        *parameters.getRawParameterValue("masterGain") = 0.0f;
        *parameters.getRawParameterValue("osc1Enabled") = 1.0f;
        *parameters.getRawParameterValue("osc1Voices") = 12.0f;
        *parameters.getRawParameterValue("osc1Detune") = 40.0f;
        *parameters.getRawParameterValue("osc1Level") = 0.80f;
        *parameters.getRawParameterValue("osc2Enabled") = 0.0f;
        *parameters.getRawParameterValue("osc3Enabled") = 1.0f;
        *parameters.getRawParameterValue("osc3Voices") = 8.0f;
        *parameters.getRawParameterValue("osc3Detune") = 18.0f;
        *parameters.getRawParameterValue("osc3Level") = 0.55f;
        *parameters.getRawParameterValue("reverbMix") = 0.14f;
        *parameters.getRawParameterValue("delayMix") = 0.18f;
        *parameters.getRawParameterValue("chorusMix") = 0.25f;
        *parameters.getRawParameterValue("monoEnabled") = 1.0f;
        *parameters.getRawParameterValue("legatoEnabled") = 1.0f;
        *parameters.getRawParameterValue("portamento") = 0.55f;
    }
    else if (index == 3) // Hard Dance
    {
        *parameters.getRawParameterValue("masterGain") = 2.0f;
        *parameters.getRawParameterValue("osc1Enabled") = 1.0f;
        *parameters.getRawParameterValue("osc1Voices") = 20.0f;
        *parameters.getRawParameterValue("osc1Detune") = 65.0f;
        *parameters.getRawParameterValue("osc1Level") = 0.90f;
        *parameters.getRawParameterValue("osc2Enabled") = 1.0f;
        *parameters.getRawParameterValue("osc2Voices") = 18.0f;
        *parameters.getRawParameterValue("osc2Detune") = 70.0f;
        *parameters.getRawParameterValue("osc2Level") = 0.85f;
        *parameters.getRawParameterValue("osc3Enabled") = 0.0f;
        *parameters.getRawParameterValue("reverbMix") = 0.20f;
        *parameters.getRawParameterValue("delayMix") = 0.15f;
        *parameters.getRawParameterValue("chorusMix") = 0.40f;
        *parameters.getRawParameterValue("distMix") = 0.35f;
    }
}

void RavelandAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    synth.setCurrentPlaybackSampleRate(sampleRate);

    // Build pointer array once — these addresses are stable for the plugin lifetime
    std::array<SampleLayer*, 3> layerPtrs { &sampleLayers[0], &sampleLayers[1], &sampleLayers[2] };

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* v = dynamic_cast<RavelandVoice*>(synth.getVoice(i)))
        {
            v->prepare(spec);
            v->setSampleLayers(layerPtrs);
        }
    }

    chorus.prepare(spec);
    delay.prepare(spec);
    distortion.prepare(spec);

    juce::Reverb::Parameters params;
    params.roomSize = 0.5f;
    params.damping = 0.35f;
    params.wetLevel = 0.22f;
    params.dryLevel = 1.0f;
    params.width = 0.85f;
    reverb.setSampleRate(sampleRate);
    reverb.setParameters(params);
}

void RavelandAudioProcessor::releaseResources()
{
}

bool RavelandAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void RavelandAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    // Push current APVTS parameter values into every voice before rendering.
    // This is the connection between the UI knobs and the actual oscillator sound.
    const bool retrigOn = parameters.getRawParameterValue("retrigger")->load() > 0.5f;

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* v = dynamic_cast<RavelandVoice*>(synth.getVoice(i)))
        {
            v->setRetrigger(retrigOn);

            for (int osc = 0; osc < 3; ++osc)
            {
                auto prefix = "osc" + juce::String(osc + 1);
                v->setOscParameters(
                    osc,
                    parameters.getRawParameterValue(prefix + "Enabled")->load() > 0.5f,
                    (int) parameters.getRawParameterValue(prefix + "Voices")->load(),
                    parameters.getRawParameterValue(prefix + "Detune")->load(),
                    parameters.getRawParameterValue(prefix + "Level")->load());
            }

            for (int l = 0; l < 3; ++l)
            {
                auto prefix = "layer" + juce::String(l + 1);
                const bool enabled = (parameters.getRawParameterValue(prefix + "Enabled")->load() > 0.5f)
                                     && !layerLoading[l].load();
                v->setLayerParameters(
                    l,
                    enabled,
                    parameters.getRawParameterValue(prefix + "Gain")->load());
            }
        }
    }

    // Render synth voices (oscillators + sample layers handled inside each voice)
    synth.renderNextBlock(buffer, midi, 0, buffer.getNumSamples());

    // FX
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    // Chorus
    chorus.setMix(parameters.getRawParameterValue("chorusMix")->load());
    chorus.setRate(parameters.getRawParameterValue("chorusRate")->load());
    chorus.setDepth(parameters.getRawParameterValue("chorusDepth")->load());
    chorus.process(context);

    // Delay + Distortion
    const auto delayMix = parameters.getRawParameterValue("delayMix")->load();
    const auto delayTime = parameters.getRawParameterValue("delayTime")->load();
    const auto delayFeedback = parameters.getRawParameterValue("delayFeedback")->load();
    const auto distMix = parameters.getRawParameterValue("distMix")->load();
    const auto distDrive = parameters.getRawParameterValue("distDrive")->load();
    const auto distTone = parameters.getRawParameterValue("distTone")->load();

    // Calculate delay in samples (simplified - using fixed delay for now)
    const int delaySamples = static_cast<int>(spec.sampleRate * delayTime / 1000.0f);
    const int maxDelaySamples = 44100; // 1 second at 44.1kHz
    const int clampedDelaySamples = juce::jlimit(1, maxDelaySamples, delaySamples);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer(channel);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            // Simple delay implementation using the DelayLine
            auto delayed = delay.popSample(channel, clampedDelaySamples) * delayFeedback;
            delay.pushSample(channel, data[i] + delayed);

            // Apply distortion with drive and tone
            auto distInput = delayed * (1.0f + distDrive * 4.0f);
            auto dist = distortion.processSample(distInput);
            // Simple tone control (high shelf)
            dist = dist * (1.0f + distTone * 0.5f) + dist * distTone * 0.3f;

            auto wet = data[i] * (1.0f - distMix) + dist * distMix;
            data[i] = data[i] * (1.0f - delayMix) + wet * delayMix;
        }
    }

    // Reverb
    if (buffer.getNumChannels() >= 2)
    {
        auto mix = parameters.getRawParameterValue("reverbMix")->load();
        auto size = parameters.getRawParameterValue("reverbSize")->load();
        auto damp = parameters.getRawParameterValue("reverbDamp")->load();

        auto params = reverb.getParameters();
        params.wetLevel = mix;
        params.roomSize = size;
        params.damping = damp;
        reverb.setParameters(params);
        reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());
    }

    const auto masterGainDb = parameters.getRawParameterValue("masterGain")->load();
    const auto gain = juce::Decibels::decibelsToGain(masterGainDb);
    buffer.applyGain(gain);
}

juce::AudioProcessorEditor* RavelandAudioProcessor::createEditor()
{
    return new RavelandAudioProcessorEditor(*this);
}

void RavelandAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void RavelandAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

void RavelandAudioProcessor::loadSampleLayer (int layerIndex, const juce::File& folder)
{
    if (layerIndex < 0 || layerIndex >= 3)
        return;

    // Signal the audio thread to skip this layer while we load
    layerLoading[layerIndex].store(true);

    // Load on the calling (message) thread — this can take a moment for large sample sets
    sampleLayers[layerIndex].loadFromFolder(folder, spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0);
    layerFolderNames[layerIndex] = folder.getFileName();

    // Re-enable the layer in the audio thread
    layerLoading[layerIndex].store(false);
}

juce::String RavelandAudioProcessor::getLayerFolderName (int layerIndex) const
{
    if (layerIndex < 0 || layerIndex >= 3)
        return {};
    return layerFolderNames[layerIndex];
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RavelandAudioProcessor();
}
