#pragma once

#include <vector>
#include <memory>
#include <optional>
#include "fmod_common.h"
#include "fmod.hpp"
#include "fmod_errors.h"
#include <string>

namespace Cassette
{
    // About 10 seconds?
    constexpr size_t RECORDBUFFER_SIZE = 44100 * 10 * 4;

    class RecordBuffer
    {
    public:
        RecordBuffer(size_t count);
        void Push(float f);
        float ReadOffset(int offset);
    private:
        std::vector<float> m_buffer;
        size_t m_pos;
    };

    class CassetteDSP
    {
    public:
        CassetteDSP(size_t recordCount);

        bool Register(FMOD::System* sys, std::string& error);

        void SetActive(size_t i);
        size_t GetActive();
        void SetPlaybackRate(double playbackRate);
    private:
        double m_playbackRate = 0;
        size_t m_active = 0;

        FMOD::DSP* m_dsp;
        FMOD_DSP_DESCRIPTION m_dspDescr;

        std::vector<RecordBuffer> m_recordBuffers;

        FMOD_RESULT Callback(
            float* inbuffer,
            float* outbuffer,
            uint32_t length,
            int inchannels,
            int* outchannels);

        static FMOD_RESULT F_CALLBACK CassetteDspGenericCallback(
            FMOD_DSP_STATE* dsp_state,
            float* inbuffer,
            float* outbuffer,
            uint32_t length,
            int inchannels,
            int* outchannels);
    };
}