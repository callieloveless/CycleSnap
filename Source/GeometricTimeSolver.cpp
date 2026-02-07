/*
  ==============================================================================
    GeometricTimeSolver.cpp
  ==============================================================================
*/
#include "GeometricTimeSolver.h"

namespace GeoTimeMath
{
    static const int kMaxBisectionIters = 100;
    static const int kMaxSearchIters = 20000;
    static const double kEpsilon = 1e-7;

    // Helper: Sum of geometric series applied to a repeating vector of deltas
    // Duration = Sum( delta[k % M] * s^k )
    static double compute_geometric_duration(const std::vector<double>& deltas, int n_steps, double s, double source_dur)
    {
        if (deltas.empty() || source_dur <= kEpsilon) return 0.0;
        
        // If s is 1.0, it's just linear repetition
        if (std::abs(s - 1.0) < kEpsilon) {
            double total_reps = (double)n_steps / deltas.size();
            return total_reps; // Ratio is just N/M
        }

        double total = 0.0;
        size_t m_size = deltas.size();
        
        // Direct summation is safer than closed-form due to the modulo indexing of deltas
        for (int k = 0; k < n_steps; ++k) {
            total += deltas[k % m_size] * std::pow(s, k);
        }
        
        return total / source_dur;
    }

    static double solve_s_bisection(const std::vector<double>& deltas, int n, double target_r, double source_dur)
    {
        // Edge case: if target ratio equals count, s must be 1.0
        if (std::abs(target_r - (double)n) < 0.001) return 1.0;

        double low = 0.0001;
        double high = 2.0;

        // Exponentially expand search range if high is too low
        int safety = 0;
        while (compute_geometric_duration(deltas, n, high, source_dur) < target_r && safety++ < 20) {
            high *= 2.0;
        }

        // Standard bisection
        for (int i = 0; i < kMaxBisectionIters; ++i) {
            double mid = low + (high - low) * 0.5;
            double r_mid = compute_geometric_duration(deltas, n, mid, source_dur);
            
            if (std::abs(r_mid - target_r) < kEpsilon) return mid;
            
            if (r_mid < target_r) low = mid; 
            else high = mid;
        }
        
        return low + (high - low) * 0.5;
    }

    static int find_best_fit_n(const std::vector<double>& deltas, double s, double target_r, double source_dur)
    {
        int limit = std::max(1000, (int)deltas.size() * 100);
        double min_diff = std::numeric_limits<double>::max();
        int best_n = 1;

        // Brute force search for best integer N. 
        // Analytical solution exists but is messy with the modulo delta term.
        for (int k = 1; k <= limit; ++k) {
            double r = compute_geometric_duration(deltas, k, s, source_dur);
            double diff = std::abs(r - target_r);
            
            if (diff < min_diff) {
                min_diff = diff;
                best_n = k;
            }
            // If we've overshot significantly and diff is growing, stop
            if (r > target_r && diff > min_diff) break;
        }
        return best_n;
    }

    CalculationResult solve(Mode mode, double targetReps, double inputS, double targetR, double inputEndScale,
                            const std::vector<double>& deltas, double sourceDur, double bpm, int ppq)
    {
        CalculationResult res;
        
        // Fallback for empty data to prevent division by zero
        std::vector<double> work_deltas = deltas;
        double work_dur = sourceDur;
        if (work_deltas.empty()) { 
            work_deltas = { 960.0 }; 
            work_dur = 960.0; 
        }
        
        int m_seg_count = (int)work_deltas.size();
        if (m_seg_count < 1) m_seg_count = 1;

        // Validate inputs basic ranges
        if (targetReps <= 0 && mode != Mode::FitToCurve) {
            res.message = "Invalid Repetitions";
            return res;
        }

        // Logic branching
        switch (mode) {
        case Mode::TargetDuration: // Reps, R -> solve s
            if (targetR <= 0) { res.message = "Target Ratio must be > 0"; return res; }
            
            res.repetitions = (int)std::round(targetReps * m_seg_count);
            if (res.repetitions < 1) res.repetitions = 1;
            
            res.timeRatio = targetR;
            res.scaleFactor = solve_s_bisection(work_deltas, res.repetitions, targetR, work_dur);
            res.terminalScale = std::pow(res.scaleFactor, res.repetitions - 1);
            res.message = "Solved Scale (s)";
            break;

        case Mode::FixedAccel: // Reps, s -> calculate R
            if (inputS <= 0) { res.message = "Scale must be > 0"; return res; }
            
            res.repetitions = (int)std::round(targetReps * m_seg_count);
            if (res.repetitions < 1) res.repetitions = 1;
            
            res.scaleFactor = inputS;
            res.timeRatio = compute_geometric_duration(work_deltas, res.repetitions, res.scaleFactor, work_dur);
            res.terminalScale = std::pow(res.scaleFactor, res.repetitions - 1);
            res.message = "Calculated Ratio (R)";
            break;

        case Mode::MatchEndScale: // Reps, EndScale -> solve s, R
            if (inputEndScale <= 0) { res.message = "End Scale must be > 0"; return res; }
            
            res.repetitions = (int)std::round(targetReps * m_seg_count);
            if (res.repetitions < 1) res.repetitions = 1;
            
            // s = (end_scale)^(1/(N-1))
            res.scaleFactor = (res.repetitions > 1) ? std::pow(inputEndScale, 1.0 / (double)(res.repetitions - 1)) : 1.0;
            res.terminalScale = inputEndScale;
            res.timeRatio = compute_geometric_duration(work_deltas, res.repetitions, res.scaleFactor, work_dur);
            res.message = "Solved s from End Ratio";
            break;

        case Mode::FitToCurve: // s, R -> solve N
            if (inputS <= 0 || targetR <= 0) { res.message = "Invalid Input"; return res; }
            
            res.scaleFactor = inputS; 
            res.timeRatio = targetR;
            res.repetitions = find_best_fit_n(work_deltas, inputS, targetR, work_dur);
            res.terminalScale = std::pow(res.scaleFactor, res.repetitions - 1);
            res.message = "Solved Repetitions (N)";
            break;
        }

        res.success = true;

        // Post-calculation: Verify quantization error
        double quantized_ticks = 0.0;
        for (int k = 0; k < res.repetitions; ++k) {
            double exact_dt = work_deltas[k % m_seg_count] * std::pow(res.scaleFactor, k);
            quantized_ticks += (double)std::llround(exact_dt);
        }

        res.realizedRatio = quantized_ticks / work_dur;
        double ideal_ticks = work_dur * res.timeRatio;
        res.errorTicks = std::abs(quantized_ticks - ideal_ticks);

        int safe_ppq = (ppq > 0) ? ppq : 960;
        double ms_per_tick = 60000.0 / (bpm * safe_ppq);
        res.errorMs = res.errorTicks * ms_per_tick;

        return res;
    }
}