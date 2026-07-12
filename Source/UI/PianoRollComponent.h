#pragma once

#include "../AudioEngine.h"

#include <JuceHeader.h>

#include <functional>
#include <optional>
#include <vector>

class PianoRollComponent final : public juce::Component
{
public:
    PianoRollComponent(AudioEngine& engineToUse, TrackId trackIdToUse, juce::Uuid clipIdToUse);

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    std::function<void()> onEdited;

private:
    struct NoteItem
    {
        int note = 60;
        double startBeat = 0.0;
        double endBeat = 0.25;
        float velocity = 0.8f;
    };

    std::vector<NoteItem> getNotes() const;
    bool commitNotes(std::vector<NoteItem> notes);
    std::optional<int> findNoteAt(juce::Point<float> position) const;
    juce::Rectangle<float> getRollBounds() const;
    juce::Rectangle<float> getNoteBounds(const NoteItem& note) const;
    double getClipLengthBeats() const;
    int getTrackMidiChannel() const;
    float beatToX(double beat) const;
    double xToBeat(float x) const;
    float noteToY(int note) const;
    int yToNote(float y) const;
    double snapBeat(double beat) const;
    void updateSelectedVelocity(float velocity);

    AudioEngine& audioEngine;
    TrackId trackId;
    juce::Uuid clipId;
    juce::Slider velocitySlider;
    juce::Label velocityLabel;
    std::optional<int> selectedNoteIndex;
    bool draggingNote = false;
    bool resizingNote = false;
    double dragBeatOffset = 0.0;
    int dragNoteOffset = 0;
    double resizeOriginalStart = 0.0;
    static constexpr int minNote = 36;
    static constexpr int maxNote = 84;
    static constexpr float toolbarHeight = 42.0f;
    static constexpr float keyboardWidth = 54.0f;
    static constexpr double gridBeats = 0.25;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollComponent)
};
