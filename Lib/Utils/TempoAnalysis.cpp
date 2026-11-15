#include "TempoAnalysis.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

namespace
{
std::vector<double> correlationsFor(std::span<const double> onsets, int shortest, int longest)
{
    const auto mean = std::accumulate(onsets.begin(), onsets.end(), 0.0) / onsets.size();
    auto centered = std::vector<double>(onsets.begin(), onsets.end());
    for (auto& value : centered) value -= mean;
    std::vector<double> correlations(2 * longest + 3);
    for (int lag = shortest - 2; lag < static_cast<int>(correlations.size()); ++lag) {
        double cross = 0.0, first = 0.0, second = 0.0;
        for (std::size_t frame = lag; frame < centered.size(); ++frame) {
            const auto left = centered[frame - lag];
            const auto right = centered[frame];
            cross += left * right;
            first += left * left;
            second += right * right;
        }
        if (first > 0.0 && second > 0.0) correlations[lag] = cross / std::sqrt(first * second);
    }
    return correlations;
}

double bpmAtPeak(const std::vector<double>& scores, int peak, double framesPerSecond)
{
    const auto curvature = scores[peak - 1] - 2.0 * scores[peak] + scores[peak + 1];
    const auto correction = curvature < -1.0e-8
        ? std::clamp(0.5 * (scores[peak - 1] - scores[peak + 1]) / curvature, -0.5, 0.5) : 0.0;
    return std::round(framesPerSecond * 600.0 / (peak + correction)) / 10.0;
}

std::optional<double> estimateFromSections(std::span<const double> onsets, double framesPerSecond,
                                           int shortest, int longest)
{
    const auto sectionCount = std::clamp(static_cast<int>(onsets.size() / (12 * framesPerSecond)), 1, 8);
    if (sectionCount < 2) return {};
    std::vector<double> consensus(longest + 2);
    std::vector<int> support(longest + 2);
    for (int section = 0; section < sectionCount; ++section) {
        const auto begin = onsets.size() * section / sectionCount;
        const auto end = onsets.size() * (section + 1) / sectionCount;
        const auto slice = onsets.subspan(begin, end - begin);
        if (*std::max_element(slice.begin(), slice.end()) < 0.008) continue;
        const auto correlations = correlationsFor(slice, shortest, longest);
        std::vector<double> scores(longest + 2);
        double maxScore = 0.0;
        for (int lag = shortest; lag <= longest; ++lag) {
            const auto center = correlations[lag];
            if (center < 0.14 || center < correlations[lag - 1] || center < correlations[lag + 1]) continue;
            const auto harmonic = std::max({correlations[2 * lag - 2], correlations[2 * lag - 1],
                                            correlations[2 * lag], correlations[2 * lag + 1],
                                            correlations[2 * lag + 2]});
            if (harmonic < 0.08) continue;
            scores[lag] = 0.75 * center + 0.25 * harmonic;
            maxScore = std::max(maxScore, scores[lag]);
        }
        for (int lag = shortest; lag <= longest; ++lag) {
            const auto radius = std::max(1, static_cast<int>(std::round(lag * 0.025)));
            double nearby = 0.0;
            for (int candidate = std::max(shortest, lag - radius); candidate <= std::min(longest, lag + radius); ++candidate) {
                const auto penalty = 1.0 - 0.12 * std::abs(candidate - lag) / radius;
                nearby = std::max(nearby, scores[candidate] * penalty);
            }
            if (nearby >= std::max(0.13, 0.67 * maxScore)) {
                consensus[lag] += nearby;
                ++support[lag];
            }
        }
    }
    int best = 0;
    for (int lag = shortest; lag <= longest; ++lag) {
        if (support[lag] < std::max(2, (sectionCount + 1) / 2)) continue;
        if (consensus[lag] > consensus[best]) best = lag;
    }
    if (best == 0) return {};
    for (int lag = shortest; lag <= longest; ++lag) {
        if (support[lag] < std::max(2, (sectionCount + 1) / 2) || consensus[lag] < consensus[best] * 0.84) continue;
        const auto ratio = static_cast<double>(std::max(lag, best)) / std::min(lag, best);
        if (std::abs(ratio - 1.0) > 0.06 && std::abs(ratio - 2.0) > 0.09 && std::abs(ratio - 3.0) > 0.12)
            return {};
    }
    return bpmAtPeak(consensus, best, framesPerSecond);
}
}

