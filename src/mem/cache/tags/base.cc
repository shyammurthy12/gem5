/*
 * Copyright (c) 2013,2016,2018 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2003-2005 The Regents of The University of Michigan
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
 * Authors: Erik Hallnor
 *          Ron Dreslinski
 */

/**
 * @file
 * Definitions of BaseTags.
 */

#include "mem/cache/tags/base.hh"

#include <cassert>

#include "base/types.hh"
#include "mem/cache/replacement_policies/replaceable_entry.hh"
#include "mem/cache/tags/indexing_policies/base.hh"
#include "mem/ongal_VC.hh"
#include "mem/request.hh"
#include "mem/ruby/common/ASDT_entry.hh"
#include "sim/core.hh"
#include "sim/sim_exit.hh"
#include "sim/system.hh"

BaseTags::BaseTags(const Params *p)
    : ClockedObject(p), blkSize(p->block_size), blkMask(blkSize - 1),
      size(p->size), lookupLatency(p->tag_latency),
      system(p->system), indexingPolicy(p->indexing_policy),
      warmupBound((p->warmup_percentage/100.0) * (p->size / p->block_size)),
      warmedUp(false), numBlocks(p->size / p->block_size),
      dataBlks(new uint8_t[p->size]) // Allocate data storage in one big chunk
{
}

ReplaceableEntry*
BaseTags::findBlockBySetAndWay(int set, int way) const
{
    return indexingPolicy->getEntry(set, way);
}

CacheBlk*
BaseTags::findBlock(Addr addr, bool is_secure) const
{
#ifdef Ongal_VC
#ifdef VIVT
     if (get_VC_structure() != NULL){

       // No Corresponding ASDT entry?
       uint64_t Region_Size = get_VC_structure()->get_region_size();
       uint64_t PPN = addr/Region_Size;
       ASDT_entry * ASDT_entry =
               get_VC_structure()->access_matching_ASDT_map(PPN);
#ifdef Smurthy_debug
      printf("FindBlock: PPN is %lu\n",
                      PPN);
#endif

       if (ASDT_entry == NULL){
         return NULL;
       }else{
         Addr CPA_VPN   = ASDT_entry->get_virtual_page_number();
         Addr CPA_Vaddr = (CPA_VPN * Region_Size) + (addr % Region_Size);
         //uint32_t random_number_to_hash_with =
         //        ASDT_entry->get_random_number_to_hash_with();
         uint32_t constant_to_xor_with = 0;
         uint64_t CPA_CR3 = ASDT_entry->get_cr3();

         uint64_t index_into_hash_lookup_table = (CPA_VPN^CPA_CR3)&
                  (m_vc_structure->get_hash_lookup_table_size()-1);
#ifdef Smurthy_debug
         printf("Index into the hash lookup table(findBlock) is %ld\n",
                         index_into_hash_lookup_table);
#endif
         //if the entry in the hash lookup table is valid
         int temp = index_into_hash_lookup_table;
         if (get_VC_structure()->hash_entry_to_use_getValid(temp))
         {
           //obtain the hash entry to use.
           uint64_t hash_entry_to_use =
                   get_VC_structure()->get_hash_entry_to_use(temp);
           //the hashing function table is always assumed
           //to have a valid entry that can be used.
           int temp1 = hash_entry_to_use;
    constant_to_xor_with =
  get_VC_structure()->hashing_function_to_use_get_constant_to_xor_with(temp1);

         }
         //absence of a valid entry, indicates
         //a miss in the cache.
         else
         {
          return NULL;
         }
#ifdef Smurthy_debug
         printf("Find block with addr: %lx vtag: %lx, cr3: %lu, Paddr:"
                         "%lx\n",CPA_Vaddr,
                         extractTag(CPA_Vaddr),
                         CPA_CR3, addr);
#endif
        const std::vector<ReplaceableEntry*> entries =
indexingPolicy->getPossibleEntries_with_Vaddr(CPA_Vaddr,
                constant_to_xor_with);
      // only leading virtual address
      CacheBlk* target_block = NULL;
     // Search for block
     for (const auto& location : entries) {
         CacheBlk* blk = static_cast<CacheBlk*>(location);
         //if ((blk->vtag == extractTag(CPA_Vaddr))&& (CPA_CR3 == blk->cr3) &&
         //part of the temporary hack
         if ((blk->vtag == CPA_Vaddr/64)&& (CPA_CR3 == blk->cr3) &&
                 (blk->isValid()) &&
             (blk->isSecure() == is_secure)) {
             target_block =  blk;
         }
     }
     return target_block;

      }
    }
#endif
#endif


    // Extract block tag
    Addr tag = extractTag(addr);
  //the updated address is sent as a parameter to getPossibleEntries.

    // Find possible entries that may contain the given address
    const std::vector<ReplaceableEntry*> entries =
        indexingPolicy->getPossibleEntries(addr);

    // Search for block
    for (const auto& location : entries) {
        CacheBlk* blk = static_cast<CacheBlk*>(location);
        if ((blk->tag == tag) && blk->isValid() &&
            (blk->isSecure() == is_secure)) {
            return blk;
        }
    }

    // Did not find block
    return nullptr;
}

