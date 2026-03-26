#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SampleLayer.h"

// ─────────────────────────────────────────────────────────────────────────────
//  PolyBLEP helper — bandlimited correction at the sawtooth discontinuity.
//  t  : normalised phase [0, 1)
//  dt : normalised phase increment per sample (freq / sampleRate)
// ─────────────────────────────────────────────────────────────────────────────
static inline double polyBlep (double t, double dt) noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

// ─────────────────────────────────────────────────────────────────────────────
/** Supersaw-style oscillator — bandlimited sawtooth (polyBLEP) with
 *  pitch-correct cent-based detune.  Matches the detune behaviour of
 *  Serum / Vital / Sylenth1.
 *
 *  Phase is stored normalised [0, 1).  That keeps the polyBLEP maths clean
 *  and avoids trig calls entirely.
 */
class SupersawOsc
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        // Spread initial phases evenly so voices don't all start in sync.
        // Free-running from here on (no click on first note-on).
        for (int i = 0; i < maxVoices; ++i)
            detunedPhase[i] = (double) i / (double) maxVoices;
    }

    /** Hard retrigger: reset voices to evenly distributed phases.
     *  Evenly distributed (not all-zero) avoids the sharp click that
     *  occurs when every voice hits its discontinuity simultaneously. */
    void resetPhases()
    {
        for (int i = 0; i < maxVoices; ++i)
            detunedPhase[i] = (double) i / (double) maxVoices;
    }

    void setFrequency  (float hz)     { freq = (double) hz; }
    /** detuneCents: total spread in cents (0 = unison, 100 = ±50 ct). */
    void setDetuneCents (float cents) { detuneCents = (double) cents; }
    void setNumVoices  (int v)        { numVoices = juce::jlimit (1, maxVoices, v); }
    void setGain       (float g)      { gain = g; }

    float processSample() noexcept
    {
        if (sr <= 0.0) return 0.0f;

        double acc = 0.0;

        for (int i = 0; i < numVoices; ++i)
        {
            // Distribute voices linearly across ±detuneCents/2
            double centsOff = 0.0;
            if (numVoices > 1)
                centsOff = detuneCents * ((double) i / (double) (numVoices - 1) - 0.5);

            // Pitch-correct ratio: 2^(cents/1200)
            const double ratio = std::pow (2.0, centsOff / 1200.0);
            const double dt    = (freq * ratio) / sr;   // normalised increment

            // Advance phase
            detunedPhase[i] += dt;
            if (detunedPhase[i] >= 1.0)
                detunedPhase[i] -= 1.0;

            // Bandlimited sawtooth via polyBLEP
            const double t   = detunedPhase[i];
            double saw       = 2.0 * t - 1.0;           // naive saw [-1, 1]
            saw             -= polyBlep (t, dt);         // remove alias at jump

            acc += saw;
        }

        // Normalise by voice count and apply gain
        return static_cast<float> (acc / (double) numVoices) * gain;
    }

private:
    static constexpr int maxVoices = 32;

    double sr          { 0.0 };
    double freq        { 440.0 };
    double detuneCents { 0.0 };
    int    numVoices   { 8 };
    float  gain        { 0.7f };

    std::array<double, maxVoices> detunedPhase {};
};

