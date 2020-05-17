#include "Cassette.h"
#include <algorithm>

using namespace Cassette;

/*
size_t Cassette::initRecordBuffer()
{
    _recordBuffers.resize(_recordBuffers.size() + 1);
    return _recordBuffers.size() - 1;
}

std::optional<RecordBuffer&> Cassette::getRecordBuffer(size_t id)
{
    if (id < _recordBuffers.size())
    {
        return _recordBuffers[id];
    }

    return {};
}
*/

CassetteDSP::CassetteDSP(size_t count)
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
}

void CassetteDSP::SetActive(size_t id)
{
    m_active = std::min(id, m_recordBuffers.size() - 1);
}

size_t CassetteDSP::GetActive()
{
    return m_active;
}

void CassetteDSP::SetPlaybackRate(double playbackRate)
{
    m_playbackRate = std::max(0.0, playbackRate);
}

bool CassetteDSP::Register(FMOD::System* sys, std::string& error)
{
    FMOD_RESULT result = sys->createDSP(&m_dspDescr, &m_dsp); 
    if (result != FMOD_OK)
    {
        error = "Could not create DSP";
        return false;
    }

    FMOD::ChannelGroup *masterGroup = nullptr;
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
    int* outchannels)
{
    for (uint32_t samp = 0; samp < length; samp++) 
    { 
        for (uint32_t chan = 0; chan < *outchannels; chan++)
        {
			const uint32_t offset = (samp * *outchannels) + chan;
			float value = inbuffer[offset] * 1.f;

            if (chan < 2)
            {
                value += 0.9 * this->m_recordBuffers[chan].ReadOffset(-44100);
                this->m_recordBuffers[chan].Push(value);
            }

			if (value > 1.f)
			{
				value = 1.f;
			}
			else if (value < -1.f)
			{
				value = -1.f;
			}

            /*
			if (samp < RECORDBUFFER_SIZE)
			{
				*(recordBuffer + samp) = value;
			}
            */

			outbuffer[offset] = value;
        }
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
    m_pos = 0;
}

void RecordBuffer::Push(float f)
{
    m_buffer[m_pos] = f;
    m_pos++;
    if (m_pos >= m_buffer.size())
    {
        m_pos = 0;
    }
}

float RecordBuffer::ReadOffset(int offset)
{
    int pos = (int)m_pos + offset;
    if (pos < 0)
    {
        pos += (int)m_buffer.size();
    }
    else if (pos >= (int)m_buffer.size())
    {
        pos -= m_buffer.size();
    }

    return m_buffer[pos];
}
