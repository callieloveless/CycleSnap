/*
  ==============================================================================
    MidiGridModel.h

    Loads a MIDI file and segments it into time-slices (grid) for processing.
    Preserves "groove" by storing event offsets relative to the grid points.
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>
#include <vector>

struct QueuedEvent {
    int sourceTrackIndex;
    juce::MidiMessage message; // Relative timestamp (offset from grid start)
};

class MidiGridModel
{
public:
    MidiGridModel() = default;

    void clear();
    juce::Result load(const juce::File& file);

    // Getters
    bool isLoaded() const { return hasLoaded; }
    int getNumTracks() const { return sourceMidi.getNumTracks(); }
    int getPPQ() const { return sourceMidi.getTimeFormat(); }
    double getBPM() const { return initialBpm; }
    double getTotalDuration() const { return totalDurationTicks; }

    const std::vector<double>& getDeltas() const { return segmentDeltas; }
    const std::vector<std::vector<QueuedEvent>>& getSegments() const { return eventSegments; }

    // For debugging/dumping
    const juce::MidiFile& getSourceMidi() const { return sourceMidi; }

private:
    void analyzeTimeline();
    void segmentEvents();

    bool hasLoaded = false;
    juce::MidiFile sourceMidi;
    int midiFormat = 1;
    double initialBpm = 120.0;
    double totalDurationTicks = 0.0;

    // The calculated grid
    std::vector<double> timePoints;     // Absolute timestamps of grid lines
    std::vector<double> segmentDeltas;  // Duration of each segment

    // Events bucketed by segment index
    std::vector<std::vector<QueuedEvent>> eventSegments;
};