#include "FMSynth.h"

FMSynthDSP::FMSynthDSP() : m_prevSamples(32)
{
    // Do we need this?
    memset(&m_dspDescr, 0, sizeof(m_dspDescr));
    
    strncpy_s(m_dspDescr.name, "FM Synth DSP", sizeof(m_dspDescr.name));
    m_dspDescr.version = 0x00010000;
    m_dspDescr.numinputbuffers = 1;
    m_dspDescr.numoutputbuffers = 1;
    m_dspDescr.read = FMSynthGenericCallback; 
    m_dspDescr.userdata = (void *)0x12345678; 

    m_curSample = 0;
}

bool FMSynthDSP::Register(FMOD::System* sys, std::string& error)
{
    // TODO refactor duplicate register function into abstract DSP class.
    FMOD_RESULT result = sys->createDSP(&m_dspDescr, &m_dsp);
    if (result != FMOD_OK)
    {
        error = "Could not create DSP";
        return false;
    }

    FMOD::ChannelGroup* masterGroup = nullptr;
    result = sys->getMasterChannelGroup(&masterGroup);

    if (result != FMOD_OK && masterGroup != nullptr)
    {
        error = "Could not get master channel";
        return false;
    }

    // Push to end of dsp list.
    result = masterGroup->addDSP(FMOD_CHANNELCONTROL_DSP_TAIL, m_dsp);
    if (result != FMOD_OK)
    {
        error = "Could not add dsp";
        return false;
    }

    m_dsp->setUserData(reinterpret_cast<void*>(this));

    return true;
}

float PulseWidthGenerator(double samp, double pulseWidth)
{
    const double yy = fmod(samp, 6.282);

    if (yy < pulseWidth)
    {
        return 1.0;
    }
    else
    {
        return -1.0;
    }
}

float SawGenerator(double samp)
{
    const double yy = fmod(samp, 6.282);

    return 2.0 * ((yy / 6.282) - 0.5);
}

void FMSynthDSP::FillBuffer(std::vector<float>& buffer)
{
    if (!m_enabled)
    {
        return;
    }

    const double baseSynthVol = 0.2;
    const double freqMult = 0.03 * m_config.Freq;// Constants::Globals.GetDouble("speech_synth_freq_mult");

    double pulseWidth = m_config.PulseWidth;

    for (uint32_t i = 0; i < buffer.size(); i++)
    {
        float amp1;
        if (m_keydown)
        {
            amp1 = m_config.AmpASDR.ValDown((double)m_keydownTime);
            m_keydownTime++;
        }
        else
        {
            amp1 = m_config.AmpASDR.ValUp((double)m_keyupTime);
            m_keyupTime++;
        }

        m_amp = AudioProcessors::lerp(m_amp, amp1, m_config.AmpSmoothK);
        m_freq = AudioProcessors::lerp(m_freq, freqMult, m_config.FreqSmoothK);

        const double samp = (m_pitch * m_freq * (double)m_curSample);
        float oscValue = 0;

        if (m_config.Wave == WaveType::SIN)
        {
            oscValue = sin(samp);
        }
        else if (m_config.Wave == WaveType::PULSE)
        {
            //pulseWidth += pulseWidthLfoDepth * sin((double)m_curSample * pulseWidthLfoSpeed);
            oscValue = PulseWidthGenerator(samp, pulseWidth);
        }
        else if (m_config.Wave == WaveType::SAW)
        {
            oscValue = SawGenerator(samp);
        }

        buffer[i] = m_amp * oscValue;

        m_curSample++;
    }
}

FMOD_RESULT FMSynthDSP::Callback(
    float* inbuffer,
    float* outbuffer,
    uint32_t length,
    int inchannels,
    int* outChannels)
{
    const double synthVol = 0.2;
    const double freqMult = 0.03;// Constants::Globals.GetDouble("speech_synth_freq_mult");
    //const double freqLfoSpeed = Constants::Globals.GetDouble("speech_synth_freq_lfo_speed");
    //const double freqLfoDepth = Constants::Globals.GetDouble("speech_synth_freq_lfo_depth");

    //const double synthVol = Constants::Globals.GetDouble("speech_synth_vol");
    //const auto shape = Constants::Globals.GetString("speech_synth_shape");
    //const double pulseWidthLfoSpeed = Constants::Globals.GetDouble("speech_synth_pulse_width_lfo_speed");
    //const double pulseWidthLfoDepth = Constants::Globals.GetDouble("speech_synth_pulse_width_lfo_depth");


    std::vector<float> oscBuffer;
    oscBuffer.resize(length);
    this->FillBuffer(oscBuffer);

    oscBuffer = m_lowPass.Run(oscBuffer, m_config.LowPassAlpha);

    for (uint32_t samp = 0; samp < length; samp++) 
    {
        for (int chan = 0; chan < *outChannels; chan++)
        {
			const uint32_t offset = (samp * *outChannels) + chan;
            float value = inbuffer[offset];

            {
                value += oscBuffer[samp];

                /*
                const double samp = (m_pitch * freqMult * (double)m_curSample) 
                    + freqLfoDepth * sin((double)m_curSample * m_pitch * freqLfoSpeed);


                //if (samp == 0)
                {
                    //m_freqBuf.Push(100.0 * frequency);
                }
                */

            }

			outbuffer[offset] = value;
        }
    } 

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK FMSynthDSP::FMSynthGenericCallback(
    FMOD_DSP_STATE* dsp_state,
    float* inbuffer,
    float* outbuffer,
    uint32_t length,
    int inchannels,
    int* outchannels)
{
    FMOD_RESULT result;

    FMOD::DSP *thisdsp = reinterpret_cast<FMOD::DSP *>(dsp_state->instance); 

    FMSynthDSP* fmSynth;
    result = thisdsp->getUserData(reinterpret_cast<void **>(&fmSynth));

    if (result != FMOD_OK)
    {
        return result;
    }

    return fmSynth->Callback(inbuffer, outbuffer, length, inchannels, outchannels);
}
