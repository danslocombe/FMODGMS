#include "RingBuffer.h"

RingBuffer::RingBuffer(size_t bufferSize)
{
    m_size = bufferSize;
    m_currentPos = 0;

    m_buffer = reinterpret_cast<float*>(malloc(sizeof(float) * bufferSize));
    for (uint32_t i = 0; i < bufferSize; i++)
    {
        m_buffer[i] = 0.0;
    }
}

RingBuffer::~RingBuffer()
{
    free(m_buffer);
}

void RingBuffer::Push(float x)
{
    m_currentPos++;

    if (m_currentPos == m_size)
    {
        m_currentPos = 0;
    }

    m_buffer[m_currentPos] = x;
}

float RingBuffer::ReadOffset(int offset) const
{
    const size_t pos = this->PosWrapping(offset);
    return m_buffer[pos];
}

size_t RingBuffer::PosWrapping(int offset) const
{
    // Assume: offset < buffersize
    const int size = static_cast<int>(m_size);

    int pos = m_currentPos + offset;
    if (pos < 0)
    {
        pos += size;
    }
    else if (pos > size)
    {
        pos -= size;
    }

    return static_cast<size_t>(pos);
}