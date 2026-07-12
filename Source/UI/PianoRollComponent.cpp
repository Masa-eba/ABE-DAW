#include "PianoRollComponent.h"

#include <algorithm>
#include <cmath>

PianoRollComponent::PianoRollComponent(AudioEngine& engineToUse,
                                       TrackId trackIdToUse,
                                       juce::Uuid clipIdToUse)
    : audioEngine(engineToUse),
      trackId(trackIdToUse),
      clipId(clipIdToUse)
{
    velocityLabel.setText("Velocity", juce::dontSendNotification);
    velocityLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(velocityLabel);

    velocitySlider.setRange(0.05, 1.0, 0.01);
    velocitySlider.setValue(0.8, juce::dontSendNotification);
    velocitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    velocitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
    velocitySlider.onValueChange = [this]
    {
        updateSelectedVelocity(static_cast<float>(velocitySlider.getValue()));
    };
    addAndMakeVisible(velocitySlider);

    setWantsKeyboardFocus(true);
}

void PianoRollComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff1e1f22));

    auto roll = getRollBounds();
    const auto clipLength = getClipLengthBeats();

    graphics.setColour(juce::Colour(0xff27292d));
    graphics.fillRect(roll);

    const auto rowHeight = roll.getHeight() / static_cast<float>(maxNote - minNote + 1);
    for (auto note = minNote; note <= maxNote; ++note)
    {
        const auto y = noteToY(note);
        const auto isBlackKey = juce::MidiMessage::isMidiNoteBlack(note);
        graphics.setColour(isBlackKey ? juce::Colour(0x22111111) : juce::Colour(0x11000000));
        graphics.fillRect(roll.getX(), y, roll.getWidth(), rowHeight);
        graphics.setColour(juce::Colour(0x334d535f));
        graphics.drawHorizontalLine(static_cast<int>(y), roll.getX(), roll.getRight());

        if ((note % 12) == 0)
        {
            graphics.setColour(juce::Colours::lightgrey);
            graphics.drawText(juce::MidiMessage::getMidiNoteName(note, true, true, 3),
                              4,
                              static_cast<int>(y),
                              static_cast<int>(keyboardWidth) - 8,
                              static_cast<int>(rowHeight),
                              juce::Justification::centredRight,
                              false);
        }
    }

    for (auto beat = 0.0; beat <= clipLength + 0.001; beat += gridBeats)
    {
        const auto x = beatToX(beat);
        const auto isBar = (static_cast<int>(std::round(beat * 4.0)) % 16) == 0;
        graphics.setColour(isBar ? juce::Colour(0xaa5d6676) : juce::Colour(0x335d6676));
        graphics.drawVerticalLine(static_cast<int>(x), roll.getY(), roll.getBottom());
    }

    auto notes = getNotes();
    for (auto index = 0; index < static_cast<int>(notes.size()); ++index)
    {
        const auto bounds = getNoteBounds(notes[static_cast<size_t>(index)]);
        const auto selected = selectedNoteIndex.has_value() && *selectedNoteIndex == index;
        graphics.setColour(selected ? juce::Colour(0xffffc857) : juce::Colour(0xff7dd3fc));
        graphics.fillRoundedRectangle(bounds, 3.0f);
        graphics.setColour(selected ? juce::Colours::black : juce::Colour(0xff0f172a));
        graphics.drawRoundedRectangle(bounds, 3.0f, selected ? 2.0f : 1.0f);
    }

    graphics.setColour(juce::Colours::white);
    graphics.drawText("Piano Roll",
                      12,
                      8,
                      160,
                      24,
                      juce::Justification::centredLeft,
                      false);
}

void PianoRollComponent::resized()
{
    auto area = getLocalBounds().removeFromTop(static_cast<int>(toolbarHeight));
    area.removeFromLeft(170);
    velocityLabel.setBounds(area.removeFromLeft(72).reduced(0, 8));
    velocitySlider.setBounds(area.removeFromLeft(220).reduced(0, 7));
}

