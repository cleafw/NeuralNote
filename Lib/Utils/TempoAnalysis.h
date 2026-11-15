#pragma once

#include <optional>
#include <span>
#include <vector>

namespace nn
{
std::optional<double> estimateTempo(std::span<const float> samples, int sampleRate);

class TapTempo
{
public:
    std::optional<double> tap(double seconds);
    void reset() { mTimes.clear(); }
    int count() const { return static_cast<int>(mTimes.size()); }

private:
    std::vector<double> mTimes;
};
}
