/*
  ==============================================================================
    MidiTransformEngine.h

    Orchestrator class.
    Connects the data model (MidiGridModel) with the math solver (GeoTimeMath)
    and handles the generation of the output MIDI file.
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>
#include "MidiGridModel.h"
#include "GeometricTimeSolver.h"

class MidiTransformEngine
{
public:
    MidiTransformEngine() = default;
    ~MidiTransformEngine() = default;

    juce::Result loadSource(const juce::File& file);

    // Run the solver to determine generation parameters
    GeoTimeMath::CalculationResult runSolver(GeoTimeMath::Mode mode,
        double reps, double s, double R, double endScale, bool constrainToIntegerReps);

    // Construct the new MIDI sequence based on solved parameters
    juce::Result generateOutput(int steps, double s);

    juce::Result saveFile(const juce::File& destination);

    // Diagnostics
    juce::String getDebugDump();

    // State inspectors
    bool isSourceLoaded() const { return model.isLoaded(); }
    int getSourceTrackCount() const { return model.getNumTracks(); }
    int getSegmentCount() const { return (int)model.getDeltas().size(); }
    double getSourceBPM() const { return model.getBPM(); }

private:
    MidiGridModel model;
    juce::MidiFile generatedMidi;
    bool isGenerated = false;

    juce::String midiToString(const juce::MidiFile& file, const juce::String& title);
};