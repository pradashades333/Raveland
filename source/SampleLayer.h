#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>

/** Per-key sample data for one layer.
 *
 *  Loading is flexible: any audio file in the folder is accepted.
 *  The MIDI root note is detected from the filename using common patterns
 *  (e.g. "Piano_C4.wav", "A#3.wav", "Kick_60.wav", "069.wav").
 *  If no note can be detected the file is mapped to middle C (60) so
 *  something always plays when a folder is loaded.
 */
class SampleLayer
{
public:
    SampleLayer() = default;

    bool loadFromFolder (const juce::File& folder, double /*targetSampleRate*/)
    {
        if (! folder.isDirectory())
            return false;

        juce::AudioFormatManager manager;
        manager.registerBasicFormats();

        // Clear previous content
        for (auto& b : samples)    b.setSize (0, 0);
        for (auto& r : sampleRates) r = 0.0;

        // Collect all audio files
        juce::Array<juce::File> files;
        folder.findChildFiles (files, juce::File::findFiles, false,
                               "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");

        if (files.isEmpty())
            return false;

        bool anyLoaded = false;
        bool anyNoteDetected = false;

        for (const auto& f : files)
        {
            std::unique_ptr<juce::AudioFormatReader> reader (manager.createReaderFor (f));
            if (reader == nullptr) continue;

            const int note = detectNoteFromFilename (f.getFileNameWithoutExtension());

            if (note >= 0)
                anyNoteDetected = true;

            const int destNote = (note >= 0) ? note : 60; // default: middle C

            juce::AudioBuffer<float> buf (
                static_cast<int> (reader->numChannels),
                static_cast<int> (reader->lengthInSamples));

            reader->read (buf.getArrayOfWritePointers(),
                          buf.getNumChannels(), 0, buf.getNumSamples());

            // Mix down to mono if stereo (keeps memory simpler for a sample layer)
            if (buf.getNumChannels() > 1)
            {
                juce::AudioBuffer<float> mono (1, buf.getNumSamples());
                mono.copyFrom (0, 0, buf, 0, 0, buf.getNumSamples());
                mono.addFrom  (0, 0, buf, 1, 0, buf.getNumSamples());
                mono.applyGain (0.5f);
                samples[destNote]    = std::move (mono);
            }
            else
            {
                samples[destNote] = std::move (buf);
            }

            sampleRates[destNote] = reader->sampleRate;
            anyLoaded = true;
        }

        (void) anyNoteDetected; // reserved for future use
        return anyLoaded;
    }

    // ── Playback ──────────────────────────────────────────────────────────────

    /** Linear interpolation at a fractional source position.
     *  @param pos   position in source samples (fractional OK). */
    float getSampleAt (int note, double pos) const noexcept
    {
        if (note < 0 || note >= 128) return 0.0f;
        const auto& buf = samples[note];
        if (buf.getNumSamples() == 0) return 0.0f;

        const int   i0   = static_cast<int> (pos);
        const int   i1   = i0 + 1;
        const float frac = static_cast<float> (pos - i0);

        if (i0 >= buf.getNumSamples()) return 0.0f;

        const float s0 = buf.getSample (0, i0);
        const float s1 = (i1 < buf.getNumSamples()) ? buf.getSample (0, i1) : 0.0f;
        return s0 + frac * (s1 - s0);
    }

    double getSampleRate (int note) const noexcept
    {
        if (note < 0 || note >= 128) return 0.0;
        return sampleRates[note];
    }

    bool hasNote (int note) const noexcept
    {
        if (note < 0 || note >= 128) return false;
        return samples[note].getNumSamples() > 0;
    }

    int getLengthInSamples (int note) const noexcept
    {
        if (note < 0 || note >= 128) return 0;
        return samples[note].getNumSamples();
    }

private:
    // ── Filename → MIDI note detection ───────────────────────────────────────

    /** Returns MIDI note 0-127 parsed from the filename, or -1 if not found.
     *
     *  Handles (case-insensitive):
     *    Pure numeric:      "60"  "069"  "C60"
     *    Note + octave:     "C4"  "A#3"  "Bb2"  "C-1"  "Ab-1"
     *    Note in longer name: "Piano_C4_mf"  "Kick_A#2"
     */
    static int detectNoteFromFilename (const juce::String& rawName)
    {
        const juce::String name = rawName.trim();

        // 1. Pure integer (e.g. "60" or "069")
        if (name.containsOnly ("0123456789"))
        {
            const int n = name.getIntValue();
            if (n >= 0 && n < 128) return n;
        }

        // 2. Note name scan — search all positions in the (uppercase) string.
        //    Two-char names (C#, Db …) are checked before one-char names to
        //    avoid partial matches.
        struct NoteEntry { const char* str; int semitone; };
        static const NoteEntry notes[] = {
            { "C#", 1 }, { "DB", 1 },
            { "D#", 3 }, { "EB", 3 },
            { "F#", 6 }, { "GB", 6 },
            { "G#", 8 }, { "AB", 8 },
            { "A#",10 }, { "BB",10 },
            { "C",  0 }, { "D",  2 }, { "E",  4 },
            { "F",  5 }, { "G",  7 }, { "A",  9 }, { "B", 11 }
        };

        const juce::String upper = name.toUpperCase();

        for (const auto& ne : notes)
        {
            int idx = 0;
            while ((idx = upper.indexOf (idx, ne.str)) >= 0)
            {
                int pos = idx + (int) strlen (ne.str);

                // Optional separator
                if (pos < upper.length() &&
                    (upper[pos] == '-' || upper[pos] == '_' || upper[pos] == ' '))
                    ++pos;

                if (pos >= upper.length()) { ++idx; continue; }

                // Optional minus for negative octave
                int sign = 1;
                if (upper[pos] == '-') { sign = -1; ++pos; }

                if (pos < upper.length() && juce::CharacterFunctions::isDigit (upper[pos]))
                {
                    const int octave = sign * (int) (upper[pos] - '0');
                    const int midi   = (octave + 1) * 12 + ne.semitone;
                    if (midi >= 0 && midi < 128)
                        return midi;
                }
                ++idx;
            }
        }

        return -1; // not detected
    }

    std::array<juce::AudioBuffer<float>, 128> samples;
    std::array<double, 128> sampleRates {};
};
