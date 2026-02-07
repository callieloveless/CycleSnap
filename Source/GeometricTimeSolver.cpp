/*
  ==============================================================================
    GeometricTimeSolver.cpp
  ==============================================================================
*/
#include "GeometricTimeSolver.h"

namespace GeoTimeMath
{
    // Solver constraints
    static const int kMaxBisectionIters = 100;
    static const double kEpsilon = 1e-7;

    // Helper: Calculates total duration given a specific per-step scale (s_step).
    // Performs the summation: Sum( delta[i] * s_step^k )
    static double compute_duration_with_step_s(const std::vector<double>& deltas, int n_steps, double s_step, double source_dur)
    {
        if (deltas.empty() || source_dur <= kEpsilon) return 0.0;

        // Optimization: Linear case (s=1.0) is just a scalar multiple
        if (std::abs(s_step - 1.0) < kEpsilon) {
            return (double)n_steps / deltas.size();
        }

        double total = 0.0;
        size_t m_size = deltas.size();

        for (int k = 0; k < n_steps; ++k) {
            total += deltas[k % m_size] * std::pow(s_step, k);
        }

        return total / source_dur;
    }

    // Binary search to find s_step that results in target_r
    static double solve_s_step_bisection(const std::vector<double>& deltas, int n, double target_r, double source_dur)
    {
        // If target R is linear, s must be 1.0
        double linear_r = (double)n / deltas.size();
        if (std::abs(target_r - linear_r) < 0.001) return 1.0;

        double low = 0.00001;
        double high = 2.0;

        // Adaptive bounds: if target is huge/tiny, expand search space first
        int safety = 0;
        while (compute_duration_with_step_s(deltas, n, high, source_dur) < target_r && safety++ < 30) {
            high *= 2.0;
        }

        // Standard bisection
        for (int i = 0; i < kMaxBisectionIters; ++i) {
            double mid = low + (high - low) * 0.5;
            double r_mid = compute_duration_with_step_s(deltas, n, mid, source_dur);

            if (std::abs(r_mid - target_r) < kEpsilon) return mid;

            if (r_mid < target_r) low = mid;
            else high = mid;
        }
        return low + (high - low) * 0.5;
    }

    // Brute force N: supports stepping by 1 (any step) or M (full loops)
    static int find_best_fit_n(const std::vector<double>& deltas, double s_step, double target_r, double source_dur, int step_stride)
    {
        // Sanity limit to prevent hang on bad inputs
        int limit = std::max(1000, (int)deltas.size() * 100);

        double min_diff = std::numeric_limits<double>::max();
        int best_n = step_stride;

        for (int k = step_stride; k <= limit; k += step_stride) {
            double r = compute_duration_with_step_s(deltas, k, s_step, source_dur);
            double diff = std::abs(r - target_r);

            if (diff < min_diff) { min_diff = diff; best_n = k; }

            // Optimization: function is generally monotonic, stop if error grows
            if (r > target_r && diff > min_diff) break;
        }
        return best_n;
    }

    // Iteratively finds N when both End Scale and Total Ratio are locked.
    // This is tricky because s changes as N changes.
    static int find_best_fit_n_with_fixed_end(const std::vector<double>& deltas, double target_end, double target_r, double source_dur, int step_stride)
    {
        // Linear edge case
        if (std::abs(target_end - 1.0) < 0.001) {
            int n = (int)std::round(target_r * deltas.size());

            // Quantize to stride
            if (n % step_stride != 0) {
                n = ((n + step_stride / 2) / step_stride) * step_stride;
                if (n < step_stride) n = step_stride;
            }
            return n;
        }

        int limit = std::max(1000, (int)deltas.size() * 100);
        double min_diff = std::numeric_limits<double>::max();

        // Start search at valid stride (need >1 point for a curve)
        int start = step_stride;
        if (start < 2) start = 2;

        int best_n = start;

        for (int k = start; k <= limit; k += step_stride) {
            // 1. Derive s_step for this specific candidate N
            // s_step = end ^ (1 / (N-1))
            double s_step = std::pow(target_end, 1.0 / (double)(k - 1));

            // 2. Calculate resulting R
            double r = compute_duration_with_step_s(deltas, k, s_step, source_dur);
            double diff = std::abs(r - target_r);

            if (diff < min_diff) {
                min_diff = diff;
                best_n = k;
            }

            if (r > target_r && diff > min_diff) break;
        }
        return best_n;
    }

