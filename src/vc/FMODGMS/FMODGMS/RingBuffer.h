#pragma once 
#include <vector>

class RingBuffer
{
public:
    RingBuffer() = delete;
    RingBuffer(size_t bufferSize);
    ~RingBuffer();

    void Push(float x);
    float ReadOffset(int offset) const;
private:
    float* m_buffer;
    size_t m_currentPos;
    size_t m_size;

    size_t PosWrapping(int offset) const;
};