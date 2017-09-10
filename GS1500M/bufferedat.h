#pragma once

#include "PinNames.h"
#include "RawSerial.h"
#include "Thread.h"
#include "Callback.h"
#include "Timer.h"
#include "PlatformMutex.h"
#include "specialsequence.h"
#include "buffer.h"
#include <regex>
#include <vector>
#include <cstdarg>

using mbed::RawSerial;
using mbed::callback;
using mbed::Callback;
using mbed::Timer;
using rtos::Thread;

class BufferedAT
{
public:
    BufferedAT(PinName tx, PinName rx, size_t baud)
        : serial(tx, rx, baud),
          oob(osPriorityHigh, 8192/2),
          ob(4*1512),
          rb(512),
          timeout(100),
          pushed(0)
    {
        oob.start(callback(this, &BufferedAT::checkOob));
        serial.attach(callback(this, &BufferedAT::bufferRx), mbed::SerialBase::RxIrq);
    }

    bool send(const char *command, ...)
    {
        va_list args;
        va_start(args, command);
        bool res = vsend(command, args);
        va_end(args);
        return res;
    }

    bool recv(const char* sequence)
    {
        return recveiveSequence(rb, sequence);
    }

    void registerSequence(const std::string& _sequence, Callback<void()> callback)
    {
        specialSequences.emplace_back(std::make_pair(SpecialSequence(_sequence), callback));
    }

    size_t write(const char *data, size_t size)
    {
        size_t i = 0;
        lock();
        for(; i < size; ++i)
        {
            if(serial.putc(data[i]) < 0)
            {
                i = 0;
                break;
            }
        }
        unlock();
        return i;
    }

    size_t read(char *data, size_t size)
    {
        return read(rb, data, size);
    }

    size_t readData(char *data, size_t size)
    {
        return read(ob, data, size);
    }

    size_t readTill(char *data, size_t size, const char* delim)
    {
        return readTill(rb, data, size, delim);
    }

    void setTimeout(uint32_t _timeout)
    {
        timeout = _timeout;
    }

    int readable(void)
    {
        return !rb.empty();
    }

    int writeable(void)
    {
        return 1;   // buffer allows overwriting by design, always true
    }

    void setBaud(uint32_t _baud)
    {
        serial.baud(_baud);
    }


private:
    void bufferRx()
    {
        uint8_t data = serial.getc();
        ob.push(data);
        pushed++;
        oob.signal_set(0x2);
    }

    void checkOob()
    {
        while(true)
        {
            rtos::Thread::signal_wait(0x2);
            while(!ob.empty())
            {
                uint8_t data = ob.pop();
                auto specialSequence = specialSequences.begin();
                while(specialSequence != specialSequences.end())
                {
                    if(specialSequence->first.feed(data))
                    {
                        specialSequence->second();
                        break;
                    }
                    specialSequence++;
                }
                // character not needed by special sequences, passing it to command response buffer
                rb.push(data);
            }
        }
    }

    bool recveiveSequence(Buffer& source, const char* sequence)
    {
        bool res = false;

        SpecialSequence seq(sequence);
        Timer timer;
        timer.start();
        while(true)
        {
            int c = getc(source);
            while((c < 0) && (static_cast<uint32_t>(timer.read_ms()) < timeout))
            {
                c = getc(source);
            }

            if (c < 0)
            {
                break;
            }
            if(seq.feed(c))
            {
                res = true;
                break;
            }
        }
        return res;
    }

    size_t read(Buffer& source, char *data, size_t size)
    {
        size_t i = 0;
        Timer timer;
        timer.start();
        for( ; i < size; i++)
        {
            int c = getc(source);
            while((c < 0) && (static_cast<uint32_t>(timer.read_ms()) < 1000))
            {
                c = getc(source);
            }
            if(c < 0)
            {
                i = 0;
                break;
            }
            data[i] = c;
        }
        return i;
    }

    size_t readTill(Buffer& source, char *data, size_t size, const char* delim)
    {
        size_t i = 0;
        size_t delimLen = std::strlen(delim);
        Timer timer;
        timer.start();
        for( ; i < size; ++i)
        {
            int c = getc(source);
            while((c < 0) && (static_cast<uint32_t>(timer.read_ms()) < 1000))
            {
                c = getc(source);
            }
            if(c < 0)
            {
                return 0;
            }
            data[i] = c;
            if(i >= delimLen && std::strncmp(&data[i - delimLen + 1], delim, delimLen) == 0)
            {
                data[i] = '\0';
                break;
            }

        }
        return i;
    }

    int getc(Buffer& source)
    {
        if(!source.empty())
        {
            return source.pop();
        }
        else
        {
            return -1;
        }
    }

    bool vsend(const char *format, va_list args)
    {
        if(vsprintf(sendBuffer, format, args) < 0)
        {
            return false;
        }

        int i = 0;
        lock();
        for( ; sendBuffer[i]; i++)
        {
            if(serial.putc(sendBuffer[i]) < 0)
            {
                i = 0;
                break;
            }
        }
        unlock();
        return (i != 0);
    }

    void lock()
    {
        mutex.lock();
    }

    void unlock()
    {
        mutex.unlock();
    }

private:
    RawSerial serial;
    Thread oob;
    Buffer ob;
    Buffer rb;
    char sendBuffer[256];

    uint32_t timeout;
    volatile int pushed;
    PlatformMutex mutex;
    std::vector<std::pair<SpecialSequence, Callback<void()>>> specialSequences;
};
