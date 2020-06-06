#pragma once

#include <vector>

namespace AudioProcessors
{
    float Compress(float x, float mult, float thresh, float ramp);
    std::vector<float> HighPass(const std::vector<float>& data, float alpha);
    std::vector<float> LowPass(const std::vector<float>& data, float alpha);
}
