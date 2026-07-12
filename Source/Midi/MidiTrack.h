#pragma once

#include "../Core/TrackTypes.h"
#include "MidiClip.h"

#include <JuceHeader.h>

#include <vector>

enum class MidiInstrument
{
    Lead,
    Bass,
    Guitar,
    Drum
};

inline juce::String midiInstrumentToString(MidiInstrument instrument)
{
    switch (instrument)
    {
        case MidiInstrument::Bass:
            return "Bass";
        case MidiInstrument::Guitar:
            return "Guitar";
        case MidiInstrument::Drum:
            return "Drum";
        case MidiInstrument::Lead:
        default:
            return "Lead";
    }
}

inline MidiInstrument midiInstrumentFromString(const juce::String& text)
{
    const auto normalized = text.trim().toLowerCase();

    if (normalized == "bass")
        return MidiInstrument::Bass;
    if (normalized == "guitar")
        return MidiInstrument::Guitar;
    if (normalized == "drum" || normalized == "drums")
        return MidiInstrument::Drum;

    return MidiInstrument::Lead;
}

inline int midiInstrumentToChannel(MidiInstrument instrument)
{
    switch (instrument)
    {
        case MidiInstrument::Bass:
            return 2;
        case MidiInstrument::Guitar:
            return 3;
        case MidiInstrument::Drum:
            return 10;
        case MidiInstrument::Lead:
        default:
            return 1;
    }
}

class MidiTrack final
{
public:
    explicit MidiTrack(juce::String trackName);

    TrackState state;
    MidiInstrument instrument = MidiInstrument::Lead;
    std::vector<MidiClip> clips;
};