#ifdef Ongal_VC
CacheBlk*
BaseTags::findBlock_vaddr(Addr addr, Addr cr3, uint32_t
                random_number_to_xor_with) const
{

    // Extract block tag
    Addr tag = extractTag(addr);
    // Find possible entries that may contain the given address
    const std::vector<ReplaceableEntry*> entries =
        indexingPolicy->getPossibleEntries_with_Vaddr(addr,
                        random_number_to_xor_with);

    // Search for block
    for (const auto& location : entries) {
        CacheBlk* blk = static_cast<CacheBlk*>(location);
        if ((blk->tag == tag) && blk->isValid() && (blk->cr3 == cr3)) {
            return blk;
        }
    }

    // Did not find block
    return nullptr;

}


CacheBlk*
BaseTags::findBlock_with_vaddr(Addr addr, Addr cr3, bool is_secure) const
{

    // Extract block tag
    Addr tag = extractTag(addr);
    // Find possible entries that may contain the given address
    //This implementation is broken, but does not get used.
    //Change this later on, when we turn on LateMemTrap.
    const std::vector<ReplaceableEntry*> entries =
        indexingPolicy->getPossibleEntries_with_Vaddr(addr,0);

    // Search for block
    for (const auto& location : entries) {
        CacheBlk* blk = static_cast<CacheBlk*>(location);
        if ((blk->tag == tag) && blk->isValid() && (blk->cr3 == cr3)
                        &&(blk->isSecure() == is_secure)) {
            return blk;
        }
    }

    // Did not find block
    return nullptr;

}
#endif


void
BaseTags::insertBlock(const PacketPtr pkt, CacheBlk *blk)
{
    assert(!blk->isValid());


    // Previous block, if existed, has been removed, and now we have
    // to insert the new one

    // Deal with what we are bringing in
    MasterID master_id = pkt->req->masterId();
    assert(master_id < system->maxMasters());
    occupancies[master_id]++;

    // Insert block with tag, src master id and task id
    blk->insert(extractTag(pkt->getAddr()), pkt->isSecure(), master_id,
                pkt->req->taskId());

#ifdef Ongal_VC
         Addr addr = pkt->getAddr();
         blk->paddr = blkAlign(addr); // keep physical address for the line.

         if (get_VC_structure() != NULL){

           // Access ASDT and get correct ASDT and CR3
           uint64_t Region_Size = get_VC_structure()->get_region_size();
           uint64_t PPN = pkt->req->getPaddr()/Region_Size;
           uint64_t CPA_VPN = 0;
           uint64_t CPA_CR3 = 0;
           bool is_writable_page = false;
           ASDT_entry * ASDT_entry =
                   get_VC_structure()->access_matching_ASDT_map(PPN);

           if (ASDT_entry != NULL){
             CPA_VPN = ASDT_entry->get_virtual_page_number();
             CPA_CR3 = ASDT_entry->get_cr3();
             // obtain the current permission of the leading virtual page
             is_writable_page = ASDT_entry->get_is_writable_page();
           }else{
             std::cout<<"tags->insertblock(), should find a corresponding ASDT"
                     "entry in a map";
             abort();
           }
           // update blk->vtag
           uint64_t CPA_Vaddr = (CPA_VPN * Region_Size) + (pkt->req->getPaddr()
                           % Region_Size);
           //blk->vtag = extractTag(CPA_Vaddr);
           //Ongal:
           //temporary hack, to get this to work with
           //hashing.
           //what constitutes a tag with hashing, find out.
           blk->vtag = CPA_Vaddr/64;
           // update blk->cr3
           blk->cr3 = CPA_CR3;
           // store the write permission information
           blk->is_writable_page = is_writable_page;
          // printf("Inserting block with vtag %lx cr3:"
          //                 "%lu\n",blk->vtag,blk->cr3);
         }
#endif



    // Check if cache warm up is done
    if (!warmedUp && tagsInUse.value() >= warmupBound) {
        warmedUp = true;
        warmupCycle = curTick();
    }

    // We only need to write into one tag and one data block.
    tagAccesses += 1;
    dataAccesses += 1;
}
void
BaseTags::insertBlock_helper_for_VC(const
                PacketPtr pkt, CacheBlk* blk, uint64_t epoch_id)
{

}