// ─────────────────────────────────────────────────────────────────────────────
/** One polyphonic voice: 3 supersaw oscillators + 3 sample layers. */
class RavelandVoice : public juce::SynthesiserVoice
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        for (auto& o : oscs)
            o.prepare (spec.sampleRate);

        adsr.setSampleRate (spec.sampleRate);

        juce::ADSR::Parameters p;
        p.attack  = 0.002f;
        p.decay   = 0.12f;
        p.sustain = 0.80f;
        p.release = 0.35f;
        adsr.setParameters (p);
    }

    // ── Parameter setters (audio thread, called each block) ──────────────────

    void setOscParameters (int idx, bool enabled, int voices,
                           float detuneCents, float level)
    {
        if (idx < 0 || idx >= numOscs) return;
        oscEnabled[idx] = enabled;
        oscs[idx].setNumVoices   (voices);
        oscs[idx].setDetuneCents (detuneCents);
        oscs[idx].setGain        (level);
    }

    void setRetrigger (bool r) { retrigger = r; }

    void setSampleLayers (std::array<SampleLayer*, 3> layers)
    {
        sampleLayers = layers;
    }

    void setLayerParameters (int idx, bool enabled, float gainValue)
    {
        if (idx < 0 || idx >= 3) return;
        layerEnabled[idx] = enabled;
        layerGain[idx]    = gainValue;
    }

    // ── SynthesiserVoice ─────────────────────────────────────────────────────

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<juce::SynthesiserSound*> (s) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int) override
    {
        currentNote     = midiNoteNumber;
        currentVelocity = velocity;

        const float hz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        for (int i = 0; i < numOscs; ++i)
        {
            oscs[i].setFrequency (hz);
            if (retrigger)
                oscs[i].resetPhases();
        }

        adsr.noteOn();

        // ── Sample layer initialisation ──────────────────────────────────────
        const double sr = getSampleRate();

        for (int l = 0; l < 3; ++l)
        {
            samplePos[l]       = 0.0;
            sampleSpeed[l]     = 1.0;
            layerBestNote[l]   = -1;

            if (sampleLayers[l] == nullptr) continue;

            // Find nearest loaded note within ±4 octaves
            for (int offset = 0; offset < 48; ++offset)
            {
                auto tryNote = [&] (int note) -> bool
                {
                    if (note < 0 || note > 127) return false;
                    if (! sampleLayers[l]->hasNote (note)) return false;

                    layerBestNote[l] = note;

                    // Speed = pitch ratio × sample-rate ratio
                    const double semitones = currentNote - note;
                    const double pitchRatio = std::pow (2.0, semitones / 12.0);
                    const double srcRate    = sampleLayers[l]->getSampleRate (note);
                    const double srRatio    = (srcRate > 0.0 && sr > 0.0)
                                             ? srcRate / sr : 1.0;
                    sampleSpeed[l] = pitchRatio * srRatio;
                    return true;
                };

                if (tryNote (currentNote - offset)) break;
                if (offset > 0 && tryNote (currentNote + offset)) break;
            }
        }
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
            adsr.noteOff();
        else
        {
            clearCurrentNote();
            adsr.reset();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& output,
                          int startSample, int numSamples) override
    {
        if (! adsr.isActive())
            return;

        temp.setSize (1, numSamples, false, false, true);

        for (int i = 0; i < numSamples; ++i)
        {
            float s = 0.0f;

            // Oscillators
            for (int o = 0; o < numOscs; ++o)
                if (oscEnabled[o])
                    s += oscs[o].processSample();

            s *= currentVelocity;

            // Sample layers — fractional position → smooth interpolation
            for (int l = 0; l < 3; ++l)
            {
                if (! layerEnabled[l] || sampleLayers[l] == nullptr
                    || layerBestNote[l] < 0)
                    continue;

                const int    note = layerBestNote[l];
                const int    len  = sampleLayers[l]->getLengthInSamples (note);
                const double pos  = samplePos[l];

                if (pos < (double) len)
                {
                    s += sampleLayers[l]->getSampleAt (note, pos)
                         * layerGain[l] * currentVelocity;
                }

                samplePos[l] += sampleSpeed[l];
            }

            temp.setSample (0, i, s);
        }

        adsr.applyEnvelopeToBuffer (temp, 0, numSamples);

        for (int ch = 0; ch < output.getNumChannels(); ++ch)
            output.addFrom (ch, startSample, temp, 0, 0, numSamples);
    }

private:
    static constexpr int numOscs = 3;

    std::array<SupersawOsc, numOscs> oscs;
    std::array<bool, numOscs>        oscEnabled { true, true, false };

    juce::ADSR               adsr;
    juce::AudioBuffer<float> temp;

    float currentVelocity { 0.0f };
    int   currentNote     { 69 };
    bool  retrigger       { false };

    std::array<SampleLayer*, 3> sampleLayers   { nullptr, nullptr, nullptr };
    std::array<bool,   3>       layerEnabled   { false, false, false };
    std::array<float,  3>       layerGain      { 0.8f, 0.7f, 0.6f };
    std::array<double, 3>       samplePos      { 0.0, 0.0, 0.0 };
    std::array<double, 3>       sampleSpeed    { 1.0, 1.0, 1.0 }; // pitch × sr ratio
    std::array<int,    3>       layerBestNote  { -1, -1, -1 };
};
