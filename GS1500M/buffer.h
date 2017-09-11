#pragma once

#include <memory>

using ByteBuffer = std::unique_ptr<uint8_t []>;

class Buffer
{
public:
    Buffer(size_t buffSize)
        : bsize(buffSize),
          head(0),
          tail(0),
          buf(std::make_unique<uint8_t []>(bsize))
    {
    }

    void push(const uint8_t& data)
    {
        buf[head] = data;
        head++;
        if(head >= bsize)
        {
            head = 0;
        }
    }

    uint8_t pop()
    {
        uint8_t& data = buf[tail];
        tail++;
        if(tail >= bsize)
        {
            tail = 0;
        }
        return data;
    }

    bool empty()
    {
        return (tail == head);
    }

    void rewind(size_t amount)
    {
        tail -= amount;
        if(static_cast<int32_t>(tail) < 0)
        {
            tail += bsize;
        }
    }

private:
    size_t bsize;
    volatile size_t head;
    volatile size_t tail;
    ByteBuffer buf;
};