Addr
BaseTags::extractTag(const Addr addr) const
{
    return indexingPolicy->extractTag(addr);
}

void
BaseTags::cleanupRefsVisitor(CacheBlk &blk)
{
    if (blk.isValid()) {
        totalRefs += blk.refCount;
        ++sampledRefs;
    }
}

void
BaseTags::cleanupRefs()
{
    forEachBlk([this](CacheBlk &blk) { cleanupRefsVisitor(blk); });
}

void
BaseTags::computeStatsVisitor(CacheBlk &blk)
{
    if (blk.isValid()) {
        assert(blk.task_id < ContextSwitchTaskId::NumTaskId);
        occupanciesTaskId[blk.task_id]++;
        assert(blk.tickInserted <= curTick());
        Tick age = curTick() - blk.tickInserted;

        int age_index;
        if (age / SimClock::Int::us < 10) { // <10us
            age_index = 0;
        } else if (age / SimClock::Int::us < 100) { // <100us
            age_index = 1;
        } else if (age / SimClock::Int::ms < 1) { // <1ms
            age_index = 2;
        } else if (age / SimClock::Int::ms < 10) { // <10ms
            age_index = 3;
        } else
            age_index = 4; // >10ms

        ageTaskId[blk.task_id][age_index]++;
    }
}

void
BaseTags::computeStats()
{
    for (unsigned i = 0; i < ContextSwitchTaskId::NumTaskId; ++i) {
        occupanciesTaskId[i] = 0;
        for (unsigned j = 0; j < 5; ++j) {
            ageTaskId[i][j] = 0;
        }
    }

    forEachBlk([this](CacheBlk &blk) { computeStatsVisitor(blk); });
}

std::string
BaseTags::print()
{
    std::string str;

    auto print_blk = [&str](CacheBlk &blk) {
        if (blk.isValid())
            str += csprintf("\tBlock: %s\n", blk.print());
    };
    forEachBlk(print_blk);

    if (str.empty())
        str = "no valid tags\n";

    return str;
}

void
BaseTags::regStats()
{
    ClockedObject::regStats();

    using namespace Stats;

    tagsInUse
        .name(name() + ".tagsinuse")
        .desc("Cycle average of tags in use")
        ;

    totalRefs
        .name(name() + ".total_refs")
        .desc("Total number of references to valid blocks.")
        ;

    sampledRefs
        .name(name() + ".sampled_refs")
        .desc("Sample count of references to valid blocks.")
        ;

    avgRefs
        .name(name() + ".avg_refs")
        .desc("Average number of references to valid blocks.")
        ;

    avgRefs = totalRefs/sampledRefs;

    warmupCycle
        .name(name() + ".warmup_cycle")
        .desc("Cycle when the warmup percentage was hit.")
        ;

    occupancies
        .init(system->maxMasters())
        .name(name() + ".occ_blocks")
        .desc("Average occupied blocks per requestor")
        .flags(nozero | nonan)
        ;
    for (int i = 0; i < system->maxMasters(); i++) {
        occupancies.subname(i, system->getMasterName(i));
    }

    avgOccs
        .name(name() + ".occ_percent")
        .desc("Average percentage of cache occupancy")
        .flags(nozero | total)
        ;
    for (int i = 0; i < system->maxMasters(); i++) {
        avgOccs.subname(i, system->getMasterName(i));
    }

    avgOccs = occupancies / Stats::constant(numBlocks);

    occupanciesTaskId
        .init(ContextSwitchTaskId::NumTaskId)
        .name(name() + ".occ_task_id_blocks")
        .desc("Occupied blocks per task id")
        .flags(nozero | nonan)
        ;

    ageTaskId
        .init(ContextSwitchTaskId::NumTaskId, 5)
        .name(name() + ".age_task_id_blocks")
        .desc("Occupied blocks per task id")
        .flags(nozero | nonan)
        ;

    percentOccsTaskId
        .name(name() + ".occ_task_id_percent")
        .desc("Percentage of cache occupancy per task id")
        .flags(nozero)
        ;

    percentOccsTaskId = occupanciesTaskId / Stats::constant(numBlocks);

    tagAccesses
        .name(name() + ".tag_accesses")
        .desc("Number of tag accesses")
        ;

    dataAccesses
        .name(name() + ".data_accesses")
        .desc("Number of data accesses")
        ;

    registerDumpCallback(new BaseTagsDumpCallback(this));
    registerExitCallback(new BaseTagsCallback(this));
}
