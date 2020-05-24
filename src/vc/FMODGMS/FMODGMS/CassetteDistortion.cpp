#include "CassetteDistortion.h"
#include <stdlib.h>
#include "ConstantReader.h"

using namespace Cassette;

constexpr size_t BUFFERSIZE = 1024;
CassetteDistortion::CassetteDistortion() : m_ringBuffer(BUFFERSIZE)
{}

float compress(float x, float mult, float thresh, float ramp)
{
    x *= mult;

    float ax = abs(x);
    if (ax > thresh)
    {
        const float diff = ax - thresh;
        ax = thresh + ramp * diff;
    }

    if (x >= 0)
    {
        return ax;
    }
    else
    {
        return -ax;
    }
}

std::vector<float> highPass(const std::vector<float>& data)
{
    const size_t len = data.size();
    std::vector<float> ret;
    ret.resize(len);

    ret[0] = data[0];

    constexpr float alpha = 1.0;

    for (uint32_t i = 1; i < len; i++)
    {
        const float x = alpha * (ret[i-1] + data[i] - data[i - 1]);
        ret[i] = x;
    }

    return ret;
}

std::vector<float> lowPass(const std::vector<float>& data)
{
    const size_t len = data.size();
    std::vector<float> ret;
    ret.resize(len);

    ret[0] = data[0];

    constexpr float alpha = 1.0;

    for (uint32_t i = 1; i < len; i++)
    {
        const float x = ret[i - 1] + alpha * (data[i] - ret[i - 1]);
        ret[i] = x;
    }

    return ret;
}

std::vector<float> CassetteDistortion::Run(const std::vector<float>& data)
{
    const size_t len = data.size();
    std::vector<float> ret;
    ret.resize(len);

    //constexpr float MULT = 3.4;
    //constexpr float THRESH = 0.7;
    //constexpr float RAMP = 0.2;
    const double mult = Constants::Globals.GetDouble("cassette_dist_compress_mult");
    const double thresh = Constants::Globals.GetDouble("cassette_dist_compress_thresh");
    const double ramp = Constants::Globals.GetDouble("cassette_dist_compress_ramp");

    //const auto highPassed = highPass(data);
    const auto bandPassed = lowPass(data);

    for (uint32_t i = 0; i < len; i++)
    {
        ret[i] = compress(bandPassed[i], mult, thresh, ramp);
    }

    return ret;
}

