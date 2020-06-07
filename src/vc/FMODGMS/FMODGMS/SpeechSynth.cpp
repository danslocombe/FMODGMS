#include "SpeechSynth.h"
#include "ConstantReader.h"
#include "StringHelpers.h"

SpeechSynthDSP::SpeechSynthDSP() : m_freqBuf(128)
{
}

bool SpeechSynthDSP::Register(FMOD::System* sys, std::string& error)
{
    const bool success = this->m_synth.Register(sys, error);
    if (success)
    {
        this->m_synth.SetEnabled(true);
    }

    return success;
}

void SpeechSynthDSP::Talk(const std::string_view& text)
{
    m_text = std::string(text);
    m_textCurChar = 0;
    m_talking = true;
}

void SpeechSynthDSP::UpdateConfigFromReader()
{
    auto& config = m_synth.GetConfigMut();
    config.AmpASDR.Attack = Constants::Globals.GetDouble("speech_synth_amp_a");
    config.AmpASDR.Decay = Constants::Globals.GetDouble("speech_synth_amp_d");
    config.AmpASDR.Sustain = Constants::Globals.GetDouble("speech_synth_amp_s");
    config.AmpASDR.Release = Constants::Globals.GetDouble("speech_synth_amp_r");
    config.AmpSmoothK = Constants::Globals.GetDouble("speech_synth_amp_smooth");

    const auto shape = Constants::Globals.GetString("speech_synth_shape");
    if (stringEqualIgnoreCase(shape, "sin"))
    {
        config.Wave = WaveType::SIN;
    }
    else if (stringEqualIgnoreCase(shape, "saw"))
    {
        config.Wave = WaveType::SAW;
    }
    else
    {
        config.Wave = WaveType::PULSE;
    }

    config.PulseWidth = 6.282 * Constants::Globals.GetDouble("speech_synth_pulse_width");
    config.Freq = Constants::Globals.GetDouble("speech_synth_freq");
    config.FreqSmoothK = Constants::Globals.GetDouble("speech_synth_freq_smooth");

    config.LowPassAlpha = Constants::Globals.GetDouble("speech_synth_low_pass_alpha");
}

void SpeechSynthDSP::MutateConfig(char c)
{
    auto& config = m_synth.GetConfigMut();

    srand((uint32_t)c);

    //config.SinWave = true;

    {
        //const double pulseMod = Constants::Globals.GetDouble("speech_synth_shape_mod_mult");
        //const double frac = fmod((double)c * pulseMod, 1.0);
        const int pwr = rand() % 100;
        config.PulseWidth = 6.282 * (double)(pwr) / 100.0;
    }

    config.AmpASDR.Attack += (double)(rand() % 1000) - 500.0;

    config.Freq += Constants::Globals.GetDouble("speech_synth_freq_mod") * (double)(rand() % 100) / 100.0;
}

void SpeechSynthDSP::NextChar(uint32_t pos)
{
    m_textCurChar = pos;
    const char c = m_text[pos];
    if (c == ' ')
    {
        m_curCharLen = 0;
        m_curCharEndWait = Constants::Globals.GetUint("speech_space_dur");
    }
    else
    {
        if (!Constants::Globals.GetBool("speech_synth_from_config"))
        {
            this->UpdateConfigFromReader();
            this->MutateConfig(c);
        }
        m_curCharLen = Constants::Globals.GetUint("speech_synth_char_len");
        m_curCharEndWait = Constants::Globals.GetUint("speech_synth_end_dur");
    }

    m_curCharT = 0;
}

void SpeechSynthDSP::Tick()
{
    if (Constants::Globals.GetBool("speech_synth_from_config"))
    {
        this->UpdateConfigFromReader();
    }

    if (m_talking)
    {
        m_curCharT += 1;
        if (m_curCharT > m_curCharLen + m_curCharEndWait)
        {
            if (m_textCurChar < m_text.size())
            {
                this->NextChar(++m_textCurChar);
            }
            else
            {
                m_talking = false;
                m_text = "";
                m_curCharT = 0;
                m_curCharLen = 0;
            }
        }

        this->m_synth.SetKeydown(m_curCharT < m_curCharLen);
    }
    else
    {
        this->m_synth.SetKeydown(false);
    }
}