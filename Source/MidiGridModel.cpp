/*
  ==============================================================================
    MidiGridModel.cpp
  ==============================================================================
*/
#include "MidiGridModel.h"

void MidiGridModel::clear()
{
    sourceMidi.clear();
    segmentDeltas.clear();
    timePoints.clear();
    eventSegments.clear();
    totalDurationTicks = 0.0;
    initialBpm = 120.0;
    hasLoaded = false;
    midiFormat = 1;
}

juce::Result MidiGridModel::load(const juce::File& file)
{
    clear();

    if (!file.existsAsFile())
        return juce::Result::fail("File not found: " + file.getFullPathName());

    juce::FileInputStream inputStream(file);
    if (!inputStream.openedOk())
        return juce::Result::fail("Could not open file stream.");

    if (!sourceMidi.readFrom(inputStream, true, &midiFormat))
        return juce::Result::fail("Corrupt or invalid MIDI file.");

    if (sourceMidi.getNumTracks() == 0)
        return juce::Result::fail("MIDI file contains no tracks.");

    hasLoaded = true;
    analyzeTimeline();
    segmentEvents();

    return juce::Result::ok();
}

void MidiGridModel::analyzeTimeline()
{
    // Flatten tracks to find all unique rhythmic points
    juce::MidiMessageSequence merged;
    for (int i = 0; i < sourceMidi.getNumTracks(); ++i)
        merged.addSequence(*sourceMidi.getTrack(i), 0.0);

    // Remove EndOfTrack meta events (0x2F) to prevent fake grid points at the end
    for (int i = merged.getNumEvents() - 1; i >= 0; --i) {
        auto m = merged.getEventPointer(i)->message;
        if (m.isMetaEvent() && m.getMetaEventType() == 0x2f)
            merged.deleteEvent(i, false);
    }

    // Sort is critical for delta calculation
    merged.sort();
    merged.updateMatchedPairs();

    // 1. Detect BPM 
    // Uses the first tempo change found. 
    // TODO: Support complex tempo maps if we add multi-tempo file support.
    initialBpm = 120.0;
    for (int i = 0; i < merged.getNumEvents(); ++i) {
        auto m = merged.getEventPointer(i)->message;
        if (m.isTempoMetaEvent()) {
            double spq = m.getTempoSecondsPerQuarterNote();
            if (spq > 0) initialBpm = 60.0 / spq;
            break;
        }
    }

    // 2. Build Grid Points
    timePoints.clear();
    timePoints.push_back(0.0);

    double lastT = 0.0;
    for (int i = 0; i < merged.getNumEvents(); ++i) {
        double t = merged.getEventPointer(i)->message.getTimeStamp();
        // Debounce micro-timing (< 0.001 ticks) to avoid zero-length segments
        if (t > lastT + 0.001) {
            timePoints.push_back(t);
            lastT = t;
        }
    }

    // 3. Calculate Deltas
    segmentDeltas.clear();
    totalDurationTicks = 0.0;

    if (timePoints.size() < 2) {
        // Fallback for empty/single-event files
        segmentDeltas.push_back(960.0);
        totalDurationTicks = 960.0;
    }
    else {
        for (size_t i = 0; i < timePoints.size() - 1; ++i) {
            double dt = timePoints[i + 1] - timePoints[i];
            segmentDeltas.push_back(dt);
            totalDurationTicks += dt;
        }
    }
}

void MidiGridModel::segmentEvents()
{
    eventSegments.clear();
    eventSegments.resize(timePoints.size() + 1);

    for (int t = 0; t < sourceMidi.getNumTracks(); ++t) {
        const auto* track = sourceMidi.getTrack(t);
        for (const auto* ev : *track) {
            const auto& msg = ev->message;

            // Skip EndOfTrack markers
            if (msg.isMetaEvent() && msg.getMetaEventType() == 0x2f) continue;

            double tStamp = msg.getTimeStamp();

            // Find nearest segment (Bucket)
            int bestIdx = -1;
            double minDiff = std::numeric_limits<double>::max();

            for (size_t i = 0; i < timePoints.size(); ++i) {
                double diff = std::abs(tStamp - timePoints[i]);
                if (diff < minDiff) {
                    minDiff = diff;
                    bestIdx = (int)i;
                }
            }

            // Catch events past the last grid point
            if (minDiff > 1.0 && tStamp >= totalDurationTicks) {
                bestIdx = (int)timePoints.size() - 1;
            }

            if (bestIdx >= 0) {
                juce::MidiMessage clone = msg;
                // Store relative offset (Groove) from the grid line
                double gridTime = timePoints[bestIdx];
                clone.setTimeStamp(tStamp - gridTime);

                eventSegments[bestIdx].push_back({ t, clone });
            }
        }
    }
}