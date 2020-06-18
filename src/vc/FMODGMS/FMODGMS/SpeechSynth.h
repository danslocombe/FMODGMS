#pragma once

#include "FMSynth.h"

class SpeechSynthDSP
{
public:
    SpeechSynthDSP();

    enum class SpeechSynthState
    {
        SYNTH_TALKING,
        SYNTH_STOPPED,
    };


    bool Register(FMOD::System* sys, std::string& error);

    void Talk(const std::string_view& text);
    void SetSpeaker(const std::string_view& speaker);
    void Tick();

    //void SetEnabled(bool enabled)
    //{
    //    m_state = enabled ? SpeechSynthState::SYNTH_TALKING : SpeechSynthState::SYNTH_STOPPED;
    //}

    //void SetPitch(double pitch)
    //{
    //    m_pitch = pitch;
    //}

    std::optional<std::string> TryGetText() const;
    bool IsTalking() const;

    RingBuffer m_freqBuf;
private:
    void UpdateConfigFromReader();
    void MutateConfig(char c);
    void NextChar(uint32_t c);

    FMSynthDSP m_synth;

    std::string m_text;
    std::string m_speaker;

    bool m_talking = false;
    uint32_t m_textCurChar = 0;
    uint32_t m_curCharLen = 0;
    uint32_t m_curCharEndWait = 0;
    uint32_t m_curCharT = 0;
};
