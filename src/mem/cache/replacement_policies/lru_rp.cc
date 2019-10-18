/**
 * Copyright (c) 2018 Inria
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
 * Authors: Daniel Carvalho
 */

#include "mem/cache/replacement_policies/lru_rp.hh"

#include <cassert>
#include <memory>

#include "params/LRURP.hh"

LRURP::LRURP(const Params *p)
    : BaseReplacementPolicy(p)
{
}

void
LRURP::invalidate(const std::shared_ptr<ReplacementData>& replacement_data)
const
{
    // Reset last touch timestamp
    std::static_pointer_cast<LRUReplData>(
        replacement_data)->lastTouchTick = Tick(0);
}

void
LRURP::touch(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    // Update last touch timestamp
    std::static_pointer_cast<LRUReplData>(
        replacement_data)->lastTouchTick = curTick();
}

void
LRURP::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    // Set last touch timestamp
    std::static_pointer_cast<LRUReplData>(
        replacement_data)->lastTouchTick = curTick();
}

void
LRURP::reset_helper(const std::shared_ptr<ReplacementData>& replacement_data,
                    uint64_t epoch_id,
                    uint64_t threshold_after_which_epoch_id_invalid) const
{

    // Set epoch id
    std::static_pointer_cast<LRUReplData>(
        replacement_data)->epoch_id = epoch_id;
    //set the tick when the block was inserted.
    std::static_pointer_cast<LRUReplData>(
        replacement_data)->tickInserted = curTick();
    std::static_pointer_cast<LRUReplData>(
        replacement_data)->threshold_after_which_epoch_id_invalid
               = threshold_after_which_epoch_id_invalid;
    reset(replacement_data);
}



ReplaceableEntry*
LRURP::getVictim(const ReplacementCandidates& candidates) const
{
    // There must be at least one replacement candidate
    assert(candidates.size() > 0);

    // Visit all candidates to find victim
    ReplaceableEntry* victim = candidates[0];
    for (const auto& candidate : candidates) {
        // Update victim entry if necessary
        if (std::static_pointer_cast<LRUReplData>(
                    candidate->replacementData)->lastTouchTick <
                std::static_pointer_cast<LRUReplData>(
                    victim->replacementData)->lastTouchTick) {
            victim = candidate;
        }
    }

    return victim;
}

ReplaceableEntry*
LRURP::getVictim_epoch_considered(const
                ReplacementCandidates& candidates) const
{
    // There must be at least one replacement candidate
    assert(candidates.size() > 0);
    //check if all the epochIDs are valid.
    bool epoch_valid = true;
    Tick tickInserted_temp;
    uint64_t threshold_for_validity;
    //this threshold is the same for all candidates.
    threshold_for_validity = std::static_pointer_cast<LRUReplData>(
      candidates[0]->replacementData)->threshold_after_which_epoch_id_invalid;
    for (const auto& candidate : candidates) {
       tickInserted_temp =
               std::static_pointer_cast<LRUReplData>(
               candidate->replacementData)->tickInserted;

       if ((curTick()-tickInserted_temp)> threshold_for_validity){
          epoch_valid = false;
          break;
       }
    }
    //if any of the epochID's is invalid, then we want the block is kicked out.
    //We turn to the FIFO replacement policy in that case.
    //Else, if all have epochIDs to be valid, we use the epochID plus LRU
    //replacement
    //policy.
    // Visit all candidates to find victim
    ReplaceableEntry* victim = candidates[0];
    if (epoch_valid){
       for (const auto& candidate : candidates) {
           // Update victim entry if necessary

          if (std::static_pointer_cast<LRUReplData>(
                       candidate->replacementData)->epoch_id <
                   std::static_pointer_cast<LRUReplData>(
                       victim->replacementData)->epoch_id) {
               victim = candidate;
           }
          //if epoch id's match, default to the normal LRU
          else if (std::static_pointer_cast<LRUReplData>(
                       candidate->replacementData)->epoch_id ==
                   std::static_pointer_cast<LRUReplData>(
                       victim->replacementData)->epoch_id) {
                if (std::static_pointer_cast<LRUReplData>(
                            candidate->replacementData)->lastTouchTick <
                        std::static_pointer_cast<LRUReplData>(
                            victim->replacementData)->lastTouchTick) {
                    victim = candidate;
                }
           }
       }
    }
    //if epoch id has gone invalid for any of the blocks,
    //then we need to kick it out,
    //resort to FIFO for the same.
    else{
        // Visit all candidates to find victim
        for (const auto& candidate : candidates) {
            // Update victim entry if necessary
            if (std::static_pointer_cast<LRUReplData>(
                        candidate->replacementData)->tickInserted <
                    std::static_pointer_cast<LRUReplData>(
                        victim->replacementData)->tickInserted) {
                victim = candidate;
            }
        }

    }

    return victim;
}




std::shared_ptr<ReplacementData>
LRURP::instantiateEntry()
{
    return std::shared_ptr<ReplacementData>(new LRUReplData());
}

LRURP*
LRURPParams::create()
{
    return new LRURP(this);
}
