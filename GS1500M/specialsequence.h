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
