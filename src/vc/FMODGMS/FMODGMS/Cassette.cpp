#include "Cassette.h"
#include "UserData.h"
#include <algorithm>
#include <math.h>

using namespace Cassette;

CassetteDSP::CassetteDSP(size_t recordCount, AnnotationStore* annotationStore, const std::unordered_map<size_t, FMOD::Channel*>* channels) :
    m_annotationStore(annotationStore),
    m_channels(channels)
{
    RecordBuffer buffer(RECORDBUFFER_SIZE);
    m_recordBuffers.emplace_back(std::move(buffer));
    RecordBuffer buffer2(RECORDBUFFER_SIZE);
    m_recordBuffers.emplace_back(std::move(buffer2));

    // Do we need this?
    memset(&m_dspDescr, 0, sizeof(m_dspDescr));
    
    strncpy_s(m_dspDescr.name, "record capture DSP", sizeof(m_dspDescr.name));
    m_dspDescr.version = 0x00010000;
    m_dspDescr.numinputbuffers = 1;
    m_dspDescr.numoutputbuffers = 1;
    m_dspDescr.read = CassetteDspGenericCallback; 
    m_dspDescr.userdata = (void *)0x12345678; 

    m_state = CassetteState::CASSETTE_PAUSED;
}

void CassetteDSP::SetActive(size_t id)
{
    m_active = std::min(id, m_recordBuffers.size() - 1);
}

void CassetteDSP::SetState(CassetteState state)
{
m_state = state;
}

void CassetteDSP::SetPlaybackRate(double playbackRate)
{
    m_playbackRate = std::max(0.0, playbackRate);
}

size_t CassetteDSP::GetActive() const
{
    return m_active;
}


double CassetteDSP::GetWaveform(double pos) const
{
    if (pos > 1.0 || pos < 0.0)
    {
        return 0.0;
    }

    const auto& recordBuffer = this->m_recordBuffers.at(this->m_active);
    double recordBufferPos = pos * (double)(recordBuffer.GetSize() - 1);
    // Floating point modulo not FMOD!
    double intPart;
    const double fracPart = modf(recordBufferPos, &intPart);

    // Interpolate
    const uint32_t lower = (uint32_t)intPart;
    const uint32_t upper = std::min(lower + 1, recordBuffer.GetSize() - 1);

    const float valLower = recordBuffer.ReadPos(lower);
    const float valUpper = recordBuffer.ReadPos(upper);

    return fracPart * valLower + (1 - fracPart) * valUpper;
}

double CassetteDSP::GetActivePosition() const
{
    const auto& x = m_recordBuffers.at(m_active);
    return x.GetPosition();
}

const AnnotationValue& CassetteDSP::GetCurrentWorldAnnotation() const
{
    return this->m_worldCurrentAnnotation;
}

