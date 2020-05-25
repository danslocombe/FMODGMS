#pragma once
#include "RingBuffer.h"

namespace Cassette
{
    class CassetteDistortion
    {
    public:
        CassetteDistortion();
        std::vector<float> Run(const std::vector<float>& data);

    private:

        float Compress(float x, float mult, float thresh, float ramp);
        std::vector<float> HighPass(const std::vector<float>& data, float alpha);
        std::vector<float> LowPass(const std::vector<float>& data, float alpha);

        RingBuffer m_lowpassBuffer;
        RingBuffer m_highpassBuffer;
    };
}