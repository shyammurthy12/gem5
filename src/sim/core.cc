/*
 * Copyright (c) 2006 The Regents of The University of Michigan
 * Copyright (c) 2013 Advanced Micro Devices, Inc.
 * Copyright (c) 2013 Mark D. Hill and David A. Wood
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Nathan Binkert
 *          Steve Reinhardt
 */


#include "sim/core.hh"

#include <iostream>
#include <string>

#include "base/callback.hh"
#include "base/cprintf.hh"
#include "base/logging.hh"
#include "base/output.hh"
#include "cpu/o3/commit.hh"
#include "sim/eventq.hh"

using namespace std;

namespace SimClock {
/// The simulated frequency of curTick(). (In ticks per second)
Tick Frequency;

namespace Float {
double s;
double ms;
double us;
double ns;
double ps;

double Hz;
double kHz;
double MHz;
double GHz;
} // namespace Float

namespace Int {
Tick s;
Tick ms;
Tick us;
Tick ns;
Tick ps;
} // namespace Float

} // namespace SimClock

namespace {

bool _clockFrequencyFixed = false;

// Default to 1 THz (1 Tick == 1 ps)
Tick _ticksPerSecond = 1e12;

} // anonymous namespace

void
fixClockFrequency()
{
    if (_clockFrequencyFixed)
        return;

    using namespace SimClock;
    Frequency = _ticksPerSecond;
    Float::s = static_cast<double>(Frequency);
    Float::ms = Float::s / 1.0e3;
    Float::us = Float::s / 1.0e6;
    Float::ns = Float::s / 1.0e9;
    Float::ps = Float::s / 1.0e12;

    Float::Hz  = 1.0 / Float::s;
    Float::kHz = 1.0 / Float::ms;
    Float::MHz = 1.0 / Float::us;
    Float::GHz = 1.0 / Float::ns;

    Int::s  = Frequency;
    Int::ms = Int::s / 1000;
    Int::us = Int::ms / 1000;
    Int::ns = Int::us / 1000;
    Int::ps = Int::ns / 1000;

    cprintf("Global frequency set at %d ticks per second\n", _ticksPerSecond);

    _clockFrequencyFixed = true;
}
bool clockFrequencyFixed() { return _clockFrequencyFixed; }

void
setClockFrequency(Tick tps)
{
    panic_if(_clockFrequencyFixed,
            "Global frequency already fixed at %f ticks/s.", _ticksPerSecond);
    _ticksPerSecond = tps;
}
Tick getClockFrequency() { return _ticksPerSecond; }

void
setOutputDir(const string &dir)
{
    simout.setDirectory(dir);
}

/**
 * Queue of C++ callbacks to invoke on simulator exit.
 */
inline CallbackQueue &
exitCallbacks()
{
    static CallbackQueue theQueue;
    return theQueue;
}

/**
 * Register an exit callback.
 */
void
registerExitCallback(Callback *callback)
{
    exitCallbacks().add(callback);
}

/**
 * Do C++ simulator exit processing.  Exported to Python to be invoked
 * when simulator terminates via Python's atexit mechanism.
 */
void
doExitCleanup()
{
    printf("Hello, end of simulation\n");
   // uint64_t max_record_lifetime_across_all_entries = 0;
   // for (int i = 0;i<lifetimes_of_hash_entries.size();i++)
   // {
   //  if (hash_entries_used.at(i))
   //  {
   //    printf("We have %lu lifetime records for entry %d\n",
   //           lifetimes_of_hash_entries.at(i).size(),
   //             i);
   //    if (!lifetimes_of_hash_entries.at(i).back().subtraction_done){
   //      printf("The record for entry %d is still present\n",i);
   //      //lifetimes_of_hash_entries.at(i).back().lifetime =
   //      // curTick()-lifetimes_of_hash_entries.at(i).back().lifetime;
   //      lifetimes_of_hash_entries.at(i).back().lifetime =
   //       memRefCommits-lifetimes_of_hash_entries.at(i).back().lifetime;
   //    }
   //    int  max_subrecord;
   //    max_subrecord = 0;
   //    uint64_t max_subrecord_lifetime,max_epoch_id;
   //    max_subrecord_lifetime =
   //    lifetimes_of_hash_entries.at(i).at(0).lifetime;
   //    max_epoch_id = lifetimes_of_hash_entries.at(i).at(0).epoch_id;
   //    for (int j = 0;j<lifetimes_of_hash_entries.at(i).size();j++)
   //    {
   //       if (lifetimes_of_hash_entries.at(i).at(j).lifetime >
   //                       max_subrecord_lifetime)
   //       {
   //         max_subrecord_lifetime =
   //                 lifetimes_of_hash_entries.at(i).at(j).lifetime;
   //         max_epoch_id = lifetimes_of_hash_entries.at(i).at(j).epoch_id;
   //         max_subrecord = j;
   //       }

   //       printf("The lifetime for entry %d and sub-record %d is "
   //       "%lu and epoch-id is %lu\n",i,j,
   //       lifetimes_of_hash_entries.at(i).at(j).lifetime,
   //       lifetimes_of_hash_entries.at(i).at(j).epoch_id);
   //    }
   //    if (max_subrecord_lifetime > max_record_lifetime_across_all_entries)
   //         max_record_lifetime_across_all_entries = max_subrecord_lifetime;
   //    printf("The max-lifetime for entry %d and sub-record %d is "
   //    "%lu and epoch-id is %lu\n",i,max_subrecord,
   //    max_subrecord_lifetime,max_epoch_id);
   //  }
   // }
   // printf("The maximum lifetime across all entries is %lu\n",
   //                 max_record_lifetime_across_all_entries);
   // //printf("The value of the curTick is %lu\n",curTick());
   // printf("The number of instructions committed is %d\n",memRefCommits);
    exitCallbacks().process();
    exitCallbacks().clear();

    cout.flush();
}

