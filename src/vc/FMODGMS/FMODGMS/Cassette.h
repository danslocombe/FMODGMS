#pragma once

#include <vector>
#include <memory>
#include <optional>
#include "fmod_common.h"
#include "fmod.hpp"
#include "fmod_errors.h"
#include <string>
#include "AnnotationStore.h"
#include "CassetteControl.h"
#include "CassetteDistortion.h"
#include "SpeechSynth.h"

namespace Cassette
{
    // About 4 seconds?
    //constexpr size_t RECORDBUFFER_SIZE = 44100 * 2;
    constexpr size_t RECORDBUFFER_SIZE = static_cast<size_t>(24100 * 2.5);

    struct AnnotationValue
    {
        std::optional<std::string> Value;
    };

    class RecordBuffer
    {
    public:
        RecordBuffer(size_t count);

        void Push(float f, AnnotationValue annotation);
        void Seek(size_t pos);
        void SeekOffset(int offset);

        float ReadOffset(int offset) const;
        float ReadPos(size_t pos) const;
        float ReadPosInterpolate(double pos) const;

        const AnnotationValue& ReadOffsetAnnotation(int offset) const;
        const AnnotationValue& ReadPosAnnotation(size_t pos) const;

        float GetPosition() const;
        uint32_t GetPositionSample() const;
        size_t GetSize() const;
    private:
        std::vector<float> m_buffer;

        // TODO Optimise, this is dumb
        std::vector<AnnotationValue> m_annotations;

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
        CassetteDSP(
            size_t recordCount,
            AnnotationStore* annotationStore,
            const std::unordered_map<std::size_t, FMOD::Channel*>* channels,
            const SpeechSynthDSP* speechSynth);

        bool Register(FMOD::System* sys, std::string& error);

        void SetActive(size_t i);
        void SetState(CassetteState state);
        void SetPlaybackRate(double playbackRate);

        size_t GetActive() const;
        double GetActivePosition() const;
        double GetWaveform(double pos) const;
        const AnnotationValue& GetCurrentWorldAnnotation() const;
    private:
        std::vector<RecordBuffer> m_recordBuffers;
        double m_playbackRate = 0;
        size_t m_active = 0;
        CassetteState m_state = CassetteState::CASSETTE_PAUSED;

        CassetteControl m_control;
        CassetteDistortion m_distort;

        AnnotationStore* m_annotationStore;
        const std::unordered_map<std::size_t, FMOD::Channel*>* m_channels;

        const SpeechSynthDSP* m_speechSynth;

        FMOD::DSP* m_dsp;
        FMOD_DSP_DESCRIPTION m_dspDescr;

        AnnotationValue m_worldCurrentAnnotation;

        AnnotationValue GetCurrentAnnotationValue();
        std::vector<float> PlayCassetteSamples(size_t count);

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