bool CassetteDSP::Register(FMOD::System* sys, std::string& error)
{
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

FMOD_RESULT CassetteDSP::Callback(
    float* inbuffer,
    float* outbuffer,
    uint32_t length,
    int inchannels,
    int* outChannels)
{
    const size_t sampleCount = length * (*outChannels);

    AnnotationValue annotation;

    if (m_state == CassetteState::CASSETTE_PLAYING)
    {
        const auto& buffer = this->m_recordBuffers.at(this->m_active);
        annotation = buffer.ReadOffsetAnnotation(0);
    }
    else
    {
        for (const auto& kv : *m_channels)
        {
            const auto& channel = kv.second;

            bool chanIsPlaying;
            FMOD::Sound* playingSound;
            SoundUserData* userData;
            uint32_t posMs;
            if (channel->isPlaying(&chanIsPlaying) == FMOD_OK && chanIsPlaying
                && channel->getCurrentSound(&playingSound) == FMOD_OK)
            {
                if (playingSound->getUserData(reinterpret_cast<void**>(&userData)) == FMOD_OK
                    && channel->getPosition(&posMs, FMOD_TIMEUNIT_MS) == FMOD_OK)
                {
                    annotation.Value = this->m_annotationStore->GetAnnotation(userData->Id, (double)posMs / 1000.0);
                    break;
                }
            }
        }
    }

    this->m_worldCurrentAnnotation = annotation;

    for (uint32_t samp = 0; samp < length; samp++) 
    { 
        // Buffer of samples to write
        // We want to record in mono so average over channels
        float averagedSample = 0.0;

        for (int chan = 0; chan < *outChannels; chan++)
        {
			const uint32_t offset = (samp * *outChannels) + chan;
			float value = inbuffer[offset] * 1.f;

            averagedSample += value / ((float)(*outChannels));

            if (m_state == CassetteState::CASSETTE_PLAYING)
            {
                constexpr float playbackVolume = 0.4f;
                // Playback in mono
                const auto& buffer = this->m_recordBuffers.at(this->m_active);
                value += playbackVolume * buffer.ReadOffset(samp);
            }

			if (value > 1.f)
			{
				value = 1.f;
			}
			else if (value < -1.f)
			{
				value = -1.f;
			}

			outbuffer[offset] = value;
        }

        if (m_state == CassetteState::CASSETTE_RECORDING)
        {
            auto& recordingBuffer = this->m_recordBuffers.at(this->m_active);
            recordingBuffer.Push(averagedSample, annotation);
        }
    } 

    if (m_state == CassetteState::CASSETTE_PLAYING)
    {
        auto& buffer = this->m_recordBuffers.at(this->m_active);
        buffer.Seek(length);
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK CassetteDSP::CassetteDspGenericCallback(
    FMOD_DSP_STATE* dsp_state,
    float* inbuffer,
    float* outbuffer,
    uint32_t length,
    int inchannels,
    int* outchannels)
{
    FMOD_RESULT result;

    FMOD::DSP *thisdsp = reinterpret_cast<FMOD::DSP *>(dsp_state->instance); 

    CassetteDSP* cassetteObj;
    result = thisdsp->getUserData(reinterpret_cast<void **>(&cassetteObj));

    if (result != FMOD_OK)
    {
        return result;
    }

    return cassetteObj->Callback(inbuffer, outbuffer, length, inchannels, outchannels);
}

RecordBuffer::RecordBuffer(size_t count)
{
    m_buffer.resize(count);
    m_annotations.resize(count);
    m_pos = 0;
}

void RecordBuffer::Push(float f, AnnotationValue annotation)
{
    m_buffer[m_pos] = f;
    m_annotations[m_pos] = annotation;

    m_pos++;

    if (m_pos >= m_buffer.size())
    {
        m_pos = 0;
    }
}

void RecordBuffer::Seek(int offset)
{
    const size_t pos = this->WrapOffset(offset);
    m_pos = pos;
}

float RecordBuffer::ReadOffset(int offset) const
{
    const size_t pos = this->WrapOffset(offset);
    return m_buffer[pos];
}

float RecordBuffer::ReadPos(size_t pos) const
{
    return m_buffer[pos];
}

const AnnotationValue& RecordBuffer::ReadOffsetAnnotation(int offset) const
{
    const size_t pos = this->WrapOffset(offset);
    return m_annotations[pos];
}

const AnnotationValue& RecordBuffer::ReadPosAnnotation(size_t pos) const
{
    return m_annotations[pos];
}

float RecordBuffer::GetPosition() const
{
    return (float)m_pos / (float)m_buffer.size();
}

size_t RecordBuffer::WrapOffset(int offset) const
{
    int pos = (int)m_pos + offset;
    // Assume size > abs(offset) and won't wrap twice.
    if (pos < 0)
    {
        pos += (int)m_buffer.size();
    }
    else if (pos >= (int)m_buffer.size())
    {
        pos -= m_buffer.size();
    }

    return pos;
}

size_t RecordBuffer::GetSize() const
{
    return this->m_buffer.size();
}
