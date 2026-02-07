/*
  ==============================================================================
    GeometricTimeSolver.h

    Core math for the geometric time stretching algorithms.
    Stateless solver that handles the geometric series summation and
    parameter estimation (bisection search) for the time-warping.
  ==============================================================================
*/
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <string>

namespace GeoTimeMath
{
    struct CalculationResult {
        bool success = false;
        std::string message;

        // --- Output Parameters ---
        double beatRatio = 1.0;     // (s) Scale factor per 1.0 repetition 
        double totalScale = 0.0;    // (R) Total Output Duration / Source Duration
        double beatEnd = 1.0;       // (E) Scale factor of the final segment relative to start

        // --- Internal Engine State ---
        int repetitions = 0;        // (N) Total integer steps to generate
        double stepScale = 1.0;     // (s_step) Scale factor per single event step

        // --- Validation Stats ---
        double realizedScale = 0.0; // Actual R achieved after integer rounding
        double errorTicks = 0.0;    // Quantization error in MIDI ticks
        double errorMs = 0.0;       // Quantization error in milliseconds
    };

    enum class Mode {
        TargetTotalScale, // Fixed N, R -> Solve s
        FixedBeatRatio,   // Fixed N, s -> Solve R
        MatchBeatEnd,     // Fixed N, E -> Solve s, R
        FitToCurve,       // Fixed s, R -> Solve N
        FitEndAndRatio    // Fixed E, R -> Solve N
    };

    /**
     * Solves the geometric series parameters.
     * * @param constrainToIntegerReps: Forces N to be a multiple of the segment count
     * (ensures we always end on a full loop boundary).
     */
    CalculationResult solve(Mode mode, double targetReps, double inputBeatRatio, double targetTotalScale, double inputBeatEnd,
        const std::vector<double>& deltas, double sourceDur, double bpm, int ppq, bool constrainToIntegerReps);
}