void PianoRollComponent::mouseDown(const juce::MouseEvent& event)
{
    grabKeyboardFocus();

    if (! getRollBounds().contains(event.position))
        return;

    auto notes = getNotes();

    if (auto hit = findNoteAt(event.position))
    {
        selectedNoteIndex = *hit;

        if (event.mods.isPopupMenu())
        {
            notes.erase(notes.begin() + *hit);
            selectedNoteIndex.reset();
            commitNotes(std::move(notes));
            repaint();
            return;
        }

        const auto noteBounds = getNoteBounds(notes[static_cast<size_t>(*hit)]);
        resizingNote = event.position.x >= noteBounds.getRight() - 8.0f;
        draggingNote = ! resizingNote;
        dragBeatOffset = xToBeat(event.position.x) - notes[static_cast<size_t>(*hit)].startBeat;
        dragNoteOffset = yToNote(event.position.y) - notes[static_cast<size_t>(*hit)].note;
        resizeOriginalStart = notes[static_cast<size_t>(*hit)].startBeat;
        velocitySlider.setValue(notes[static_cast<size_t>(*hit)].velocity, juce::dontSendNotification);
        repaint();
        return;
    }

    const auto startBeat = snapBeat(xToBeat(event.position.x));
    const auto note = yToNote(event.position.y);
    const auto length = juce::jmin(gridBeats, juce::jmax(0.05, getClipLengthBeats() - startBeat));

    notes.push_back(NoteItem {
        note,
        startBeat,
        juce::jmin(getClipLengthBeats(), startBeat + length),
        static_cast<float>(velocitySlider.getValue())
    });

    std::sort(notes.begin(), notes.end(),
              [](const auto& left, const auto& right)
              {
                  if (std::abs(left.startBeat - right.startBeat) < 0.000001)
                      return left.note < right.note;
                  return left.startBeat < right.startBeat;
              });

    selectedNoteIndex = 0;
    for (auto index = 0; index < static_cast<int>(notes.size()); ++index)
        if (std::abs(notes[static_cast<size_t>(index)].startBeat - startBeat) < 0.000001
            && notes[static_cast<size_t>(index)].note == note)
            selectedNoteIndex = index;

    commitNotes(std::move(notes));
    repaint();
}

void PianoRollComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (! selectedNoteIndex.has_value())
        return;

    auto notes = getNotes();
    if (! juce::isPositiveAndBelow(*selectedNoteIndex, static_cast<int>(notes.size())))
        return;

    auto& note = notes[static_cast<size_t>(*selectedNoteIndex)];
    const auto clipLength = getClipLengthBeats();

    if (draggingNote)
    {
        const auto length = juce::jmax(0.05, note.endBeat - note.startBeat);
        note.startBeat = juce::jlimit(0.0, juce::jmax(0.0, clipLength - length), snapBeat(xToBeat(event.position.x) - dragBeatOffset));
        note.endBeat = note.startBeat + length;
        note.note = juce::jlimit(minNote, maxNote, yToNote(event.position.y) - dragNoteOffset);
    }
    else if (resizingNote)
    {
        note.endBeat = juce::jlimit(resizeOriginalStart + 0.05,
                                   clipLength,
                                   snapBeat(xToBeat(event.position.x)));
    }

    commitNotes(std::move(notes));
    repaint();
}

void PianoRollComponent::mouseUp(const juce::MouseEvent&)
{
    draggingNote = false;
    resizingNote = false;
}

std::vector<PianoRollComponent::NoteItem> PianoRollComponent::getNotes() const
{
    std::vector<NoteItem> notes;

    if (const auto* track = audioEngine.getProjectModel().findMidiTrack(trackId))
        for (const auto& clip : track->clips)
            if (clip.id == clipId)
            {
                auto sequence = clip.sequence;
                sequence.updateMatchedPairs();

                for (auto i = 0; i < sequence.getNumEvents(); ++i)
                {
                    const auto* event = sequence.getEventPointer(i);

                    if (event == nullptr || ! event->message.isNoteOn())
                        continue;

                    const auto* noteOff = event->noteOffObject;
                    notes.push_back(NoteItem {
                        event->message.getNoteNumber(),
                        juce::jlimit(0.0, clip.lengthBeats, event->message.getTimeStamp()),
                        noteOff != nullptr
                            ? juce::jlimit(0.0, clip.lengthBeats, noteOff->message.getTimeStamp())
                            : juce::jmin(clip.lengthBeats, event->message.getTimeStamp() + gridBeats),
                        event->message.getFloatVelocity()
                    });
                }

                break;
            }

    std::sort(notes.begin(), notes.end(),
              [](const auto& left, const auto& right)
              {
                  if (std::abs(left.startBeat - right.startBeat) < 0.000001)
                      return left.note < right.note;
                  return left.startBeat < right.startBeat;
              });
    return notes;
}

