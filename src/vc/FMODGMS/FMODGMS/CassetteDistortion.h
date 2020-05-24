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
        RingBuffer m_ringBuffer;
    };
}