std::optional<double> nn::estimateTempo(std::span<const float> samples, int sampleRate)
{
    if (sampleRate < 2000 || samples.size() < static_cast<std::size_t>(sampleRate) * 5) return {};
    const int hop = sampleRate / 100;
    const double framesPerSecond = static_cast<double>(sampleRate) / hop;
    const auto frames = samples.size() / hop;
    std::vector<double> onsets(frames);
    std::array<double, 3> previous {};
    double low = 0.0, mid = 0.0;
    const double lowStep = 1.0 - std::exp(-6.283185307 * 150.0 / sampleRate);
    const double midStep = 1.0 - std::exp(-6.283185307 * 1800.0 / sampleRate);
    double peak = 0.0;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        std::array<double, 3> energy {};
        for (const auto sample : samples.subspan(frame * hop, hop)) {
            low += lowStep * (sample - low);
            mid += midStep * (sample - mid);
            energy[0] += low * low;
            energy[1] += (mid - low) * (mid - low);
            energy[2] += (sample - mid) * (sample - mid);
        }
        for (std::size_t band = 0; band < energy.size(); ++band) {
            const double level = std::log1p(100.0 * std::sqrt(energy[band] / hop));
            if (frame > 2) onsets[frame] += std::max(0.0, level - previous[band]);
            previous[band] = level;
        }
        peak = std::max(peak, onsets[frame]);
    }
    if (peak < 0.015) return {};
    auto smoothed = onsets;
    for (std::size_t frame = 3; frame + 3 < frames; ++frame) {
        smoothed[frame] = (onsets[frame - 3] + onsets[frame + 3]
                          + 2.0 * (onsets[frame - 2] + onsets[frame + 2])
                          + 3.0 * (onsets[frame - 1] + onsets[frame + 1])
                          + 4.0 * onsets[frame]) / 16.0;
    }
    const auto mean = std::accumulate(smoothed.begin(), smoothed.end(), 0.0) / frames;
    int peaks = 0;
    const auto threshold = std::max(3.0 * mean, peak * 0.10);
    for (std::size_t frame = 1; frame + 1 < frames; ++frame) {
        if (smoothed[frame] > threshold && smoothed[frame] > smoothed[frame - 1]
            && smoothed[frame] >= smoothed[frame + 1]) ++peaks;
    }
    const int shortest = static_cast<int>(std::ceil(framesPerSecond * 60.0 / 240.0));
    const int longest = static_cast<int>(std::floor(framesPerSecond * 60.0 / 45.0));
    if (peaks < 4) return estimateFromSections(smoothed, framesPerSecond, shortest, longest);
    const auto correlations = correlationsFor(smoothed, shortest, longest);
    int best = 0;
    double bestScore = 0.0;
    for (int lag = shortest; lag <= longest; ++lag) {
        const double center = correlations[lag];
        if (center < 0.32 || center < correlations[lag - 1] || center < correlations[lag + 1]) continue;
        const auto harmonic = std::max({correlations[2 * lag - 1], correlations[2 * lag], correlations[2 * lag + 1]});
        if (harmonic < 0.20) continue;
        const double score = 0.80 * center + 0.20 * harmonic;
        if (score > bestScore + 0.11) {
            best = lag;
            bestScore = score;
        }
    }
    if (best == 0) return estimateFromSections(smoothed, framesPerSecond, shortest, longest);
    return bpmAtPeak(correlations, best, framesPerSecond);
}

std::optional<double> nn::TapTempo::tap(double seconds)
{
    if (!std::isfinite(seconds)) return {};
    if (!mTimes.empty()) {
        const double gap = seconds - mTimes.back();
        if (gap > 2.5 || gap < 0.0) reset();
        else if (gap < 0.15) return {};
    }
    mTimes.push_back(seconds);
    if (mTimes.size() > 9) mTimes.erase(mTimes.begin());
    if (mTimes.size() < 4) return {};
    std::vector<double> intervals;
    for (std::size_t index = 1; index < mTimes.size(); ++index)
        intervals.push_back(mTimes[index] - mTimes[index - 1]);
    std::sort(intervals.begin(), intervals.end());
    const auto middle = intervals.size() / 2;
    const auto period = intervals.size() % 2 != 0 ? intervals[middle]
        : (intervals[middle - 1] + intervals[middle]) * 0.5;
    return std::round(600.0 / period) / 10.0;
}
