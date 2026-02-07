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
    // Merge all tracks to find unique time points (the "Grid")
    juce::MidiMessageSequence merged;
    for (int i = 0; i < sourceMidi.getNumTracks(); ++i)
        merged.addSequence(*sourceMidi.getTrack(i), 0.0);

    // Filter out EOT to avoid fake end points
    // Note: Manual check used for compatibility with older JUCE versions
    for (int i = merged.getNumEvents() - 1; i >= 0; --i) {
        auto m = merged.getEventPointer(i)->message;
        if (m.isMetaEvent() && m.getMetaEventType() == 0x2f)
            merged.deleteEvent(i, false);
    }

    // Sorting is critical for correct delta calculation
    merged.sort();
    merged.updateMatchedPairs();

    // 1. Detect BPM (Naively takes the first tempo change)
    // TODO: Support map-based tempo changes if needed later.
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
        // Ignore micro-timing issues (< 0.001 ticks)
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

            // Manual check for End of Track (Meta 0x2F)
            if (msg.isMetaEvent() && msg.getMetaEventType() == 0x2f) continue;

            double tStamp = msg.getTimeStamp();

            // Find which segment this event belongs to (Nearest neighbor match)
            int bestIdx = -1;
            double minDiff = std::numeric_limits<double>::max();

            for (size_t i = 0; i < timePoints.size(); ++i) {
                double diff = std::abs(tStamp - timePoints[i]);
                if (diff < minDiff) {
                    minDiff = diff;
                    bestIdx = (int)i;
                }
            }

            // If it's way past the end, dump in the last bucket
            if (minDiff > 1.0 && tStamp >= totalDurationTicks) {
                bestIdx = (int)timePoints.size() - 1;
            }

            if (bestIdx >= 0) {
                juce::MidiMessage clone = msg;
                // Calculate Groove Offset: how far is this note from the grid line?
                double gridTime = timePoints[bestIdx];
                clone.setTimeStamp(tStamp - gridTime);

                eventSegments[bestIdx].push_back({ t, clone });
            }
        }
    }
}