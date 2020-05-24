#include "Cassette.h"
#include "UserData.h"
#include "ConstantReader.h"
#include <algorithm>
#include <cmath>

using namespace Cassette;

CassetteDSP::CassetteDSP(size_t recordCount, AnnotationStore* annotationStore, const std::unordered_map<size_t, FMOD::Channel*>* channels) :
    m_annotationStore(annotationStore),
    m_channels(channels),
    m_control(CassetteControl(RECORDBUFFER_SIZE))
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
    m_active = min(id, m_recordBuffers.size() - 1);
}

void CassetteDSP::SetState(CassetteState state)
{
    m_state = state;

    if (m_state == CassetteState::CASSETTE_PLAYING)
    {
        m_control.StartPlaying();
    }
    else
    {
        m_control.StopPlaying();
    }
}

void CassetteDSP::SetPlaybackRate(double playbackRate)
{
    m_playbackRate = max(0.0, playbackRate);
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

    return recordBuffer.ReadPosInterpolate(recordBufferPos);
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

AnnotationValue CassetteDSP::GetCurrentAnnotationValue()
{
    constexpr double VEL_THRESHOLD = 0.01;
    if (m_state != CassetteState::CASSETTE_RECORDING && abs(m_control.GetVel()) > VEL_THRESHOLD)
    {
        // Take annotation from casseette
        const auto& buffer = this->m_recordBuffers.at(this->m_active);
        return buffer.ReadOffsetAnnotation(0);
    }
    else
    {
        // Take annotation from environment
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
                    return AnnotationValue{ this->m_annotationStore->GetAnnotation(userData->Id, (double)posMs / 1000.0) };
                }
            }
        }
    }

    return AnnotationValue();
}

std::vector<float> CassetteDSP::PlayCassetteSamples(size_t count)
{
    std::vector<float> samples;
    samples.resize(count);

    for (uint32_t i = 0; i < count; i++)
    {
        // Playback in mono
        const auto& buffer = this->m_recordBuffers.at(this->m_active);
        const double readPos = m_control.GetPos() + i * m_control.GetVel();
        samples[i] = buffer.ReadPosInterpolate(readPos);
    }

    std::vector<float> distorted = m_distort.Run(samples);
    return distorted;
}

float noise(float amp)
{
    float noise = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    if (rand() % 2 == 0)
    {
        noise = -noise;
    }

    return noise * amp;
}

FMOD_RESULT CassetteDSP::Callback(
    float* inbuffer,
    float* outbuffer,
    uint32_t length,
    int inchannels,
    int* outChannels)
{
    const double cassettePlaybackVolume = Constants::Globals.GetDouble("cassette_playback_volume");
    const size_t sampleCount = length * (*outChannels);

    AnnotationValue annotation = this->GetCurrentAnnotationValue();
    this->m_worldCurrentAnnotation = annotation;

    std::vector<float> cassettePlayBuffer;
    if (m_state != CassetteState::CASSETTE_RECORDING)
    {
        cassettePlayBuffer = this->PlayCassetteSamples(length);
    }

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

            if (cassettePlayBuffer.size() > 0)
            {
                value += cassettePlaybackVolume * cassettePlayBuffer.at(samp);
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
            const float recordSample = averagedSample + noise(0.01);
            recordingBuffer.Push(recordSample, annotation);
        }
    } 

    if (m_state != CassetteState::CASSETTE_RECORDING)
    {
        auto& buffer = this->m_recordBuffers.at(this->m_active);
        m_control.Tick(static_cast<double>(length));
        const uint32_t pos = static_cast<uint32_t>(m_control.GetPos());
        buffer.Seek(pos);
        //buffer.SeekOffset(length);
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

void RecordBuffer::Seek(size_t pos)
{
    m_pos = pos;
}

void RecordBuffer::SeekOffset(int offset)
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

float RecordBuffer::ReadPosInterpolate(double pos) const
{
    double intPart;
    const float fracPart = static_cast<float>(modf(pos, &intPart));

    uint32_t lower = (uint32_t)intPart;
    if (lower >= m_buffer.size())
    {
        lower -= m_buffer.size();
    }
    uint32_t upper = lower + 1;
    if (upper >= m_buffer.size())
    {
        upper -= m_buffer.size();
    }

    const float valLower = this->ReadPos(lower);
    const float valUpper = this->ReadPos(upper);

    return fracPart * valLower + (1.0 - fracPart) * valUpper;
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
