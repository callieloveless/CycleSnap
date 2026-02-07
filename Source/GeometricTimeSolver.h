/*
  ==============================================================================
    GeometricTimeSolver.h

    Core math for the geometric time stretching.
    Implements the geometric series summation and root finding for:
    TotalDuration = Sum( delta[i] * s^k )
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
        int repetitions = 0;        // N
        double scaleFactor = 1.0;   // s
        double timeRatio = 0.0;     // R (Total Duration / Source Duration)
        double terminalScale = 1.0; // s^(N-1)

        // Quantization error stats
        double realizedRatio = 0.0;
        double errorTicks = 0.0;
        double errorMs = 0.0;

        std::string message;
    };

    enum class Mode {
        TargetDuration, // Fixed N, solve for s given R
        FixedAccel,     // Fixed N, solve for R given s
        MatchEndScale,  // Fixed N, solve for s/R given final step scale
        FitToCurve      // Fixed s/R, solve for integer N
    };

    /**
     * Solves the geometric series parameters based on the selected mode.
     * @param deltas: The source time intervals (segments)
     * @param sourceDur: Total duration of the source segments
     */
    CalculationResult solve(Mode mode, double targetReps, double inputS, double targetR, double inputEndScale,
        const std::vector<double>& deltas, double sourceDur, double bpm, int ppq);
}