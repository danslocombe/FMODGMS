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
    // About 4 seconds?
    //constexpr size_t RECORDBUFFER_SIZE = 44100 * 2;
    constexpr size_t RECORDBUFFER_SIZE = 24100;

    class RecordBuffer
    {
    public:
        RecordBuffer(size_t count);

        void Push(float f);
        void Seek(int offset);

        float ReadOffset(int offset) const;
        float ReadPos(size_t pos) const;
        float GetPosition() const;
        size_t GetSize() const;
    private:
        std::vector<float> m_buffer;
        size_t m_pos;

        size_t WrapOffset(int offset) const;
    };

    enum class CassetteState
    {
        CASSETTE_PAUSED,
        CASSETTE_PLAYING,
        CASSETTE_RECORDING,
    };

    class CassetteDSP
    {
    public:
        CassetteDSP(size_t recordCount);

        bool Register(FMOD::System* sys, std::string& error);

        void SetActive(size_t i);
        void SetState(CassetteState state);
        void SetPlaybackRate(double playbackRate);

        size_t GetActive() const;
        double GetActivePosition() const;
        double GetWaveform(double pos) const;
    private:
        std::vector<RecordBuffer> m_recordBuffers;
        double m_playbackRate = 0;
        size_t m_active = 0;
        CassetteState m_state = CassetteState::CASSETTE_PAUSED;

        FMOD::DSP* m_dsp;
        FMOD_DSP_DESCRIPTION m_dspDescr;

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