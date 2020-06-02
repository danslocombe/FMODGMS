#include "SpeechSynth.h"
#include "ConstantReader.h"
#include <math.h>
#include "StringHelpers.h"

SpeechSynthDSP::SpeechSynthDSP() : m_prevSamples(32)
{
    // Do we need this?
    memset(&m_dspDescr, 0, sizeof(m_dspDescr));
    
    strncpy_s(m_dspDescr.name, "Speech Synth DSP", sizeof(m_dspDescr.name));
    m_dspDescr.version = 0x00010000;
    m_dspDescr.numinputbuffers = 1;
    m_dspDescr.numoutputbuffers = 1;
    m_dspDescr.read = SpeechSynthDspGenericCallback; 
    m_dspDescr.userdata = (void *)0x12345678; 

    m_state = SpeechSynthState::SYNTH_STOPPED;
    m_curSample = 0;
}

bool SpeechSynthDSP::Register(FMOD::System* sys, std::string& error)
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

float PulseWidthGenerator(uint32_t t, double freq, double pulseWidth)
{
    const double xx = static_cast<double>(t) * freq;
    const double yy = fmod(xx, 6.282);

    if (yy < pulseWidth)
    {
        return 1.0;
    }
    else
    {
        return -1.0;
    }
}

float SinGenerator(uint32_t t, float freq)
{
    return sin(static_cast<float>(t) * freq);
}

FMOD_RESULT SpeechSynthDSP::Callback(
    float* inbuffer,
    float* outbuffer,
    uint32_t length,
    int inchannels,
    int* outChannels)
{
    const double freqMult = Constants::Globals.GetDouble("speech_synth_freq_mult");
    const double freqLfoSpeed = Constants::Globals.GetDouble("speech_synth_freq_lfo_speed");
    const double freqLfoDepth = Constants::Globals.GetDouble("speech_synth_freq_lfo_depth");

    double pulseWidth = 6.282 * Constants::Globals.GetDouble("speech_synth_pulse_width");
    const double synthVol = Constants::Globals.GetDouble("speech_synth_vol");
    const auto shape = Constants::Globals.GetString("speech_synth_shape");
    const double pulseWidthLfoSpeed = Constants::Globals.GetDouble("speech_synth_pulse_width_lfo_speed");
    const double pulseWidthLfoDepth = Constants::Globals.GetDouble("speech_synth_pulse_width_lfo_depth");

    const bool sinWave = stringEqualIgnoreCase(shape, "sin");

    for (uint32_t samp = 0; samp < length; samp++) 
    {
        for (int chan = 0; chan < *outChannels; chan++)
        {
			const uint32_t offset = (samp * *outChannels) + chan;
			float value = inbuffer[offset] * 1.f;

            m_curSample++;

            const float frequency = freqMult + freqLfoDepth * SinGenerator(m_curSample, freqLfoSpeed);

            float oscValue;

            if (sinWave)
            {
                oscValue = SinGenerator(m_curSample, frequency);
            }
            else
            {
                pulseWidth += pulseWidthLfoDepth * SinGenerator(m_curSample, pulseWidthLfoSpeed);
                oscValue = PulseWidthGenerator(m_curSample, frequency, pulseWidth);
            }

            value += synthVol * oscValue;

			outbuffer[offset] = value;
        }
    } 

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK SpeechSynthDSP::SpeechSynthDspGenericCallback(
    FMOD_DSP_STATE* dsp_state,
    float* inbuffer,
    float* outbuffer,
    uint32_t length,
    int inchannels,
    int* outchannels)
{
    FMOD_RESULT result;

    FMOD::DSP *thisdsp = reinterpret_cast<FMOD::DSP *>(dsp_state->instance); 

    SpeechSynthDSP* speechSynthObj;
    result = thisdsp->getUserData(reinterpret_cast<void **>(&speechSynthObj));

    if (result != FMOD_OK)
    {
        return result;
    }

    return speechSynthObj->Callback(inbuffer, outbuffer, length, inchannels, outchannels);
}
