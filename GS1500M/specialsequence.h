#pragma once

#include <string>

class SpecialSequence
{
public:
    SpecialSequence(const std::string& _sequence)
        : sequence(_sequence),
          len(sequence.size()),
          seqIdx(0)
        {}

    bool feed(uint8_t _newChar)
    {
        bool wholeMatched = false;
        if(sequence[seqIdx] == _newChar)
        {
            seqIdx++;
        }
        else
        {
            seqIdx = 0;
        }

        if(seqIdx == len)
        {
            wholeMatched = true;
            seqIdx = 0;
        }
        return wholeMatched;
    }

private:
    const std::string sequence;
    const size_t len;
    size_t seqIdx;

};
