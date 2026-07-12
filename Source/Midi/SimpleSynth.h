#pragma once

#include <JuceHeader.h>

enum class SimpleSynthInstrument
{
    Lead,
    Bass,
    Guitar,
    Drum
};

class SimpleSynthSound final : public juce::SynthesiserSound
{
public:
    SimpleSynthSound(int midiChannel, SimpleSynthInstrument instrumentType);

    bool appliesToNote(int) override;
    bool appliesToChannel(int) override;
    SimpleSynthInstrument getInstrument() const;

private:
    int channel = 1;
    SimpleSynthInstrument instrument = SimpleSynthInstrument::Lead;
};

class SimpleSynthVoice final : public juce::SynthesiserVoice
{
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int) override;
    void controllerMoved(int, int) override;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParameters { 0.01f, 0.1f, 0.8f, 0.2f };
    SimpleSynthInstrument instrument = SimpleSynthInstrument::Lead;
    double currentAngle = 0.0;
    double angleDelta = 0.0;
    float level = 0.0f;
    int currentMidiNote = 0;
    uint32_t noiseState = 0x12345678;
};

class SimpleSynth final
{
public:
    SimpleSynth();

    void setCurrentPlaybackSampleRate(double sampleRate);
    void renderNextBlock(juce::AudioBuffer<float>& buffer,
                         juce::MidiBuffer& midiMessages,
                         int startSample,
                         int numSamples);
    void allNotesOff();

private:
    juce::Synthesiser synth;
};