    CalculationResult solve(Mode mode, double targetReps, double inputBeatRatio, double targetTotalScale, double inputBeatEnd,
        const std::vector<double>& deltas, double sourceDur, double bpm, int ppq, bool constrainToIntegerReps)
    {
        CalculationResult res;

        // --- Prep Data ---
        std::vector<double> work_deltas = deltas;
        double work_dur = sourceDur;

        // Handle empty model edge case
        if (work_deltas.empty()) {
            work_deltas = { 960.0 };
            work_dur = 960.0;
        }

        int m_seg_count = (int)work_deltas.size();
        if (m_seg_count < 1) m_seg_count = 1;

        // Determine step stride
        // If "Integer Loops Only", we step by M. Otherwise step by 1.
        int search_stride = constrainToIntegerReps ? m_seg_count : 1;

        // Basic validation
        if (targetReps <= 0 && mode != Mode::FitToCurve && mode != Mode::FitEndAndRatio) {
            res.message = "Invalid Repetitions"; return res;
        }

        // --- Domain Conversion Helpers ---
        // Convert between "Per-Loop Scale" (User View) and "Per-Step Scale" (Math View)
        auto loopToStep = [&](double s_loop) { return std::pow(s_loop, 1.0 / m_seg_count); };
        auto stepToLoop = [&](double s_step) { return std::pow(s_step, m_seg_count); };

        // --- Solver Logic ---
        switch (mode) {
        case Mode::TargetTotalScale:
            if (targetTotalScale <= 0) { res.message = "Total Scale > 0 required"; return res; }

            res.repetitions = (int)std::round(targetReps * m_seg_count);
            // Apply stride constraint if needed
            if (constrainToIntegerReps) {
                res.repetitions = ((res.repetitions + m_seg_count / 2) / m_seg_count) * m_seg_count;
            }
            if (res.repetitions < 1) res.repetitions = 1;

            res.totalScale = targetTotalScale;
            res.stepScale = solve_s_step_bisection(work_deltas, res.repetitions, targetTotalScale, work_dur);
            res.beatRatio = stepToLoop(res.stepScale);
            res.message = "Solved Beat Ratio";
            break;

        case Mode::FixedBeatRatio:
            if (inputBeatRatio <= 0) { res.message = "Beat Ratio > 0 required"; return res; }

            res.repetitions = (int)std::round(targetReps * m_seg_count);
            if (constrainToIntegerReps) {
                res.repetitions = ((res.repetitions + m_seg_count / 2) / m_seg_count) * m_seg_count;
            }
            if (res.repetitions < 1) res.repetitions = 1;

            res.beatRatio = inputBeatRatio;
            res.stepScale = loopToStep(inputBeatRatio);
            res.totalScale = compute_duration_with_step_s(work_deltas, res.repetitions, res.stepScale, work_dur);
            res.message = "Calculated Total Scale";
            break;

        case Mode::MatchBeatEnd:
            if (inputBeatEnd <= 0) { res.message = "Beat End > 0 required"; return res; }

            res.repetitions = (int)std::round(targetReps * m_seg_count);
            if (constrainToIntegerReps) {
                res.repetitions = ((res.repetitions + m_seg_count / 2) / m_seg_count) * m_seg_count;
            }
            if (res.repetitions < 1) res.repetitions = 1;

            res.stepScale = (res.repetitions > 1)
                ? std::pow(inputBeatEnd, 1.0 / (double)(res.repetitions - 1)) : 1.0;
            res.beatRatio = stepToLoop(res.stepScale);
            res.totalScale = compute_duration_with_step_s(work_deltas, res.repetitions, res.stepScale, work_dur);
            res.message = "Solved Ratio from End";
            break;

        case Mode::FitToCurve:
            if (inputBeatRatio <= 0 || targetTotalScale <= 0) { res.message = "Invalid Input"; return res; }
            res.beatRatio = inputBeatRatio;
            res.stepScale = loopToStep(inputBeatRatio);
            res.totalScale = targetTotalScale;
            res.repetitions = find_best_fit_n(work_deltas, res.stepScale, targetTotalScale, work_dur, search_stride);
            res.message = "Solved Repetitions (Curve)";
            break;

        case Mode::FitEndAndRatio:
            if (inputBeatEnd <= 0 || targetTotalScale <= 0) { res.message = "Invalid Input"; return res; }
            res.beatEnd = inputBeatEnd;
            res.totalScale = targetTotalScale;

            // 1. Find best N with stride
            res.repetitions = find_best_fit_n_with_fixed_end(work_deltas, inputBeatEnd, targetTotalScale, work_dur, search_stride);

            // 2. Back-calculate s for that N
            if (res.repetitions > 1)
                res.stepScale = std::pow(inputBeatEnd, 1.0 / (double)(res.repetitions - 1));
            else
                res.stepScale = 1.0;

            res.beatRatio = stepToLoop(res.stepScale);
            res.message = "Solved Repetitions (End+Ratio)";
            break;
        }

        // Populate inferred stats
        if (mode != Mode::FitEndAndRatio) {
            if (res.repetitions > 0) res.beatEnd = std::pow(res.stepScale, res.repetitions - 1);
        }

        res.success = true;

        // --- Verification ---
        // Calculate the actual realized ticks to detect quantization drift
        double quantized_ticks = 0.0;
        for (int k = 0; k < res.repetitions; ++k) {
            double exact_dt = work_deltas[k % m_seg_count] * std::pow(res.stepScale, k);
            quantized_ticks += (double)std::llround(exact_dt);
        }

        res.realizedScale = quantized_ticks / work_dur;
        double ideal_ticks = work_dur * res.totalScale;
        res.errorTicks = std::abs(quantized_ticks - ideal_ticks);

        int safe_ppq = (ppq > 0) ? ppq : 960;
        double ms_per_tick = 60000.0 / (bpm * safe_ppq);
        res.errorMs = res.errorTicks * ms_per_tick;

        return res;
    }
}