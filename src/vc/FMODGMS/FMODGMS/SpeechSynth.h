#pragma once

#include <vector>
#include <memory>
#include <optional>
#include <string>
#include "fmod_common.h"
#include "fmod.hpp"
#include "fmod_errors.h"
#include "RingBuffer.h"

class SpeechSynthDSP
{
public:
    SpeechSynthDSP();

    enum class SpeechSynthState
    {
        SYNTH_TALKING,
        SYNTH_STOPPED,
    };

    bool SpeechSynthDSP::Register(FMOD::System* sys, std::string& error);

    FMOD_RESULT Callback(
        float* inbuffer,
        float* outbuffer,
        uint32_t length,
        int inchannels,
        int* outchannels);

    static FMOD_RESULT F_CALLBACK SpeechSynthDspGenericCallback(
        FMOD_DSP_STATE* dsp_state,
        float* inbuffer,
        float* outbuffer,
        uint32_t length,
        int inchannels,
        int* outchannels);

private:
    FMOD::DSP* m_dsp;
    FMOD_DSP_DESCRIPTION m_dspDescr;
    SpeechSynthState m_state;
    uint32_t m_curSample;
    RingBuffer m_prevSamples;
};