bool PianoRollComponent::commitNotes(std::vector<NoteItem> notes)
{
    juce::MidiMessageSequence sequence;
    const auto channel = getTrackMidiChannel();
    auto lengthBeats = getClipLengthBeats();

    for (auto& note : notes)
    {
        note.note = juce::jlimit(minNote, maxNote, note.note);
        note.startBeat = juce::jmax(0.0, note.startBeat);
        note.endBeat = juce::jmax(note.startBeat + 0.05, note.endBeat);
        lengthBeats = juce::jmax(lengthBeats, note.endBeat);

        auto noteOn = juce::MidiMessage::noteOn(channel,
                                                note.note,
                                                juce::jlimit(0.0f, 1.0f, note.velocity));
        noteOn.setTimeStamp(note.startBeat);
        auto noteOff = juce::MidiMessage::noteOff(channel, note.note);
        noteOff.setTimeStamp(note.endBeat);
        sequence.addEvent(noteOn);
        sequence.addEvent(noteOff);
    }

    sequence.sort();
    sequence.updateMatchedPairs();

    const auto changed = audioEngine.replaceMidiClipSequence(trackId, clipId, sequence, lengthBeats);
    if (changed && onEdited)
        onEdited();

    return changed;
}

std::optional<int> PianoRollComponent::findNoteAt(juce::Point<float> position) const
{
    const auto notes = getNotes();

    for (auto index = static_cast<int>(notes.size()) - 1; index >= 0; --index)
        if (getNoteBounds(notes[static_cast<size_t>(index)]).contains(position))
            return index;

    return std::nullopt;
}

juce::Rectangle<float> PianoRollComponent::getRollBounds() const
{
    return getLocalBounds().toFloat()
        .withTrimmedTop(toolbarHeight)
        .withTrimmedLeft(keyboardWidth)
        .reduced(8.0f, 8.0f);
}

juce::Rectangle<float> PianoRollComponent::getNoteBounds(const NoteItem& note) const
{
    const auto x = beatToX(note.startBeat);
    const auto width = juce::jmax(8.0f, beatToX(note.endBeat) - x);
    const auto rowHeight = getRollBounds().getHeight() / static_cast<float>(maxNote - minNote + 1);
    return { x, noteToY(note.note) + 1.0f, width, rowHeight - 2.0f };
}

double PianoRollComponent::getClipLengthBeats() const
{
    if (const auto* track = audioEngine.getProjectModel().findMidiTrack(trackId))
        for (const auto& clip : track->clips)
            if (clip.id == clipId)
                return juce::jmax(0.25, clip.lengthBeats);

    return 16.0;
}

int PianoRollComponent::getTrackMidiChannel() const
{
    if (const auto* track = audioEngine.getProjectModel().findMidiTrack(trackId))
        return midiInstrumentToChannel(track->instrument);

    return 1;
}

float PianoRollComponent::beatToX(double beat) const
{
    const auto roll = getRollBounds();
    return roll.getX() + static_cast<float>(beat / getClipLengthBeats()) * roll.getWidth();
}

double PianoRollComponent::xToBeat(float x) const
{
    const auto roll = getRollBounds();
    const auto ratio = juce::jlimit(0.0f, 1.0f, (x - roll.getX()) / roll.getWidth());
    return static_cast<double>(ratio) * getClipLengthBeats();
}

float PianoRollComponent::noteToY(int note) const
{
    const auto roll = getRollBounds();
    const auto rowHeight = roll.getHeight() / static_cast<float>(maxNote - minNote + 1);
    return roll.getY() + static_cast<float>(maxNote - juce::jlimit(minNote, maxNote, note)) * rowHeight;
}

int PianoRollComponent::yToNote(float y) const
{
    const auto roll = getRollBounds();
    const auto rowHeight = roll.getHeight() / static_cast<float>(maxNote - minNote + 1);
    const auto index = static_cast<int>((y - roll.getY()) / rowHeight);
    return juce::jlimit(minNote, maxNote, maxNote - index);
}

double PianoRollComponent::snapBeat(double beat) const
{
    return juce::jlimit(0.0, getClipLengthBeats(), std::round(beat / gridBeats) * gridBeats);
}

void PianoRollComponent::updateSelectedVelocity(float velocity)
{
    if (! selectedNoteIndex.has_value())
        return;

    auto notes = getNotes();
    if (! juce::isPositiveAndBelow(*selectedNoteIndex, static_cast<int>(notes.size())))
        return;

    notes[static_cast<size_t>(*selectedNoteIndex)].velocity = juce::jlimit(0.05f, 1.0f, velocity);
    commitNotes(std::move(notes));
    repaint();
}
