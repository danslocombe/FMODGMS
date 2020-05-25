#include "CassetteDistortion.h"
#include <stdlib.h>
#include "ConstantReader.h"

using namespace Cassette;

constexpr size_t BUFFERSIZE = 2;
CassetteDistortion::CassetteDistortion() : m_lowpassBuffer(BUFFERSIZE), m_highpassBuffer(BUFFERSIZE)
{}

float CassetteDistortion::Compress(float x, float mult, float thresh, float ramp)
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

std::vector<float> CassetteDistortion::HighPass(const std::vector<float>& data, float alpha)
{
    const size_t len = data.size();
    std::vector<float> ret;
    ret.resize(len);

    float prev = this->m_highpassBuffer.ReadOffset(0);
    ret[0] = prev;

    for (uint32_t i = 1; i < len; i++)
    {
        const float x = alpha * (prev + data[i] - data[i - 1]);
        prev = x;
        ret[i] = x;
    }

    this->m_highpassBuffer.Push(ret[ret.size() - 1]);

    return ret;
}

std::vector<float> CassetteDistortion::LowPass(const std::vector<float>& data, float alpha)
{
    const size_t len = data.size();
    std::vector<float> ret;
    ret.resize(len);

    float prev = this->m_lowpassBuffer.ReadOffset(0);

    for (uint32_t i = 0; i < len; i++)
    {
        const float x = prev + alpha * (data[i] - prev);
        prev = x;
        ret[i] = x;
    }

    this->m_lowpassBuffer.Push(ret[len - 1]);

    return ret;
}

std::vector<float> CassetteDistortion::Run(const std::vector<float>& data)
{

    //constexpr float MULT = 3.4;
    //constexpr float THRESH = 0.7;
    //constexpr float RAMP = 0.2;

    const double preCompressEnabled = Constants::Globals.GetBool("cassette_dist_compress_pre_enabled");
    const double pre_mult = Constants::Globals.GetDouble("cassette_dist_compress_pre_mult");
    const double pre_thresh = Constants::Globals.GetDouble("cassette_dist_compress_pre_thresh");
    const double pre_ramp = Constants::Globals.GetDouble("cassette_dist_compress_pre_ramp");

    const double mult = Constants::Globals.GetDouble("cassette_dist_compress_mult");
    const double thresh = Constants::Globals.GetDouble("cassette_dist_compress_thresh");
    const double ramp = Constants::Globals.GetDouble("cassette_dist_compress_ramp");

    const double highpassAlpha = Constants::Globals.GetDouble("cassette_dist_highpass_alpha");
    const bool highpassEnabled = Constants::Globals.GetBool("cassette_dist_highpass_enabled");

    const double lowpassAlpha = Constants::Globals.GetDouble("cassette_dist_lowpass_alpha");
    const bool lowpassEnabled = Constants::Globals.GetBool("cassette_dist_lowpass_enabled");

    const size_t len = data.size();

    std::vector<float> highPassed;
    std::vector<float> lowPassed;
    std::vector<float> preCompress;
    const std::vector<float>* curData = &data;

    if (preCompressEnabled)
    {
        preCompress.resize(len);

        for (uint32_t i = 0; i < len; i++)
        {
            preCompress[i] = this->Compress((*curData)[i], pre_mult, pre_thresh, pre_ramp);
        }

        curData = &preCompress;
    }

    if (highpassEnabled)
    {
        highPassed = this->HighPass(*curData, highpassAlpha);
        curData = &highPassed;
    }

    if (lowpassEnabled)
    {
        lowPassed = this->LowPass(*curData, lowpassAlpha);
        curData = &lowPassed;
    }

    std::vector<float> ret;
    ret.resize(len);

    for (uint32_t i = 0; i < len; i++)
    {
        ret[i] = this->Compress((*curData)[i], mult, thresh, ramp);
    }

    return ret;
}

