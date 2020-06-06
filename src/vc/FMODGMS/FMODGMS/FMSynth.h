#pragma once

#include <vector>
#include <memory>
#include <optional>
#include <string>
#include "fmod_common.h"
#include "fmod.hpp"
#include "fmod_errors.h"
#include "RingBuffer.h"
#include "AudioProcessors.h"


struct FMSynthConfig
{
    AudioProcessors::ASDRConfig AmpASDR;
    double AmpSmoothK = 1;

    bool SinWave = true;
    double PulseWidth = 0;
    double Freq = 1;
    double FreqSmoothK = 1;

    double LowPassAlpha = 1;
    //ASDRConfig LowPassASDR;
};

class FMSynthDSP
{
public:
    FMSynthDSP();

    bool Register(FMOD::System* sys, std::string& error);

    void SetConfig(FMSynthConfig config)
    {
        m_config = config;
    }

    FMSynthConfig& GetConfigMut()
    {
        return m_config;
    }

    void SetEnabled(bool enabled)
    {
        if (!m_enabled)
        {
            // Retrigger
            //m_curSample = 0;
        }

        m_enabled = enabled;
    }

    void SetKeydown(bool keydown)
    {
        if (keydown != m_keydown)
        {
            m_keydownTime = 0;
            m_keyupTime = 0;
            if (keydown)
            {
                //m_curSample = 0;
            }
        }

        m_keydown = keydown;
    }

    void SetPitch(double pitch)
    {
        m_pitch = pitch;
    }

    void FillBuffer(std::vector<float>& buffer);

    FMOD_RESULT Callback(
        float* inbuffer,
        float* outbuffer,
        uint32_t length,
        int inchannels,
        int* outchannels);

    static FMOD_RESULT F_CALLBACK FMSynthGenericCallback(
        FMOD_DSP_STATE* dsp_state,
        float* inbuffer,
        float* outbuffer,
        uint32_t length,
        int inchannels,
        int* outchannels);
private:
    FMOD::DSP* m_dsp;
    FMOD_DSP_DESCRIPTION m_dspDescr;

    RingBuffer m_prevSamples;
    double m_pitch = 1.0;
    bool m_enabled = false;
    uint32_t m_curSample = 0;
    FMSynthConfig m_config;
    AudioProcessors::LowPass m_lowPass;

    bool m_keydown = false;
    uint32_t m_keydownTime = 0;
    uint32_t m_keyupTime = 0;

    double m_amp = 0;
    double m_freq = 1;
};
