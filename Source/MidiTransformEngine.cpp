/*
  ==============================================================================
    MidiTransformEngine.cpp
  ==============================================================================
*/
#include "MidiTransformEngine.h"

juce::Result MidiTransformEngine::loadSource(const juce::File& file) {
    isGenerated = false;
    return model.load(file);
}

GeoTimeMath::CalculationResult MidiTransformEngine::runSolver(GeoTimeMath::Mode mode,
    double reps, double beatRatio, double totalScale, double beatEnd, bool constrainToIntegerReps)
{
    int ppq = model.getPPQ() > 0 ? model.getPPQ() : 960;

    return GeoTimeMath::solve(
        mode, reps, beatRatio, totalScale, beatEnd,
        model.getDeltas(), model.getTotalDuration(),
        model.getBPM(), ppq, constrainToIntegerReps
    );
}

juce::Result MidiTransformEngine::generateOutput(int totalSteps, double s_step)
{
    if (!model.isLoaded()) return juce::Result::fail("No source MIDI loaded.");

    generatedMidi.clear();
    int ppq = model.getPPQ() > 0 ? model.getPPQ() : 960;
    generatedMidi.setTicksPerQuarterNote(ppq);

    const auto& segments = model.getSegments();
    const auto& deltas = model.getDeltas();
    int segmentCount = (int)deltas.size();

    if (segmentCount == 0) return juce::Result::fail("Model is empty (no time segments).");

    int numTracks = model.getNumTracks();
    using TimeEvent = std::pair<double, juce::MidiMessage>;

    // Intermediate storage: Vector of events per track
    std::vector<std::vector<TimeEvent>> trackStreams(numTracks);

    // 1. Initialize Track 0 with Metadata (Tempo, Time Sig)
    if (numTracks > 0) {
        int uspq = (int)(60000000.0 / model.getBPM());
        trackStreams[0].push_back({ 0.0, juce::MidiMessage::tempoMetaEvent(uspq) });
        trackStreams[0].push_back({ 0.0, juce::MidiMessage::timeSignatureMetaEvent(4, 4) });
    }

    // 2. Add events from the very start (Time 0)
    if (!segments.empty()) {
        for (const auto& ev : segments[0]) {
            if (ev.sourceTrackIndex >= 0 && ev.sourceTrackIndex < numTracks)
                trackStreams[ev.sourceTrackIndex].push_back({ ev.message.getTimeStamp(), ev.message });
        }
    }

    // 3. Generate Sequence
    double currentAbsTime = 0.0;

    for (int k = 0; k < totalSteps; ++k) {
        int segIdx = k % segmentCount;

        // Stretch the duration of this specific segment
        double stretchedDelta = deltas[segIdx] * std::pow(s_step, k);
        currentAbsTime += stretchedDelta;

        int nextBucketIdx = segIdx + 1;

        // Lambda to inject events with geometric time scaling
        auto addScaledEvents = [&](int bucketIdx, double baseTime) {
            if (bucketIdx >= (int)segments.size()) return;
            for (const auto& ev : segments[bucketIdx]) {
                double grooveOffset = ev.message.getTimeStamp();
                // Apply scale to the groove offset too so it stays proportional
                double scaledOffset = grooveOffset * std::pow(s_step, k);

                if (ev.sourceTrackIndex >= 0 && ev.sourceTrackIndex < numTracks) {
                    trackStreams[ev.sourceTrackIndex].push_back({ baseTime + scaledOffset, ev.message });
                }
            }
            };

        // Handle loop wrap-around logic
        if (nextBucketIdx == segmentCount) {
            // End of source pattern -> Map to end of dest pattern
            if (segmentCount < (int)segments.size())
                addScaledEvents(segmentCount, currentAbsTime);

            // Start of next source pattern -> Map to start of next dest pattern
            if (k < totalSteps - 1)
                addScaledEvents(0, currentAbsTime);
        }
        else {
            addScaledEvents(nextBucketIdx, currentAbsTime);
        }
    }

    // 4. Finalize Tracks
    for (int t = 0; t < numTracks; ++t) {
        auto& stream = trackStreams[t];

        // Determine track end time
        double lastEventTime = currentAbsTime;
        for (const auto& ev : stream) {
            if (ev.first > lastEventTime) lastEventTime = ev.first;
        }
        stream.push_back({ lastEventTime, juce::MidiMessage::endOfTrack() });

        // Stable sort ensures events at same tick (e.g. NoteOn + NoteOff) stay in added order
        // Secondary sort by message type ensures Meta events processed first
        std::stable_sort(stream.begin(), stream.end(),
            [](const TimeEvent& a, const TimeEvent& b) {
                if (std::abs(a.first - b.first) > 1e-6) return a.first < b.first;
                return (a.second.isMetaEvent() ? 0 : 1) < (b.second.isMetaEvent() ? 0 : 1);
            });

        // Convert double timestamps to integer ticks
        juce::MidiMessageSequence seq;
        for (const auto& ev : stream) {
            juce::MidiMessage m = ev.second;
            m.setTimeStamp((double)std::llround(ev.first));
            seq.addEvent(m);
        }
        seq.updateMatchedPairs();
        generatedMidi.addTrack(seq);
    }

    isGenerated = true;
    return juce::Result::ok();
}

juce::Result MidiTransformEngine::saveFile(const juce::File& dest) {
    if (!isGenerated) return juce::Result::fail("Nothing to save.");

    if (dest.existsAsFile() && !dest.deleteFile())
        return juce::Result::fail("File locked.");

    juce::FileOutputStream stream(dest);
    if (!stream.openedOk()) return juce::Result::fail("Write error.");

    // Force Type 1 for multi-track compatibility
    int format = (generatedMidi.getNumTracks() > 1) ? 1 : 0;

    return generatedMidi.writeTo(stream, format)
        ? juce::Result::ok()
        : juce::Result::fail("Write error.");
}

juce::String MidiTransformEngine::getDebugDump() {
    juce::String s = "--- DEBUG ---\n";
    if (model.isLoaded()) s << midiToString(model.getSourceMidi(), "SOURCE");
    if (isGenerated) s << midiToString(generatedMidi, "OUTPUT");
    return s;
}

juce::String MidiTransformEngine::midiToString(const juce::MidiFile& file, const juce::String& title) {
    juce::String s; s << "\n[" << title << "]\n";
    for (int i = 0; i < file.getNumTracks(); ++i)
        s << "Trk" << i << ": " << file.getTrack(i)->getNumEvents() << " evs\n";
    return s;
}