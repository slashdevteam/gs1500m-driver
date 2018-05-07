/*
 * Copyright (c) 2018 Slashdev SDG UG
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
