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
    double reps, double s, double R, double endScale)
{
    int ppq = model.getPPQ() > 0 ? model.getPPQ() : 960;

    return GeoTimeMath::solve(
        mode, reps, s, R, endScale,
        model.getDeltas(), model.getTotalDuration(),
        model.getBPM(), ppq
    );
}

juce::Result MidiTransformEngine::generateOutput(int totalSteps, double s)
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
    // Temporary storage for events before adding to MidiFile
    using TimeEvent = std::pair<double, juce::MidiMessage>;
    std::vector<std::vector<TimeEvent>> trackStreams(numTracks);

    // 1. Inject Metadata on Track 0 (Tempo, Sig)
    if (numTracks > 0) {
        int uspq = (int)(60000000.0 / model.getBPM());
        trackStreams[0].push_back({ 0.0, juce::MidiMessage::tempoMetaEvent(uspq) });
        trackStreams[0].push_back({ 0.0, juce::MidiMessage::timeSignatureMetaEvent(4, 4) });
    }

    // 2. Initial Events (Time 0)
    // These are events that happened exactly at the start of the file
    if (!segments.empty()) {
        for (const auto& ev : segments[0]) {
            if (ev.sourceTrackIndex >= 0 && ev.sourceTrackIndex < numTracks)
                trackStreams[ev.sourceTrackIndex].push_back({ ev.message.getTimeStamp(), ev.message });
        }
    }

    // 3. Iterative Generation
    double currentAbsTime = 0.0;

    for (int k = 0; k < totalSteps; ++k) {
        int segIdx = k % segmentCount;

        // Calculate the stretched duration for this specific segment step
        double stretchedDelta = deltas[segIdx] * std::pow(s, k);
        currentAbsTime += stretchedDelta;

        // Events associated with the end of this segment are in the next bucket
        int nextBucketIdx = segIdx + 1;

        // Helper to inject events with time scaling
        auto addScaledEvents = [&](int bucketIdx, double baseTime) {
            if (bucketIdx >= (int)segments.size()) return;
            for (const auto& ev : segments[bucketIdx]) {
                // Apply the geometric scale to the groove offset as well
                double grooveOffset = ev.message.getTimeStamp();
                double scaledOffset = grooveOffset * std::pow(s, k);

                if (ev.sourceTrackIndex >= 0 && ev.sourceTrackIndex < numTracks) {
                    trackStreams[ev.sourceTrackIndex].push_back({ baseTime + scaledOffset, ev.message });
                }
            }
            };

        // Handle wrapping around the segments
        if (nextBucketIdx == segmentCount) {
            // End of the pattern in source -> Map to end of pattern in dest
            if (segmentCount < (int)segments.size())
                addScaledEvents(segmentCount, currentAbsTime);

            // Also grab events from the start of the pattern (wrap) if we aren't done
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
        stream.push_back({ currentAbsTime, juce::MidiMessage::endOfTrack() });

        // Sort by time. Stable sort preserves order of simultaneous events.
        std::stable_sort(stream.begin(), stream.end(),
            [](const TimeEvent& a, const TimeEvent& b) {
                // Floating point tolerance for "simultaneous"
                if (std::abs(a.first - b.first) > 1e-6)
                    return a.first < b.first;

                // Tie-breaker: Priority order (Meta > NoteOff > NoteOn > CC)
                auto getPriority = [](const juce::MidiMessage& m) {
                    if (m.isMetaEvent() && m.getMetaEventType() == 0x2f) return 4;
                    if (m.isMetaEvent()) return 0;
                    if (m.isNoteOff()) return 1;
                    if (m.isNoteOn()) return 2;
                    return 3;
                    };
                return getPriority(a.second) < getPriority(b.second);
            });

        // Convert to ticks and add to MidiFile
        juce::MidiMessageSequence seq;
        for (const auto& ev : stream) {
            juce::MidiMessage m = ev.second;
            m.setTimeStamp((double)std::llround(ev.first));
            seq.addEvent(m);
        }
        seq.updateMatchedPairs(); // Links NoteOns to NoteOffs
        generatedMidi.addTrack(seq);
    }

    isGenerated = true;
    return juce::Result::ok();
}

juce::Result MidiTransformEngine::saveFile(const juce::File& dest) {
    if (!isGenerated) return juce::Result::fail("Nothing to save.");

    // Windows file locking safety
    if (dest.existsAsFile() && !dest.deleteFile())
        return juce::Result::fail("Could not overwrite existing file (locked?).");

    juce::FileOutputStream stream(dest);
    if (!stream.openedOk()) return juce::Result::fail("Could not write to disk.");

    // Force Format 1 if multi-track
    int format = (generatedMidi.getNumTracks() > 1) ? 1 : 0;
    return generatedMidi.writeTo(stream, format)
        ? juce::Result::ok()
        : juce::Result::fail("Internal write error.");
}

juce::String MidiTransformEngine::getDebugDump() {
    juce::String s = "--- DEBUG DUMP ---\n\n";
    if (model.isLoaded()) s << midiToString(model.getSourceMidi(), "SOURCE");
    s << "\n";
    if (isGenerated) s << midiToString(generatedMidi, "OUTPUT");
    return s;
}

juce::String MidiTransformEngine::midiToString(const juce::MidiFile& file, const juce::String& title) {
    juce::String s;
    s << "--- " << title << " ---\n";
    s << "Format: " << file.getTimeFormat() << " ticks/q\n";
    for (int i = 0; i < file.getNumTracks(); ++i) {
        s << "Track " << i << ": " << file.getTrack(i)->getNumEvents() << " events\n";
    }
    return s;
}