/*
 * Copyright (c) 2010-2019 ARM Limited
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
 * Copyright (c) 2002-2005 The Regents of The University of Michigan
 * Copyright (c) 2010,2015 Advanced Micro Devices, Inc.
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
 *          Dave Greene
 *          Nathan Binkert
 *          Steve Reinhardt
 *          Ron Dreslinski
 *          Andreas Sandberg
 *          Nikos Nikoleris
 */

/**
 * @file
 * Cache definitions.
 */

#include "mem/cache/cache.hh"

#include <cassert>

#include "base/compiler.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "base/types.hh"
#include "debug/Cache.hh"
#include "debug/CacheTags.hh"
#include "debug/CacheVerbose.hh"
#include "enums/Clusivity.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/mshr.hh"
#include "mem/cache/tags/base.hh"
#include "mem/cache/write_queue_entry.hh"
#include "mem/request.hh"
#include "params/Cache.hh"

Cache::Cache(const CacheParams *p)
    : BaseCache(p, p->system->cacheLineSize()),
      doFastWrites(true)
{
    //Ongal
    unsigned numSets = p->size / (p->assoc * system->cacheLineSize());
#ifdef Ongal_VC

    m_cache_num_sets = numSets;
    m_cache_assoc = p->assoc;

   // if ( p->name.find("dcache") != string::npos || p->name.find("icache") !=
   //                 string::npos ){

    if ( p->name.find("dcache") != string::npos){

      set_is_l1cache(true);
      std::cout<<" L1Cache is found ";

      prev_cr3 = 0;
      m_region_size = REGION_SIZE;
      m_num_accesses = 0;
      m_vc_structure = 	new VC_structure(p->name,
                                         m_region_size,
                                         system->cacheLineSize(),
                                         32,
                                         256,
                                         numSets,
                                         p->assoc);

      // after creating VC structure
      tags->set_VC_structure(m_vc_structure);

      // profiling variables
      num_active_synonym_access = 0;
      num_active_synonym_access_with_non_CPA = 0;

      num_active_synonym_access_store = 0;
      num_active_synonym_access_with_non_CPA_store = 0;
      num_active_synonym_access_with_non_CPA_store_ART_miss = 0;

      num_invalidation_from_lower_level_to_region_with_active_synonym = 0;

      num_LCPA_saving_consecutive_active_synonym_access_in_a_page = 0;
      num_LCPA_saving_ART_hits = 0;

      num_ss_hits = 0;
      num_ss_64KB_hits = 0;
      num_ss_1MB_hits = 0;

      num_art_hits = 0;
      num_kernel_space_access = 0;
      num_kernel_space_access_with_active_synonym = 0;

      num_samples = 0;
      num_valid_asdt_entry = 0;
      num_max_num_valid_asdt_entry = 0;
      num_valid_asdt_entry_with_active_synonym = 0;
      num_virtual_pages_with_active_synonym    = 0;

      num_valid_asdt_entry_one_line = 0;
      num_valid_asdt_entry_two_lines = 0;

      num_asdt_entry_conflict_miss = 0;
      num_evicted_lines_per_asdt_conflict_miss = 0;

      num_other_approach_access = 0;
      num_CPA_change = 0;
      num_CPA_change_check = 0;

      num_OVC_hits = 0;
      num_TVC_hits = 0;

      num_SLB_traps_8 = 0;
      num_SLB_traps_16 = 0;
      num_SLB_traps_32 = 0;
      num_SLB_traps_48 = 0;

      num_SLB_Lookup = 0;

      num_page_info_change = 0;

    } else {
      set_is_l1cache(false);
      std::cout<<" This cache is NOT L1Cache ";

      m_vc_structure = NULL;
      tags->set_VC_structure(m_vc_structure);

    }

    std::cout<<std::endl;

#endif

}

void
Cache::satisfyRequest(PacketPtr pkt, CacheBlk *blk,
                      bool deferred_response, bool pending_downgrade)
{
    BaseCache::satisfyRequest(pkt, blk);

    if (pkt->isRead()) {
        // determine if this read is from a (coherent) cache or not
        if (pkt->fromCache()) {
            assert(pkt->getSize() == blkSize);
            // special handling for coherent block requests from
            // upper-level caches
            if (pkt->needsWritable()) {
                // sanity check
                assert(pkt->cmd == MemCmd::ReadExReq ||
                       pkt->cmd == MemCmd::SCUpgradeFailReq);
                assert(!pkt->hasSharers());

                // if we have a dirty copy, make sure the recipient
                // keeps it marked dirty (in the modified state)
                if (blk->isDirty()) {
                    pkt->setCacheResponding();
                    blk->status &= ~BlkDirty;
                }
            } else if (blk->isWritable() && !pending_downgrade &&
                       !pkt->hasSharers() &&
                       pkt->cmd != MemCmd::ReadCleanReq) {
                // we can give the requester a writable copy on a read
                // request if:
                // - we have a writable copy at this level (& below)
                // - we don't have a pending snoop from below
                //   signaling another read request
                // - no other cache above has a copy (otherwise it
                //   would have set hasSharers flag when
                //   snooping the packet)
                // - the read has explicitly asked for a clean
                //   copy of the line
                if (blk->isDirty()) {
                    // special considerations if we're owner:
                    if (!deferred_response) {
                        // respond with the line in Modified state
                        // (cacheResponding set, hasSharers not set)
                        pkt->setCacheResponding();

                        // if this cache is mostly inclusive, we
                        // keep the block in the Exclusive state,
                        // and pass it upwards as Modified
                        // (writable and dirty), hence we have
                        // multiple caches, all on the same path
                        // towards memory, all considering the
                        // same block writable, but only one
                        // considering it Modified

                        // we get away with multiple caches (on
                        // the same path to memory) considering
                        // the block writeable as we always enter
                        // the cache hierarchy through a cache,
                        // and first snoop upwards in all other
                        // branches
                        blk->status &= ~BlkDirty;
                    } else {
                        // if we're responding after our own miss,
                        // there's a window where the recipient didn't
                        // know it was getting ownership and may not
                        // have responded to snoops correctly, so we
                        // have to respond with a shared line
                        pkt->setHasSharers();
                    }
                }
            } else {
                // otherwise only respond with a shared copy
                pkt->setHasSharers();
            }
        }
    }
}


#ifdef Ongal_VC
#ifdef LateMemTrap
bool
Cache::satisfyCpuSideRequest_VC_Store(PacketPtr pkt, CacheBlk *blk,
                           bool deferred_response, bool pending_downgrade)
{
    assert(pkt->isRequest());
    assert(blk && blk->isValid());
    assert(pkt->getOffset(blkSize) + pkt->getSize() <= blkSize);

    // Check RMW operations first since both isRead() and
    // isWrite() will be true for them
    if (pkt->cmd == MemCmd::SwapReq) {

        cmpAndSwap(blk, pkt);
        return true; // L1 VC Hit
    } else if (pkt->isWrite() && (!pkt->isWriteInvalidate() || isTopLevel)) {

        assert(blk->isWritable());
        // Write or WriteInvalidate at the first cache with block in Exclusive
        if (blk->checkWrite(pkt)) {
            pkt->writeDataToBlock(blk->data, blkSize);
        }

        // Always mark the line as dirty even if we are a failed
        // StoreCond so we supply data to any snoops that have
        // appended themselves to this cache before knowing the store
        // will fail.
        blk->status |= BlkDirty;
        return true; // L1 VC Hit
    }
    return false; // L1 VC Miss
}
#endif
#endif




/////////////////////////////////////////////////////
//
// Access path: requests coming in from the CPU side
//
/////////////////////////////////////////////////////


#ifdef Ongal_VC
#ifdef LateMemTrap
bool
Cache::access_virtual_cache(PacketPtr pkt, CacheBlk *&blk, Cycles &lat,
                            PacketList &writebacks)
{
    // sanity check
    assert(pkt->isRequest());

    if (pkt->req->isUncacheable()) {
        DPRINTF(Cache, "%s%s addr %#llx uncacheable\n", pkt->cmdString(),
                pkt->req->isInstFetch() ? " (ifetch)" : "",
                pkt->getAddr());

        return false;
    }

    int id = pkt->req->hasContextId() ? pkt->req->contextId() : -1;
    blk = tags->access_VC_Block(pkt, pkt->isSecure(), lat, id);

    // Writeback handling is special case.  We can write the block into
    // the cache without having a writeable copy (or any copy at all).
    if (pkt->cmd == MemCmd::Writeback) {
        assert(blkSize == pkt->getSize());
        // need to considered as a miss for L1 VC access
        return false;
    } else if ((blk != NULL) &&
               (pkt->needsExclusive() ? blk->isWritable(): blk->isReadable()))
    {
        // OK to satisfy access
        if (!pkt->isWrite()){
          // ifetch and load
          satisfyCpuSideRequest(pkt, blk);
          return true;
        }else if (blk->is_writable_page){
          // store
          return satisfyCpuSideRequest_VC_Store(pkt, blk);
        }
    }

    // Can't satisfy access normally... either no block (blk == NULL)
    // or have block but need exclusive & only have shared.
    if (blk == NULL && pkt->isLLSC() && pkt->isWrite()) {
      // complete miss on store conditional... just give up now

      pkt->req->setExtraData(0);
      return true;
    }
    return false;
}
#endif
#endif


bool Cache::BaseCache_access_dup(PacketPtr pkt, CacheBlk *&blk, Cycles &lat,
                  PacketList &writebacks)
{
    // sanity check
    assert(pkt->isRequest());

    chatty_assert(!(isReadOnly && pkt->isWrite()),
                  "Should never see a write in a read-only cache %s\n",
                  name());

    // Access block in the tags
    Cycles tag_latency(0);
    blk = tags->accessBlock(pkt->getAddr(), pkt->isSecure(), tag_latency);

    //smurthy
//    if (pkt->isPacketFromLSQ)
//        DPRINTF(Cache,"Packet come from LSQ\n");
//    if (pkt->isPacketFromSpeculativeLoad)
//	printf("Hello, I am a speculative cache access\n");

    //smurthy
    //so that we know the request sent out hit or missed in the
    //L1 cache.
    if (get_is_l1cache() ){
     //printf("I am in L1 cache access\n");
     if (blk)
        pkt->req->isRequestHitInL1Cache = true;
      else
        pkt->req->isRequestMissInL1Cache = true;
    }
    DPRINTF(Cache, "%s for %s %s\n", __func__, pkt->print(),
            blk ? "hit " + blk->print() : "miss");

    if (pkt->req->isCacheMaintenance()) {
        // A cache maintenance operation is always forwarded to the
        // memory below even if the block is found in dirty state.

        // We defer any changes to the state of the block until we
        // create and mark as in service the mshr for the downstream
        // packet.

        // Calculate access latency on top of when the packet arrives. This
        // takes into account the bus delay.
        lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);

        return false;
    }

    if (pkt->isEviction()) {
        // We check for presence of block in above caches before issuing
        // Writeback or CleanEvict to write buffer. Therefore the only
        // possible cases can be of a CleanEvict packet coming from above
        // encountering a Writeback generated in this cache peer cache and
        // waiting in the write buffer. Cases of upper level peer caches
        // generating CleanEvict and Writeback or simply CleanEvict and
        // CleanEvict almost simultaneously will be caught by snoops sent out
        // by crossbar.
        WriteQueueEntry *wb_entry = writeBuffer.findMatch(pkt->getAddr(),
                                                          pkt->isSecure());
        if (wb_entry) {
            assert(wb_entry->getNumTargets() == 1);
            PacketPtr wbPkt = wb_entry->getTarget()->pkt;
            assert(wbPkt->isWriteback());

            if (pkt->isCleanEviction()) {
                // The CleanEvict and WritebackClean snoops into other
                // peer caches of the same level while traversing the
                // crossbar. If a copy of the block is found, the
                // packet is deleted in the crossbar. Hence, none of
                // the other upper level caches connected to this
                // cache have the block, so we can clear the
                // BLOCK_CACHED flag in the Writeback if set and
                // discard the CleanEvict by returning true.
                wbPkt->clearBlockCached();

                // A clean evict does not need to access the data array
                lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);

                return true;
            } else {
                assert(pkt->cmd == MemCmd::WritebackDirty);
                // Dirty writeback from above trumps our clean
                // writeback... discard here
                // Note: markInService will remove entry from writeback buffer.
                markInService(wb_entry);
                delete wbPkt;
            }
        }
    }

    // Writeback handling is special case.  We can write the block into
    // the cache without having a writeable copy (or any copy at all).
    if (pkt->isWriteback()) {
        assert(blkSize == pkt->getSize());

        // we could get a clean writeback while we are having
        // outstanding accesses to a block, do the simple thing for
        // now and drop the clean writeback so that we do not upset
        // any ordering/decisions about ownership already taken
        if (pkt->cmd == MemCmd::WritebackClean &&
            mshrQueue.findMatch(pkt->getAddr(), pkt->isSecure())) {
            DPRINTF(Cache, "Clean writeback %#llx to block with MSHR, "
                    "dropping\n", pkt->getAddr());

            // A writeback searches for the block, then writes the data.
            // As the writeback is being dropped, the data is not touched,
            // and we just had to wait for the time to find a match in the
            // MSHR. As of now assume a mshr queue search takes as long as
            // a tag lookup for simplicity.
            lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);

            return true;
        }

        if (!blk) {

            //if we miss in the cache, need
            //to make an allocation in the ASDT, if necessary.
            #ifdef Ongal_VC
            #ifdef ASDT_Set_Associative_Array
            ASDT_Invalidation_Check(pkt->getAddr(), writebacks);
            #endif
            Add_NEW_ASDT_map_entry(pkt);
            #endif

            // need to do a replacement
            blk = allocateBlock(pkt, writebacks);
            if (!blk) {


                // no replaceable block available: give up, fwd to next level.
                incMissCount(pkt);

                // A writeback searches for the block, then writes the data.
                // As the block could not be found, it was a tag-only access.
                lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);

                return false;
            }

          #ifdef Ongal_VC
            if (get_VC_Structure_ptr() != NULL){
              // L1 VC, check L1 line eviction (replacement, invalidations)
              // in regions with active synonym
              bool entry_with_synonym =
                 get_VC_Structure_ptr()->entry_with_active_synonym(blk->paddr);
          if (entry_with_synonym)
            ++num_invalidation_from_lower_level_to_region_with_active_synonym;
            }
          #endif
            blk->status |= BlkReadable;
        }
        // only mark the block dirty if we got a writeback command,
        // and leave it as is for a clean writeback
        if (pkt->cmd == MemCmd::WritebackDirty) {
            // TODO: the coherent cache can assert(!blk->isDirty());
            blk->status |= BlkDirty;
        }
        // if the packet does not have sharers, it is passing
        // writable, and we got the writeback in Modified or Exclusive
        // state, if not we are in the Owned or Shared state
        if (!pkt->hasSharers()) {
            blk->status |= BlkWritable;
        }
        // nothing else to do; writeback doesn't expect response
        assert(!pkt->needsResponse());
        pkt->writeDataToBlock(blk->data, blkSize);
        DPRINTF(Cache, "%s new state is %s\n", __func__, blk->print());
        incHitCount(pkt);

        // A writeback searches for the block, then writes the data
        lat = calculateAccessLatency(blk, pkt->headerDelay, tag_latency);

        // When the packet metadata arrives, the tag lookup will be done while
        // the payload is arriving. Then the block will be ready to access as
        // soon as the fill is done
        blk->setWhenReady(clockEdge(fillLatency) + pkt->headerDelay +
            std::max(cyclesToTicks(tag_latency), (uint64_t)pkt->payloadDelay));

        return true;
    } else if (pkt->cmd == MemCmd::CleanEvict) {
        // A CleanEvict does not need to access the data array
        lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);

        if (blk) {
            // Found the block in the tags, need to stop CleanEvict from
            // propagating further down the hierarchy. Returning true will
            // treat the CleanEvict like a satisfied write request and delete
            // it.
            return true;
        }
        // We didn't find the block here, propagate the CleanEvict further
        // down the memory hierarchy. Returning false will treat the CleanEvict
        // like a Writeback which could not find a replaceable block so has to
        // go to next level.
        return false;
    } else if (pkt->cmd == MemCmd::WriteClean) {
        // WriteClean handling is a special case. We can allocate a
        // block directly if it doesn't exist and we can update the
        // block immediately. The WriteClean transfers the ownership
        // of the block as well.
        assert(blkSize == pkt->getSize());

        if (!blk) {
            if (pkt->writeThrough()) {
                // A writeback searches for the block, then writes the data.
                // As the block could not be found, it was a tag-only access.
                lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);

                // if this is a write through packet, we don't try to
                // allocate if the block is not present
                return false;
            } else {
                // a writeback that misses needs to allocate a new block
                blk = allocateBlock(pkt, writebacks);
                if (!blk) {
                    // no replaceable block available: give up, fwd to
                    // next level.
                    incMissCount(pkt);

                    // A writeback searches for the block, then writes the
                    // data. As the block could not be found, it was a tag-only
                    // access.
                    lat = calculateTagOnlyLatency(pkt->headerDelay,
                                                  tag_latency);

                    return false;
                }

                blk->status |= BlkReadable;
            }
        }

        // at this point either this is a writeback or a write-through
        // write clean operation and the block is already in this
        // cache, we need to update the data and the block flags
        assert(blk);
        // TODO: the coherent cache can assert(!blk->isDirty());
        if (!pkt->writeThrough()) {
            blk->status |= BlkDirty;
        }
        // nothing else to do; writeback doesn't expect response
        assert(!pkt->needsResponse());
        pkt->writeDataToBlock(blk->data, blkSize);
        DPRINTF(Cache, "%s new state is %s\n", __func__, blk->print());

        incHitCount(pkt);

        // A writeback searches for the block, then writes the data
        lat = calculateAccessLatency(blk, pkt->headerDelay, tag_latency);

        // When the packet metadata arrives, the tag lookup will be done while
        // the payload is arriving. Then the block will be ready to access as
        // soon as the fill is done
        blk->setWhenReady(clockEdge(fillLatency) + pkt->headerDelay +
            std::max(cyclesToTicks(tag_latency), (uint64_t)pkt->payloadDelay));

        // if this a write-through packet it will be sent to cache
        // below
        return !pkt->writeThrough();
    } else if (blk && (pkt->needsWritable() ? blk->isWritable() :
                       blk->isReadable())) {
        // OK to satisfy access
        incHitCount(pkt);

        // Calculate access latency based on the need to access the data array
        if (pkt->isRead() || pkt->isWrite()) {
            lat = calculateAccessLatency(blk, pkt->headerDelay, tag_latency);
        } else {
            lat = calculateTagOnlyLatency(pkt->headerDelay, tag_latency);
        }

        satisfyRequest(pkt, blk);
        maintainClusivity(pkt->fromCache(), blk);

        return true;
    }

    // Can't satisfy access normally... either no block (blk == nullptr)
    // or have block but need writable

    incMissCount(pkt);

    lat = calculateAccessLatency(blk, pkt->headerDelay, tag_latency);

    if (!blk && pkt->isLLSC() && pkt->isWrite()) {
        // complete miss on store conditional... just give up now
        pkt->req->setExtraData(0);
        return true;
    }

    return false;
}


bool
Cache::access(PacketPtr pkt, CacheBlk *&blk, Cycles &lat,
              PacketList &writebacks)
{

    if (pkt->req->isUncacheable()) {
        assert(pkt->isRequest());

        chatty_assert(!(isReadOnly && pkt->isWrite()),
                      "Should never see a write in a read-only cache %s\n",
                      name());

        DPRINTF(Cache, "%s for %s\n", __func__, pkt->print());

        // flush and invalidate any existing block
        CacheBlk *old_blk(tags->findBlock(pkt->getAddr(), pkt->isSecure()));
        if (old_blk && old_blk->isValid()) {
            BaseCache::evictBlock(old_blk, writebacks);
        }

        blk = nullptr;
        // lookupLatency is the latency in case the request is uncacheable.
        lat = lookupLatency;
        return false;
    }

#ifdef Ongal_VC
#ifdef VIVT
    std::set<uint64_t> demaps = pkt->req->get_demap_pages();

    if ( demaps.size() > 0 ){

      for ( std::set<uint64_t>::iterator it = demaps.begin(); it !=
                      demaps.end() ; ++it ){
        // demap handler
        if ( *it == UNMAP_REQ ){
          unmap_vmas_Handler(writebacks);
        }else{
          Demap_ASDT_Handler((*it),writebacks);
        }
      }
    }
#endif
    Lookup_VCs(pkt->req->getVaddr(), pkt->req->getPaddr(), pkt->req->getCR3(),
                    pkt);
    return BaseCache_access_dup(pkt, blk, lat, writebacks);
#else
    return BaseCache_access_dup(pkt, blk, lat, writebacks);
#endif


}

void
Cache::doWritebacks(PacketList& writebacks, Tick forward_time)
{
    while (!writebacks.empty()) {
        PacketPtr wbPkt = writebacks.front();
        // We use forwardLatency here because we are copying writebacks to
        // write buffer.

        // Call isCachedAbove for Writebacks, CleanEvicts and
        // WriteCleans to discover if the block is cached above.
        if (isCachedAbove(wbPkt)) {
            if (wbPkt->cmd == MemCmd::CleanEvict) {
                // Delete CleanEvict because cached copies exist above. The
                // packet destructor will delete the request object because
                // this is a non-snoop request packet which does not require a
                // response.
                delete wbPkt;
            } else if (wbPkt->cmd == MemCmd::WritebackClean) {
                // clean writeback, do not send since the block is
                // still cached above
                assert(writebackClean);
                delete wbPkt;
            } else {
                assert(wbPkt->cmd == MemCmd::WritebackDirty ||
                       wbPkt->cmd == MemCmd::WriteClean);
                // Set BLOCK_CACHED flag in Writeback and send below, so that
                // the Writeback does not reset the bit corresponding to this
                // address in the snoop filter below.
                wbPkt->setBlockCached();
                allocateWriteBuffer(wbPkt, forward_time);
            }
        } else {
            // If the block is not cached above, send packet below. Both
            // CleanEvict and Writeback with BLOCK_CACHED flag cleared will
            // reset the bit corresponding to this address in the snoop filter
            // below.
            allocateWriteBuffer(wbPkt, forward_time);
        }
        writebacks.pop_front();
    }
}

void
Cache::doWritebacksAtomic(PacketList& writebacks)
{
    while (!writebacks.empty()) {
        PacketPtr wbPkt = writebacks.front();
        // Call isCachedAbove for both Writebacks and CleanEvicts. If
        // isCachedAbove returns true we set BLOCK_CACHED flag in Writebacks
        // and discard CleanEvicts.
        if (isCachedAbove(wbPkt, false)) {
            if (wbPkt->cmd == MemCmd::WritebackDirty ||
                wbPkt->cmd == MemCmd::WriteClean) {
                // Set BLOCK_CACHED flag in Writeback and send below,
                // so that the Writeback does not reset the bit
                // corresponding to this address in the snoop filter
                // below. We can discard CleanEvicts because cached
                // copies exist above. Atomic mode isCachedAbove
                // modifies packet to set BLOCK_CACHED flag
                memSidePort.sendAtomic(wbPkt);
            }
        } else {
            // If the block is not cached above, send packet below. Both
            // CleanEvict and Writeback with BLOCK_CACHED flag cleared will
            // reset the bit corresponding to this address in the snoop filter
            // below.
            memSidePort.sendAtomic(wbPkt);
        }
        writebacks.pop_front();
        // In case of CleanEvicts, the packet destructor will delete the
        // request object because this is a non-snoop request packet which
        // does not require a response.
        delete wbPkt;
    }
}


void
Cache::recvTimingSnoopResp(PacketPtr pkt)
{
    DPRINTF(Cache, "%s for %s\n", __func__, pkt->print());

    // determine if the response is from a snoop request we created
    // (in which case it should be in the outstandingSnoop), or if we
    // merely forwarded someone else's snoop request
    const bool forwardAsSnoop = outstandingSnoop.find(pkt->req) ==
        outstandingSnoop.end();

    if (!forwardAsSnoop) {
        // the packet came from this cache, so sink it here and do not
        // forward it
        assert(pkt->cmd == MemCmd::HardPFResp);

        outstandingSnoop.erase(pkt->req);

        DPRINTF(Cache, "Got prefetch response from above for addr "
                "%#llx (%s)\n", pkt->getAddr(), pkt->isSecure() ? "s" : "ns");
        recvTimingResp(pkt);
        return;
    }

    // forwardLatency is set here because there is a response from an
    // upper level cache.
    // To pay the delay that occurs if the packet comes from the bus,
    // we charge also headerDelay.
    Tick snoop_resp_time = clockEdge(forwardLatency) + pkt->headerDelay;
    // Reset the timing of the packet.
    pkt->headerDelay = pkt->payloadDelay = 0;
    memSidePort.schedTimingSnoopResp(pkt, snoop_resp_time);
}

void
Cache::promoteWholeLineWrites(PacketPtr pkt)
{
    // Cache line clearing instructions
    if (doFastWrites && (pkt->cmd == MemCmd::WriteReq) &&
        (pkt->getSize() == blkSize) && (pkt->getOffset(blkSize) == 0)) {
        pkt->cmd = MemCmd::WriteLineReq;
        DPRINTF(Cache, "packet promoted from Write to WriteLineReq\n");
    }
}

void
Cache::handleTimingReqHit(PacketPtr pkt, CacheBlk *blk, Tick request_time)
{
    // should never be satisfying an uncacheable access as we
    // flush and invalidate any existing block as part of the
    // lookup
    assert(!pkt->req->isUncacheable());

    BaseCache::handleTimingReqHit(pkt, blk, request_time);
}


CacheBlk*
Cache::handleFill(PacketPtr pkt, CacheBlk *blk, PacketList &writebacks,
                      bool allocate)
{
    assert(pkt->isResponse());
    Addr addr = pkt->getAddr();
    bool is_secure = pkt->isSecure();
#if TRACING_ON
    CacheBlk::State old_state = blk ? blk->status : 0;
#endif

    // When handling a fill, we should have no writes to this line.
    assert(addr == pkt->getBlockAddr(blkSize));
    assert(!writeBuffer.findMatch(addr, is_secure));

    if (!blk) {
        // better have read new data...
        assert(pkt->hasData() || pkt->cmd == MemCmd::InvalidateResp);

        // need to do a replacement if allocating, otherwise we stick
        // with the temporary storage

        #ifdef Ongal_VC
        #ifdef ASDT_Set_Associative_Array
        ASDT_Invalidation_Check(pkt->req->getPaddr(), writebacks);
        #endif
        Add_NEW_ASDT_map_entry(pkt);
        #endif


        blk = allocate ? allocateBlock(pkt, writebacks) : nullptr;

        if (!blk) {
            // No replaceable block or a mostly exclusive
            // cache... just use temporary storage to complete the
            // current request and then get rid of it
            blk = tempBlock;
            tempBlock->insert(addr, is_secure);
            DPRINTF(Cache, "using temp block for %#llx (%s)\n", addr,
                    is_secure ? "s" : "ns");
        }
        else {
         #ifdef Ongal_VC
                    if (get_VC_Structure_ptr() != NULL){
                      // L1 VC, check L1 line eviction (replacement,
                      // invalidations)
                      // in regions with active synonym
                      bool entry_with_synonym =
               get_VC_Structure_ptr()->entry_with_active_synonym(blk->paddr);
                      if (entry_with_synonym)
          ++num_invalidation_from_lower_level_to_region_with_active_synonym;
                    }
         #endif
        }
    } else {
        // existing block... probably an upgrade
        // don't clear block status... if block is already dirty we
        // don't want to lose that
    }

    // Block is guaranteed to be valid at this point
    assert(blk->isValid());
    assert(blk->isSecure() == is_secure);
   // printf("regenerateBlkAddr %lu\n",regenerateBlkAddr(blk));
   // printf("addr is %lu\n", addr);

    //ongal
    //disabling this assert, as this is not applicable
    //for a VIVT cache, unlike for a PIPT cache.
    //assert(regenerateBlkAddr(blk) == addr);

    blk->status |= BlkReadable;

    // sanity check for whole-line writes, which should always be
    // marked as writable as part of the fill, and then later marked
    // dirty as part of satisfyRequest
    if (pkt->cmd == MemCmd::InvalidateResp) {
        assert(!pkt->hasSharers());
    }

    // here we deal with setting the appropriate state of the line,
    // and we start by looking at the hasSharers flag, and ignore the
    // cacheResponding flag (normally signalling dirty data) if the
    // packet has sharers, thus the line is never allocated as Owned
    // (dirty but not writable), and always ends up being either
    // Shared, Exclusive or Modified, see Packet::setCacheResponding
    // for more details
    if (!pkt->hasSharers()) {
        // we could get a writable line from memory (rather than a
        // cache) even in a read-only cache, note that we set this bit
        // even for a read-only cache, possibly revisit this decision
        blk->status |= BlkWritable;

        // check if we got this via cache-to-cache transfer (i.e., from a
        // cache that had the block in Modified or Owned state)
        if (pkt->cacheResponding()) {
            // we got the block in Modified state, and invalidated the
            // owners copy
            blk->status |= BlkDirty;

            chatty_assert(!isReadOnly, "Should never see dirty snoop response "
                          "in read-only cache %s\n", name());
        }
    }

    DPRINTF(Cache, "Block addr %#llx (%s) moving from state %x to %s\n",
            addr, is_secure ? "s" : "ns", old_state, blk->print());

    // if we got new data, copy it in (checking for a read response
    // and a response that has data is the same in the end)
    if (pkt->isRead()) {
        // sanity checks
        assert(pkt->hasData());
        assert(pkt->getSize() == blkSize);

        pkt->writeDataToBlock(blk->data, blkSize);
    }
    // The block will be ready when the payload arrives and the fill is done
    blk->setWhenReady(clockEdge(fillLatency) + pkt->headerDelay +
                      pkt->payloadDelay);

    return blk;
}



//overloaded this function, so that we call the implementation of
//handleFill defined in Cache class, instead of BaseCache class.
void
Cache::recvTimingResp(PacketPtr pkt)
{
    DPRINTF(Cache,"Running Cache class recvTimingResp\n");
    assert(pkt->isResponse());

    // all header delay should be paid for by the crossbar, unless
    // this is a prefetch response from above
    panic_if(pkt->headerDelay != 0 && pkt->cmd != MemCmd::HardPFResp,
             "%s saw a non-zero packet delay\n", name());

    const bool is_error = pkt->isError();

    if (is_error) {
        DPRINTF(Cache, "%s: Cache received %s with error\n", __func__,
                pkt->print());
    }

    DPRINTF(Cache, "%s: Handling response %s\n", __func__,
            pkt->print());

    // if this is a write, we should be looking at an uncacheable
    // write
    if (pkt->isWrite()) {
        assert(pkt->req->isUncacheable());
        handleUncacheableWriteResp(pkt);
        return;
    }

    // we have dealt with any (uncacheable) writes above, from here on
    // we know we are dealing with an MSHR due to a miss or a prefetch
    MSHR *mshr = dynamic_cast<MSHR*>(pkt->popSenderState());
    assert(mshr);

    if (mshr == noTargetMSHR) {
        // we always clear at least one target
        clearBlocked(Blocked_NoTargets);
        noTargetMSHR = nullptr;
    }

    // Initial target is used just for stats
    QueueEntry::Target *initial_tgt = mshr->getTarget();
    int stats_cmd_idx = initial_tgt->pkt->cmdToIndex();
    Tick miss_latency = curTick() - initial_tgt->recvTime;

    if (pkt->req->isUncacheable()) {
        assert(pkt->req->masterId() < system->maxMasters());
        mshr_uncacheable_lat[stats_cmd_idx][pkt->req->masterId()] +=
            miss_latency;
    } else {
        assert(pkt->req->masterId() < system->maxMasters());
        mshr_miss_latency[stats_cmd_idx][pkt->req->masterId()] +=
            miss_latency;
    }

    PacketList writebacks;

    bool is_fill = !mshr->isForward &&
        (pkt->isRead() || pkt->cmd == MemCmd::UpgradeResp ||
         mshr->wasWholeLineWrite);

    // make sure that if the mshr was due to a whole line write then
    // the response is an invalidation
    assert(!mshr->wasWholeLineWrite || pkt->isInvalidate());

    CacheBlk *blk = tags->findBlock(pkt->getAddr(), pkt->isSecure());

    if (is_fill && !is_error) {
        DPRINTF(Cache, "Block for addr %#llx being updated in Cache\n",
                pkt->getAddr());

        const bool allocate = (writeAllocator && mshr->wasWholeLineWrite) ?
            writeAllocator->allocate() : mshr->allocOnFill();
        blk = handleFill(pkt, blk, writebacks, allocate);
        assert(blk != nullptr);
        ppFill->notify(pkt);
    }

    if (blk && blk->isValid() && pkt->isClean() && !pkt->isInvalidate()) {
        // The block was marked not readable while there was a pending
        // cache maintenance operation, restore its flag.
        blk->status |= BlkReadable;

        // This was a cache clean operation (without invalidate)
        // and we have a copy of the block already. Since there
        // is no invalidation, we can promote targets that don't
        // require a writable copy
        mshr->promoteReadable();
    }

    if (blk && blk->isWritable() && !pkt->req->isCacheInvalidate()) {
        // If at this point the referenced block is writable and the
        // response is not a cache invalidate, we promote targets that
        // were deferred as we couldn't guarrantee a writable copy
        mshr->promoteWritable();
    }

    serviceMSHRTargets(mshr, pkt, blk);

    if (mshr->promoteDeferredTargets()) {
        // avoid later read getting stale data while write miss is
        // outstanding.. see comment in timingAccess()
        if (blk) {
            blk->status &= ~BlkReadable;
        }
        mshrQueue.markPending(mshr);
        schedMemSideSendEvent(clockEdge() + pkt->payloadDelay);
    } else {
        // while we deallocate an mshr from the queue we still have to
        // check the isFull condition before and after as we might
        // have been using the reserved entries already
        const bool was_full = mshrQueue.isFull();
        mshrQueue.deallocate(mshr);
        if (was_full && !mshrQueue.isFull()) {
            clearBlocked(Blocked_NoMSHRs);
        }

        // Request the bus for a prefetch if this deallocation freed enough
        // MSHRs for a prefetch to take place
        BaseCache::helper_function();
    }

    // if we used temp block, check to see if its valid and then clear it out
    if (blk == tempBlock && tempBlock->isValid()) {
            BaseCache::evictBlock(blk, writebacks);
    }

    const Tick forward_time = clockEdge(forwardLatency) + pkt->headerDelay;
    // copy writebacks to write buffer
    doWritebacks(writebacks, forward_time);

    DPRINTF(CacheVerbose, "%s: Leaving with %s\n", __func__, pkt->print());
    delete pkt;
}



void
Cache::handleTimingReqMiss(PacketPtr pkt, CacheBlk *blk, Tick forward_time,
                           Tick request_time)
{
    if (pkt->req->isUncacheable()) {
        // ignore any existing MSHR if we are dealing with an
        // uncacheable request

        // should have flushed and have no valid block
        assert(!blk || !blk->isValid());

        mshr_uncacheable[pkt->cmdToIndex()][pkt->req->masterId()]++;

        if (pkt->isWrite()) {
            allocateWriteBuffer(pkt, forward_time);
        } else {
            assert(pkt->isRead());

            // uncacheable accesses always allocate a new MSHR

            // Here we are using forward_time, modelling the latency of
            // a miss (outbound) just as forwardLatency, neglecting the
            // lookupLatency component.
            allocateMissBuffer(pkt, forward_time);
        }

        return;
    }

    Addr blk_addr = pkt->getBlockAddr(blkSize);

    MSHR *mshr = mshrQueue.findMatch(blk_addr, pkt->isSecure());

    // Software prefetch handling:
    // To keep the core from waiting on data it won't look at
    // anyway, send back a response with dummy data. Miss handling
    // will continue asynchronously. Unfortunately, the core will
    // insist upon freeing original Packet/Request, so we have to
    // create a new pair with a different lifecycle. Note that this
    // processing happens before any MSHR munging on the behalf of
    // this request because this new Request will be the one stored
    // into the MSHRs, not the original.
    if (pkt->cmd.isSWPrefetch()) {
        assert(pkt->needsResponse());
        assert(pkt->req->hasPaddr());
        assert(!pkt->req->isUncacheable());

        // There's no reason to add a prefetch as an additional target
        // to an existing MSHR. If an outstanding request is already
        // in progress, there is nothing for the prefetch to do.
        // If this is the case, we don't even create a request at all.
        PacketPtr pf = nullptr;

        if (!mshr) {
            // copy the request and create a new SoftPFReq packet
            RequestPtr req = std::make_shared<Request>(pkt->req->getPaddr(),
                                                       pkt->req->getSize(),
                                                       pkt->req->getFlags(),
                                                       pkt->req->masterId());
            pf = new Packet(req, pkt->cmd);
            pf->allocate();
            assert(pf->matchAddr(pkt));
            assert(pf->getSize() == pkt->getSize());
        }

        pkt->makeTimingResponse();

        // request_time is used here, taking into account lat and the delay
        // charged if the packet comes from the xbar.
        cpuSidePort.schedTimingResp(pkt, request_time);

        // If an outstanding request is in progress (we found an
        // MSHR) this is set to null
        pkt = pf;
    }

    BaseCache::handleTimingReqMiss(pkt, mshr, blk, forward_time, request_time);
}

void
Cache::recvTimingReq(PacketPtr pkt)
{
    DPRINTF(CacheTags, "%s tags:\n%s\n", __func__, tags->print());

    promoteWholeLineWrites(pkt);

    if (pkt->cacheResponding()) {
        // a cache above us (but not where the packet came from) is
        // responding to the request, in other words it has the line
        // in Modified or Owned state
        DPRINTF(Cache, "Cache above responding to %s: not responding\n",
                pkt->print());

        // if the packet needs the block to be writable, and the cache
        // that has promised to respond (setting the cache responding
        // flag) is not providing writable (it is in Owned rather than
        // the Modified state), we know that there may be other Shared
        // copies in the system; go out and invalidate them all
        assert(pkt->needsWritable() && !pkt->responderHadWritable());

        // an upstream cache that had the line in Owned state
        // (dirty, but not writable), is responding and thus
        // transferring the dirty line from one branch of the
        // cache hierarchy to another

        // send out an express snoop and invalidate all other
        // copies (snooping a packet that needs writable is the
        // same as an invalidation), thus turning the Owned line
        // into a Modified line, note that we don't invalidate the
        // block in the current cache or any other cache on the
        // path to memory

        // create a downstream express snoop with cleared packet
        // flags, there is no need to allocate any data as the
        // packet is merely used to co-ordinate state transitions
        Packet *snoop_pkt = new Packet(pkt, true, false);

        // also reset the bus time that the original packet has
        // not yet paid for
        snoop_pkt->headerDelay = snoop_pkt->payloadDelay = 0;

        // make this an instantaneous express snoop, and let the
        // other caches in the system know that the another cache
        // is responding, because we have found the authorative
        // copy (Modified or Owned) that will supply the right
        // data
        snoop_pkt->setExpressSnoop();
        snoop_pkt->setCacheResponding();

        // this express snoop travels towards the memory, and at
        // every crossbar it is snooped upwards thus reaching
        // every cache in the system
        bool M5_VAR_USED success = memSidePort.sendTimingReq(snoop_pkt);
        // express snoops always succeed
        assert(success);

        // main memory will delete the snoop packet

        // queue for deletion, as opposed to immediate deletion, as
        // the sending cache is still relying on the packet
        pendingDelete.reset(pkt);

        // no need to take any further action in this particular cache
        // as an upstram cache has already committed to responding,
        // and we have already sent out any express snoops in the
        // section above to ensure all other copies in the system are
        // invalidated
        return;
    }

    BaseCache::recvTimingReq(pkt);
}

PacketPtr
Cache::createMissPacket(PacketPtr cpu_pkt, CacheBlk *blk,
                        bool needsWritable,
                        bool is_whole_line_write) const
{
    // should never see evictions here
    assert(!cpu_pkt->isEviction());

    bool blkValid = blk && blk->isValid();

    if (cpu_pkt->req->isUncacheable() ||
        (!blkValid && cpu_pkt->isUpgrade()) ||
        cpu_pkt->cmd == MemCmd::InvalidateReq || cpu_pkt->isClean()) {
        // uncacheable requests and upgrades from upper-level caches
        // that missed completely just go through as is
        return nullptr;
    }

    assert(cpu_pkt->needsResponse());

    MemCmd cmd;
    // @TODO make useUpgrades a parameter.
    // Note that ownership protocols require upgrade, otherwise a
    // write miss on a shared owned block will generate a ReadExcl,
    // which will clobber the owned copy.
    const bool useUpgrades = true;
    assert(cpu_pkt->cmd != MemCmd::WriteLineReq || is_whole_line_write);
    if (is_whole_line_write) {
        assert(!blkValid || !blk->isWritable());
        // forward as invalidate to all other caches, this gives us
        // the line in Exclusive state, and invalidates all other
        // copies
        cmd = MemCmd::InvalidateReq;
    } else if (blkValid && useUpgrades) {
        // only reason to be here is that blk is read only and we need
        // it to be writable
        assert(needsWritable);
        assert(!blk->isWritable());
        cmd = cpu_pkt->isLLSC() ? MemCmd::SCUpgradeReq : MemCmd::UpgradeReq;
    } else if (cpu_pkt->cmd == MemCmd::SCUpgradeFailReq ||
               cpu_pkt->cmd == MemCmd::StoreCondFailReq) {
        // Even though this SC will fail, we still need to send out the
        // request and get the data to supply it to other snoopers in the case
        // where the determination the StoreCond fails is delayed due to
        // all caches not being on the same local bus.
        cmd = MemCmd::SCUpgradeFailReq;
    } else {
        // block is invalid

        // If the request does not need a writable there are two cases
        // where we need to ensure the response will not fetch the
        // block in dirty state:
        // * this cache is read only and it does not perform
        //   writebacks,
        // * this cache is mostly exclusive and will not fill (since
        //   it does not fill it will have to writeback the dirty data
        //   immediately which generates uneccesary writebacks).
        bool force_clean_rsp = isReadOnly || clusivity == Enums::mostly_excl;
        cmd = needsWritable ? MemCmd::ReadExReq :
            (force_clean_rsp ? MemCmd::ReadCleanReq : MemCmd::ReadSharedReq);
    }
    PacketPtr pkt = new Packet(cpu_pkt->req, cmd, blkSize);

    // if there are upstream caches that have already marked the
    // packet as having sharers (not passing writable), pass that info
    // downstream
    if (cpu_pkt->hasSharers() && !needsWritable) {
        // note that cpu_pkt may have spent a considerable time in the
        // MSHR queue and that the information could possibly be out
        // of date, however, there is no harm in conservatively
        // assuming the block has sharers
        pkt->setHasSharers();
        DPRINTF(Cache, "%s: passing hasSharers from %s to %s\n",
                __func__, cpu_pkt->print(), pkt->print());
    }

    // the packet should be block aligned
    assert(pkt->getAddr() == pkt->getBlockAddr(blkSize));

    pkt->allocate();
    DPRINTF(Cache, "%s: created %s from %s\n", __func__, pkt->print(),
            cpu_pkt->print());
    return pkt;
}


Cycles
Cache::handleAtomicReqMiss(PacketPtr pkt, CacheBlk *&blk,
                           PacketList &writebacks)
{
    // deal with the packets that go through the write path of
    // the cache, i.e. any evictions and writes
    if (pkt->isEviction() || pkt->cmd == MemCmd::WriteClean ||
        (pkt->req->isUncacheable() && pkt->isWrite())) {
        Cycles latency = ticksToCycles(memSidePort.sendAtomic(pkt));

        // at this point, if the request was an uncacheable write
        // request, it has been satisfied by a memory below and the
        // packet carries the response back
        assert(!(pkt->req->isUncacheable() && pkt->isWrite()) ||
               pkt->isResponse());

        return latency;
    }

    // only misses left

    PacketPtr bus_pkt = createMissPacket(pkt, blk, pkt->needsWritable(),
                                         pkt->isWholeLineWrite(blkSize));

    bool is_forward = (bus_pkt == nullptr);

    if (is_forward) {
        // just forwarding the same request to the next level
        // no local cache operation involved
        bus_pkt = pkt;
    }

    DPRINTF(Cache, "%s: Sending an atomic %s\n", __func__,
            bus_pkt->print());

#if TRACING_ON
    CacheBlk::State old_state = blk ? blk->status : 0;
#endif

    Cycles latency = ticksToCycles(memSidePort.sendAtomic(bus_pkt));

    bool is_invalidate = bus_pkt->isInvalidate();

    // We are now dealing with the response handling
    DPRINTF(Cache, "%s: Receive response: %s in state %i\n", __func__,
            bus_pkt->print(), old_state);

    // If packet was a forward, the response (if any) is already
    // in place in the bus_pkt == pkt structure, so we don't need
    // to do anything.  Otherwise, use the separate bus_pkt to
    // generate response to pkt and then delete it.
    if (!is_forward) {
        if (pkt->needsResponse()) {
            assert(bus_pkt->isResponse());
            if (bus_pkt->isError()) {
                pkt->makeAtomicResponse();
                pkt->copyError(bus_pkt);
            } else if (pkt->isWholeLineWrite(blkSize)) {
                // note the use of pkt, not bus_pkt here.

                // write-line request to the cache that promoted
                // the write to a whole line
                const bool allocate = allocOnFill(pkt->cmd) &&
                    (!writeAllocator || writeAllocator->allocate());
                blk = handleFill(bus_pkt, blk, writebacks, allocate);
                assert(blk != NULL);
                is_invalidate = false;
                satisfyRequest(pkt, blk);
            } else if (bus_pkt->isRead() ||
                       bus_pkt->cmd == MemCmd::UpgradeResp) {
                // we're updating cache state to allow us to
                // satisfy the upstream request from the cache
                blk = handleFill(bus_pkt, blk, writebacks,
                                 allocOnFill(pkt->cmd));
                satisfyRequest(pkt, blk);
                maintainClusivity(pkt->fromCache(), blk);
            } else {
                // we're satisfying the upstream request without
                // modifying cache state, e.g., a write-through
                pkt->makeAtomicResponse();
            }
        }
        delete bus_pkt;
    }

    if (is_invalidate && blk && blk->isValid()) {
        invalidateBlock(blk);
    }

    return latency;
}

Tick
Cache::recvAtomic(PacketPtr pkt)
{
    promoteWholeLineWrites(pkt);

    // follow the same flow as in recvTimingReq, and check if a cache
    // above us is responding


#ifdef LateMemTrap

    // sanity check
    assert(pkt->isRequest());

    // Do only for L1 caches for late memory trap
    if ( get_is_l1cache() ){

      Fault fault = NoFault;
      TheISA::TLB *tlb  = (TheISA::TLB *)pkt->req->get_tlb_ptr();
      ThreadContext *tc = (ThreadContext *)pkt->req->get_tc_ptr();
      pkt->set_Fault_latetrap(fault); // initialization

      /* Check non-memory access based on vaddr.
         filter out several cases where have no need to look up L1 VCs.
         do some operations in advance in TLB:translate()

         BUT, actually, this does not need to be done early.
         L1 lookup will result in cache misses for them in the end,
         we can correctly process them by consulting TLBs. */

      bool non_mem_addr = false;
      fault = tlb->translateInt_only(pkt->req, tc, &non_mem_addr);
      bool use_paddr = false;

      if (fault == NoFault){

        if ( tlb->isProtectedMode(tc) != true || tlb->isPagingEnabled(tc) !=
                        true ){

          //physical address should be used for this operations
          uint64 vaddr = pkt->req->getVaddr();
          use_paddr = true;
          pkt->req->setPaddr(vaddr);
          pkt->setAddr(vaddr);
        }

        if (use_paddr){
          // here BaseTLB::Read is meaningless
          fault = tlb->finalizePhysical(pkt->req, tc, BaseTLB::Read);
        }
      }

      // Filter out late memory traps
      if (fault == NoFault) {

        if (!pkt->req->getFlags().isSet(Request::NO_ACCESS)) {

          Packet temp = Packet(pkt->req, pkt->cmd);
          temp.dataStatic(pkt->data);
          pkt->cp_Packet(&temp, true, true);
          pkt->set_Fault_latetrap(fault);

          if (pkt->req->isMmappedIpr()){ // mem mapped ipr
            return 1; // back to CPU
          }else{
            // process this packet in the caches
          }
        }else{
          // No cache access, back to CPU
          return 1;
        }
      }
      else{ // 2. Faults resulting from TLB lookups
        return 1; // back to CPU
      }

      ////////////////////////////////////////////////////////////////
      // L1 Virtual Cache Lookup Operations

      bool l1_vc_lookup = true;

      // Filter out several conditions we do not need to consult L1 VCs
      if (system->bypassCaches())
        l1_vc_lookup = false;
      if (pkt->memInhibitAsserted())
        l1_vc_lookup = false;

      // handle tlb invalidations
      std::set<uint64_t> demaps = pkt->req->get_demap_pages();
      if ( demaps.size() > 0 ){
        // bypass L1 VC access if we have some request invalidating TLB entries
        l1_vc_lookup = false;
      }

      // see if L1 VC needs to be accessed.
      if ( l1_vc_lookup ){

        // ifetch or loads
        CacheBlk *blk = NULL;
        PacketList writebacks;
        bool satisfied = access_virtual_cache(pkt, blk, lat, writebacks);

        if (satisfied){ // Cache Hit, no need to execute below

          // Handle writebacks (from the response handling) if needed
          while (!writebacks.empty()){
            PacketPtr wbPkt = writebacks.front();
            memSidePort->sendAtomic(wbPkt);
            writebacks.pop_front();
            delete wbPkt;
          }

          if (pkt->needsResponse()) {
            pkt->makeAtomicResponse();
          }

          return 1;
        }
      }

      ////////////////////////////////////////////////////////////////
      // Virtual Cache Misses or Bypassing
      // TLB Looup Operations

      if ( (tlb != NULL) && (tc != NULL) ){

        if (pkt->req->isInstFetch()){ // ifetch
          fault = tlb->translateAtomic(pkt->req, tc, BaseTLB::Execute);
          //}else if (pkt->req->get_store() != true){ // data read request
        }else{
          if (pkt->isRead()){ // data read request
            fault = tlb->translateAtomic(pkt->req, tc, BaseTLB::Read);
          }else{ // data write
            fault = tlb->translateAtomic(pkt->req, tc, BaseTLB::Write);
          }
        }

        // keep the fault information from TLB lookup, in case CPU may use it.
        pkt->set_Fault_latetrap(fault);

        // Filter out late memory traps
        if (fault == NoFault) {

          if (!pkt->req->getFlags().isSet(Request::NO_ACCESS)) {

            Packet temp = Packet(pkt->req, pkt->cmd);
            temp.dataStatic(pkt->data);
            pkt->cp_Packet(&temp, true, true);
            pkt->set_Fault_latetrap(fault);

            if (pkt->req->isMmappedIpr()){ // mem mapped ipr
              return 1; // back to CPU
            }else{
              // process this packet in the caches
            }
          }else{
            // No cache access, back to CPU
            return 1;
          }
        }
        else{ // 2. Faults resulting from TLB lookups
          return 1; // back to CPU
        }
      }
    } // if (is_l1_cache)
#endif


    if (pkt->cacheResponding()) {
        assert(!pkt->req->isCacheInvalidate());
        DPRINTF(Cache, "Cache above responding to %s: not responding\n",
                pkt->print());

        // if a cache is responding, and it had the line in Owned
        // rather than Modified state, we need to invalidate any
        // copies that are not on the same path to memory
        assert(pkt->needsWritable() && !pkt->responderHadWritable());

        return memSidePort.sendAtomic(pkt);
    }

    return BaseCache::recvAtomic(pkt);
}


#ifdef Ongal_VC
int
Cache::find_victim_LRU(int set_index, uint64_t* victim_PPN){

  int target_way_index = -1;
  int target_way_LRU = m_vc_structure->inc_LRU_counter();
  // allocate up-to-date value
  int target_way_lines = 0;
  int num_ways = m_vc_structure->get_num_ways_ASDT_SA(set_index);



  for (int index = 0; index < num_ways ; ++index){

    ASDT_SA_entry *entry_SA = m_vc_structure->access_ASDT_SA_entry(set_index,
                    index);

    // valid entry and no locked entry
    if ( entry_SA != NULL &&
        entry_SA->get_valid() &&
        !entry_SA->get_lock()){

      uint64_t PPN = entry_SA->get_PPN(); // get a PPN
      ASDT_entry* entry = m_vc_structure->access_matching_ASDT_map(PPN);

      if (entry == NULL){
        std::cout<<"find_victim_few_lines, should be a matching entry in an"
                "ASDT map"<<std::endl;
        abort();
      }else{
        // let's check only the entries having few lines
        if ( entry->get_LRU() < target_way_LRU ){

          bool mshr_hit = false;

          // iterating all lines see if mshr hit occurs
          int line_size = m_vc_structure->get_line_size();
          int region_size = m_vc_structure->get_region_size();
          int num_lines = region_size/line_size;
          uint64_t region_paddr = PPN * region_size;

          // iterating all lines in the PPN
          for ( int line_index = 0 ; line_index < num_lines ; ++line_index ){

            uint64_t line_paddr =  region_paddr + (line_index*line_size);
            // find a block

            CacheBlk *blk = tags->findBlock(line_paddr, true);
            if ( !(blk && blk->isValid()) ) {
              blk = tags->findBlock(line_paddr, false);
            }

            // check mshr
            if ( blk && blk->isValid() ) {
              #ifdef Ongal_VC
              Addr repl_addr = blk->paddr;
              #else
              Addr repl_addr = tags->regenerateBlkAddr(blk->tag, blk->set);
              #endif
              MSHR *repl_mshr = mshrQueue.findMatch(repl_addr,
                              blk->isSecure());
              if (repl_mshr) {
                // must be an outstanding upgrade request on a block we're
                // about to replace...

                mshr_hit = true;
              } // mshr hit detection
            } // mshr hit check
          } // iterating all lines

          ///////////////////////////////////////////
          // found an entry satisfying the conditions
          if (!mshr_hit){
            // new target way!
            target_way_lines = entry->get_num_cached_lines();
            target_way_LRU = entry->get_LRU();
            target_way_index = index;
            *victim_PPN = PPN;
          }
        }
      }
    }
  }

  // profiling for ASDT conflict miss and number of invalidated lines for the
  // event
  if (target_way_index != -1){ // when we found a target entry
    num_asdt_entry_conflict_miss++;
    num_evicted_lines_per_asdt_conflict_miss += target_way_lines;
  }

  return target_way_index;
}


int
Cache::find_victim_few_lines(int set_index, uint64_t* victim_PPN,
                uint64_t* victim_VPN,uint64_t* victim_CR3){

  int target_way_index = -1;
  int target_way_lines = 9999;

  int target_way_LRU = m_vc_structure->inc_LRU_counter();

  int num_ways = m_vc_structure->get_num_ways_ASDT_SA(set_index);



  for (int index = 0; index < num_ways ; ++index){

    ASDT_SA_entry *entry_SA = m_vc_structure->access_ASDT_SA_entry(set_index,
                    index);

    // valid entry and no locked entry
    if ( entry_SA != NULL &&
        entry_SA->get_valid() &&
        !entry_SA->get_lock()){

      uint64_t PPN = entry_SA->get_PPN(); // get a PPN

      ASDT_entry* entry = m_vc_structure->access_matching_ASDT_map(PPN);
      uint64_t VPN = entry->get_virtual_page_number();
      uint64_t CR3 = entry->get_cr3();
      if (entry == NULL){
        std::cout<<"find_victim_few_lines, should be a matching entry in an"
                "ASDT map"<<std::endl;
        abort();
      }else{
        // let's check only the entries having few lines
        if ( entry->get_num_cached_lines() < 4 &&
                        entry->get_num_cached_lines() > 0 ){

          bool mshr_hit = false;

          // iterating all lines see if mshr hit occurs
          int line_size = m_vc_structure->get_line_size();
          int region_size = m_vc_structure->get_region_size();
          int num_lines = region_size/line_size;
          uint64_t region_paddr = PPN * region_size;

          // iterating all lines in the PPN
          for ( int line_index = 0 ; line_index < num_lines ; ++line_index ){

            uint64_t line_paddr =  region_paddr + (line_index*line_size);
            // find a block

            CacheBlk *blk = tags->findBlock(line_paddr, true);
            if ( !(blk && blk->isValid()) ) {
              blk = tags->findBlock(line_paddr, false);
            }

            // check mshr
            if ( blk && blk->isValid() ) {
              #ifdef Ongal_VC
              Addr repl_addr = blk->paddr;
              #else
              Addr repl_addr = tags->regenerateBlkAddr(blk->tag, blk->set);
              #endif
            MSHR *repl_mshr = mshrQueue.findMatch(repl_addr, blk->isSecure());
              if (repl_mshr) {
                // must be an outstanding upgrade request on a block we're
                // about to replace...

                mshr_hit = true;
              } // mshr hit detection
            } // mshr hit check
          } // iterating all lines

          // found an entry satisfying the conditions
          if ( !mshr_hit &&
                          (target_way_lines >entry->get_num_cached_lines())){
              // new target way!
              target_way_lines = entry->get_num_cached_lines();
              target_way_index = index;
              //update the replacement bits
              target_way_LRU = entry->get_LRU();
              *victim_PPN = PPN;
              //return the leading virtual page
              //as well as the leading cr3
              *victim_VPN = VPN;
              *victim_CR3 = CR3;
          }
          //if two cache blocks have the same number of cache lines,
          //then pick the older block for eviction.
          else if (!mshr_hit &&
                          (target_way_lines == entry->get_num_cached_lines())&&
                           (entry->get_LRU() < target_way_LRU)){
              // new target way!
              target_way_lines = entry->get_num_cached_lines();
              target_way_index = index;
              //update the replacement bits
              target_way_LRU = entry->get_LRU();
              *victim_PPN = PPN;
              //return the leading virtual page
              //as well as the leading cr3
              *victim_VPN = VPN;
              *victim_CR3 = CR3;
          }

        }
      }
    }
  }

  // profiling for ASDT conflict miss and number of invalidated lines for the
  // event
  if (target_way_index != -1){ // when we found a target entry
    num_asdt_entry_conflict_miss++;
   // cout<<"The ASDT index of interest is: "<< set_index <<endl;
   // cout<<"The ASDT way evicted is: "<< target_way_index <<endl;
   // cout<<"The number of lines is: "<<target_way_lines<<endl;
    num_evicted_lines_per_asdt_conflict_miss += target_way_lines;
  }

  return target_way_index;
}

#endif




/////////////////////////////////////////////////////
//
// Response handling: responses from the memory side
//
/////////////////////////////////////////////////////


void
Cache::serviceMSHRTargets(MSHR *mshr, const PacketPtr pkt, CacheBlk *blk)
{
    QueueEntry::Target *initial_tgt = mshr->getTarget();
    // First offset for critical word first calculations
    const int initial_offset = initial_tgt->pkt->getOffset(blkSize);

    const bool is_error = pkt->isError();
    // allow invalidation responses originating from write-line
    // requests to be discarded
    bool is_invalidate = pkt->isInvalidate() &&
        !mshr->wasWholeLineWrite;

    MSHR::TargetList targets = mshr->extractServiceableTargets(pkt);
    for (auto &target: targets) {
        Packet *tgt_pkt = target.pkt;
        switch (target.source) {
          case MSHR::Target::FromCPU:
            Tick completion_time;
            // Here we charge on completion_time the delay of the xbar if the
            // packet comes from it, charged on headerDelay.
            completion_time = pkt->headerDelay;

            // Software prefetch handling for cache closest to core
            if (tgt_pkt->cmd.isSWPrefetch()) {
                // a software prefetch would have already been ack'd
                // immediately with dummy data so the core would be able to
                // retire it. This request completes right here, so we
                // deallocate it.
                delete tgt_pkt;
                break; // skip response
            }

            // unlike the other packet flows, where data is found in other
            // caches or memory and brought back, write-line requests always
            // have the data right away, so the above check for "is fill?"
            // cannot actually be determined until examining the stored MSHR
            // state. We "catch up" with that logic here, which is duplicated
            // from above.
            if (tgt_pkt->cmd == MemCmd::WriteLineReq) {
                assert(!is_error);
                assert(blk);
                assert(blk->isWritable());
            }

            if (blk && blk->isValid() && !mshr->isForward) {
                satisfyRequest(tgt_pkt, blk, true, mshr->hasPostDowngrade());

                // How many bytes past the first request is this one
                int transfer_offset =
                    tgt_pkt->getOffset(blkSize) - initial_offset;
                if (transfer_offset < 0) {
                    transfer_offset += blkSize;
                }

                // If not critical word (offset) return payloadDelay.
                // responseLatency is the latency of the return path
                // from lower level caches/memory to an upper level cache or
                // the core.
                completion_time += clockEdge(responseLatency) +
                    (transfer_offset ? pkt->payloadDelay : 0);

                assert(!tgt_pkt->req->isUncacheable());

                assert(tgt_pkt->req->masterId() < system->maxMasters());
                missLatency[tgt_pkt->cmdToIndex()][tgt_pkt->req->masterId()] +=
                    completion_time - target.recvTime;
            } else if (pkt->cmd == MemCmd::UpgradeFailResp) {
                // failed StoreCond upgrade
                assert(tgt_pkt->cmd == MemCmd::StoreCondReq ||
                       tgt_pkt->cmd == MemCmd::StoreCondFailReq ||
                       tgt_pkt->cmd == MemCmd::SCUpgradeFailReq);
                // responseLatency is the latency of the return path
                // from lower level caches/memory to an upper level cache or
                // the core.
                completion_time += clockEdge(responseLatency) +
                    pkt->payloadDelay;
                tgt_pkt->req->setExtraData(0);
            } else {
                // We are about to send a response to a cache above
                // that asked for an invalidation; we need to
                // invalidate our copy immediately as the most
                // up-to-date copy of the block will now be in the
                // cache above. It will also prevent this cache from
                // responding (if the block was previously dirty) to
                // snoops as they should snoop the caches above where
                // they will get the response from.
                if (is_invalidate && blk && blk->isValid()) {
                    invalidateBlock(blk);
                }
                // not a cache fill, just forwarding response
                // responseLatency is the latency of the return path
                // from lower level cahces/memory to the core.
                completion_time += clockEdge(responseLatency) +
                    pkt->payloadDelay;
                if (pkt->isRead() && !is_error) {
                    // sanity check
                    assert(pkt->matchAddr(tgt_pkt));
                    assert(pkt->getSize() >= tgt_pkt->getSize());

                    tgt_pkt->setData(pkt->getConstPtr<uint8_t>());
                }

                // this response did not allocate here and therefore
                // it was not consumed, make sure that any flags are
                // carried over to cache above
                tgt_pkt->copyResponderFlags(pkt);
            }
            tgt_pkt->makeTimingResponse();
            // if this packet is an error copy that to the new packet
            if (is_error)
                tgt_pkt->copyError(pkt);
            if (tgt_pkt->cmd == MemCmd::ReadResp &&
                (is_invalidate || mshr->hasPostInvalidate())) {
                // If intermediate cache got ReadRespWithInvalidate,
                // propagate that.  Response should not have
                // isInvalidate() set otherwise.
                tgt_pkt->cmd = MemCmd::ReadRespWithInvalidate;
                DPRINTF(Cache, "%s: updated cmd to %s\n", __func__,
                        tgt_pkt->print());
            }
            // Reset the bus additional time as it is now accounted for
            tgt_pkt->headerDelay = tgt_pkt->payloadDelay = 0;
            cpuSidePort.schedTimingResp(tgt_pkt, completion_time);
            break;

          case MSHR::Target::FromPrefetcher:
            assert(tgt_pkt->cmd == MemCmd::HardPFReq);
            if (blk)
                blk->status |= BlkHWPrefetched;
            delete tgt_pkt;
            break;

          case MSHR::Target::FromSnoop:
            // I don't believe that a snoop can be in an error state
            assert(!is_error);
            // response to snoop request
            DPRINTF(Cache, "processing deferred snoop...\n");
            // If the response is invalidating, a snooping target can
            // be satisfied if it is also invalidating. If the reponse is, not
            // only invalidating, but more specifically an InvalidateResp and
            // the MSHR was created due to an InvalidateReq then a cache above
            // is waiting to satisfy a WriteLineReq. In this case even an
            // non-invalidating snoop is added as a target here since this is
            // the ordering point. When the InvalidateResp reaches this cache,
            // the snooping target will snoop further the cache above with the
            // WriteLineReq.
            assert(!is_invalidate || pkt->cmd == MemCmd::InvalidateResp ||
                   pkt->req->isCacheMaintenance() ||
                   mshr->hasPostInvalidate());
            handleSnoop(tgt_pkt, blk, true, true, mshr->hasPostInvalidate());
            break;

          default:
            panic("Illegal target->source enum %d\n", target.source);
        }
    }

    maintainClusivity(targets.hasFromCache, blk);

    if (blk && blk->isValid()) {
        // an invalidate response stemming from a write line request
        // should not invalidate the block, so check if the
        // invalidation should be discarded
        if (is_invalidate || mshr->hasPostInvalidate()) {
            invalidateBlock(blk);
        } else if (mshr->hasPostDowngrade()) {
            blk->status &= ~BlkWritable;
        }
    }
}

PacketPtr
Cache::evictBlock(CacheBlk *blk)
{
    //Ongal, we might have invalidated block during
    //ASDT invalidation check, need to check if the
    //block is valid or not, as well.
    PacketPtr pkt = ((blk->isDirty() || writebackClean)&&(blk->isValid())) ?
        writebackBlk(blk) : cleanEvictBlk(blk);

    invalidateBlock(blk);

    return pkt;
}

PacketPtr
Cache::cleanEvictBlk(CacheBlk *blk)
{
    assert(!writebackClean);
    assert(blk && blk->isValid() && !blk->isDirty());

    RequestPtr req;
    // Creating a zero sized write, a message to the snoop filter
     #ifdef Ongal_VC
     req = std::make_shared<Request>(
        blk->paddr, blkSize, 0,
        Request::wbMasterId);
     #else
     req = std::make_shared<Request>(
        regenerateBlkAddr(blk), blkSize, 0, Request::wbMasterId);
     #endif

    if (blk->isSecure())
        req->setFlags(Request::SECURE);

    req->taskId(blk->task_id);

    PacketPtr pkt = new Packet(req, MemCmd::CleanEvict);
    pkt->allocate();
    DPRINTF(Cache, "Create CleanEvict %s\n", pkt->print());

    return pkt;
}

/////////////////////////////////////////////////////
//
// Snoop path: requests coming in from the memory side
//
/////////////////////////////////////////////////////

void
Cache::doTimingSupplyResponse(PacketPtr req_pkt, const uint8_t *blk_data,
                              bool already_copied, bool pending_inval)
{
    // sanity check
    assert(req_pkt->isRequest());
    assert(req_pkt->needsResponse());

    DPRINTF(Cache, "%s: for %s\n", __func__, req_pkt->print());
    // timing-mode snoop responses require a new packet, unless we
    // already made a copy...
    PacketPtr pkt = req_pkt;
    if (!already_copied)
        // do not clear flags, and allocate space for data if the
        // packet needs it (the only packets that carry data are read
        // responses)
        pkt = new Packet(req_pkt, false, req_pkt->isRead());

    assert(req_pkt->req->isUncacheable() || req_pkt->isInvalidate() ||
           pkt->hasSharers());
    pkt->makeTimingResponse();
    if (pkt->isRead()) {
        pkt->setDataFromBlock(blk_data, blkSize);
    }
    if (pkt->cmd == MemCmd::ReadResp && pending_inval) {
        // Assume we defer a response to a read from a far-away cache
        // A, then later defer a ReadExcl from a cache B on the same
        // bus as us. We'll assert cacheResponding in both cases, but
        // in the latter case cacheResponding will keep the
        // invalidation from reaching cache A. This special response
        // tells cache A that it gets the block to satisfy its read,
        // but must immediately invalidate it.
        pkt->cmd = MemCmd::ReadRespWithInvalidate;
    }
    // Here we consider forward_time, paying for just forward latency and
    // also charging the delay provided by the xbar.
    // forward_time is used as send_time in next allocateWriteBuffer().
    Tick forward_time = clockEdge(forwardLatency) + pkt->headerDelay;
    // Here we reset the timing of the packet.
    pkt->headerDelay = pkt->payloadDelay = 0;
    DPRINTF(CacheVerbose, "%s: created response: %s tick: %lu\n", __func__,
            pkt->print(), forward_time);
    memSidePort.schedTimingSnoopResp(pkt, forward_time);
}

uint32_t
Cache::handleSnoop(PacketPtr pkt, CacheBlk *blk, bool is_timing,
                   bool is_deferred, bool pending_inval)
{
    DPRINTF(CacheVerbose, "%s: for %s\n", __func__, pkt->print());
    // deferred snoops can only happen in timing mode
    assert(!(is_deferred && !is_timing));
    // pending_inval only makes sense on deferred snoops
    assert(!(pending_inval && !is_deferred));
    assert(pkt->isRequest());

    // the packet may get modified if we or a forwarded snooper
    // responds in atomic mode, so remember a few things about the
    // original packet up front
    bool invalidate = pkt->isInvalidate();
    bool M5_VAR_USED needs_writable = pkt->needsWritable();

    // at the moment we could get an uncacheable write which does not
    // have the invalidate flag, and we need a suitable way of dealing
    // with this case
    panic_if(invalidate && pkt->req->isUncacheable(),
             "%s got an invalidating uncacheable snoop request %s",
             name(), pkt->print());

    uint32_t snoop_delay = 0;

    if (forwardSnoops) {
        // first propagate snoop upward to see if anyone above us wants to
        // handle it.  save & restore packet src since it will get
        // rewritten to be relative to cpu-side bus (if any)
        if (is_timing) {
            // copy the packet so that we can clear any flags before
            // forwarding it upwards, we also allocate data (passing
            // the pointer along in case of static data), in case
            // there is a snoop hit in upper levels
            Packet snoopPkt(pkt, true, true);
            snoopPkt.setExpressSnoop();
            // the snoop packet does not need to wait any additional
            // time
            snoopPkt.headerDelay = snoopPkt.payloadDelay = 0;
            cpuSidePort.sendTimingSnoopReq(&snoopPkt);

            // add the header delay (including crossbar and snoop
            // delays) of the upward snoop to the snoop delay for this
            // cache
            snoop_delay += snoopPkt.headerDelay;

            // If this request is a prefetch or clean evict and an upper level
            // signals block present, make sure to propagate the block
            // presence to the requester.
            if (snoopPkt.isBlockCached()) {
                pkt->setBlockCached();
            }
            // If the request was satisfied by snooping the cache
            // above, mark the original packet as satisfied too.
            if (snoopPkt.satisfied()) {
                pkt->setSatisfied();
            }

            // Copy over flags from the snoop response to make sure we
            // inform the final destination
            pkt->copyResponderFlags(&snoopPkt);
        } else {
            bool already_responded = pkt->cacheResponding();
            cpuSidePort.sendAtomicSnoop(pkt);
            if (!already_responded && pkt->cacheResponding()) {
                // cache-to-cache response from some upper cache:
                // forward response to original requester
                assert(pkt->isResponse());
            }
        }
    }

    bool respond = false;
    bool blk_valid = blk && blk->isValid();
    if (pkt->isClean()) {
        if (blk_valid && blk->isDirty()) {
            DPRINTF(CacheVerbose, "%s: packet (snoop) %s found block: %s\n",
                    __func__, pkt->print(), blk->print());
            PacketPtr wb_pkt = writecleanBlk(blk, pkt->req->getDest(), pkt->id);
            PacketList writebacks;
            writebacks.push_back(wb_pkt);

            if (is_timing) {
                // anything that is merely forwarded pays for the forward
                // latency and the delay provided by the crossbar
                Tick forward_time = clockEdge(forwardLatency) +
                    pkt->headerDelay;
                doWritebacks(writebacks, forward_time);
            } else {
                doWritebacksAtomic(writebacks);
            }
            pkt->setSatisfied();
        }
    } else if (!blk_valid) {
        DPRINTF(CacheVerbose, "%s: snoop miss for %s\n", __func__,
                pkt->print());
        if (is_deferred) {
            // we no longer have the block, and will not respond, but a
            // packet was allocated in MSHR::handleSnoop and we have
            // to delete it
            assert(pkt->needsResponse());

            // we have passed the block to a cache upstream, that
            // cache should be responding
            assert(pkt->cacheResponding());

            delete pkt;
        }
        return snoop_delay;
    } else {
        DPRINTF(Cache, "%s: snoop hit for %s, old state is %s\n", __func__,
                pkt->print(), blk->print());

        // We may end up modifying both the block state and the packet (if
        // we respond in atomic mode), so just figure out what to do now
        // and then do it later. We respond to all snoops that need
        // responses provided we have the block in dirty state. The
        // invalidation itself is taken care of below. We don't respond to
        // cache maintenance operations as this is done by the destination
        // xbar.
        respond = blk->isDirty() && pkt->needsResponse();

        chatty_assert(!(isReadOnly && blk->isDirty()), "Should never have "
                      "a dirty block in a read-only cache %s\n", name());
    }

    // Invalidate any prefetch's from below that would strip write permissions
    // MemCmd::HardPFReq is only observed by upstream caches.  After missing
    // above and in it's own cache, a new MemCmd::ReadReq is created that
    // downstream caches observe.
    if (pkt->mustCheckAbove()) {
        DPRINTF(Cache, "Found addr %#llx in upper level cache for snoop %s "
                "from lower cache\n", pkt->getAddr(), pkt->print());
        pkt->setBlockCached();
        return snoop_delay;
    }

    if (pkt->isRead() && !invalidate) {
        // reading without requiring the line in a writable state
        assert(!needs_writable);
        pkt->setHasSharers();

        // if the requesting packet is uncacheable, retain the line in
        // the current state, otherwhise unset the writable flag,
        // which means we go from Modified to Owned (and will respond
        // below), remain in Owned (and will respond below), from
        // Exclusive to Shared, or remain in Shared
        if (!pkt->req->isUncacheable())
            blk->status &= ~BlkWritable;
        DPRINTF(Cache, "new state is %s\n", blk->print());
    }

    if (respond) {
        // prevent anyone else from responding, cache as well as
        // memory, and also prevent any memory from even seeing the
        // request
        pkt->setCacheResponding();
        if (!pkt->isClean() && blk->isWritable()) {
            // inform the cache hierarchy that this cache had the line
            // in the Modified state so that we avoid unnecessary
            // invalidations (see Packet::setResponderHadWritable)
            pkt->setResponderHadWritable();

            // in the case of an uncacheable request there is no point
            // in setting the responderHadWritable flag, but since the
            // recipient does not care there is no harm in doing so
        } else {
            // if the packet has needsWritable set we invalidate our
            // copy below and all other copies will be invalidates
            // through express snoops, and if needsWritable is not set
            // we already called setHasSharers above
        }

        // if we are returning a writable and dirty (Modified) line,
        // we should be invalidating the line
        panic_if(!invalidate && !pkt->hasSharers(),
                 "%s is passing a Modified line through %s, "
                 "but keeping the block", name(), pkt->print());

        if (is_timing) {
            doTimingSupplyResponse(pkt, blk->data, is_deferred, pending_inval);
        } else {
            pkt->makeAtomicResponse();
            // packets such as upgrades do not actually have any data
            // payload
            if (pkt->hasData())
                pkt->setDataFromBlock(blk->data, blkSize);
        }
    }

    if (!respond && is_deferred) {
        assert(pkt->needsResponse());
        delete pkt;
    }

    // Do this last in case it deallocates block data or something
    // like that
    if (blk_valid && invalidate) {
        invalidateBlock(blk);
        DPRINTF(Cache, "new state is %s\n", blk->print());
    }

    return snoop_delay;
}


void
Cache::recvTimingSnoopReq(PacketPtr pkt)
{
    DPRINTF(CacheVerbose, "%s: for %s\n", __func__, pkt->print());

    // no need to snoop requests that are not in range
    if (!inRange(pkt->getAddr())) {
        return;
    }

    bool is_secure = pkt->isSecure();
    CacheBlk *blk = tags->findBlock(pkt->getAddr(), is_secure);

    Addr blk_addr = pkt->getBlockAddr(blkSize);
    MSHR *mshr = mshrQueue.findMatch(blk_addr, is_secure);

    // Update the latency cost of the snoop so that the crossbar can
    // account for it. Do not overwrite what other neighbouring caches
    // have already done, rather take the maximum. The update is
    // tentative, for cases where we return before an upward snoop
    // happens below.
    pkt->snoopDelay = std::max<uint32_t>(pkt->snoopDelay,
                                         lookupLatency * clockPeriod());

    // Inform request(Prefetch, CleanEvict or Writeback) from below of
    // MSHR hit, set setBlockCached.
    if (mshr && pkt->mustCheckAbove()) {
        DPRINTF(Cache, "Setting block cached for %s from lower cache on "
                "mshr hit\n", pkt->print());
        pkt->setBlockCached();
        return;
    }

    // Let the MSHR itself track the snoop and decide whether we want
    // to go ahead and do the regular cache snoop
    if (mshr && mshr->handleSnoop(pkt, order++)) {
        DPRINTF(Cache, "Deferring snoop on in-service MSHR to blk %#llx (%s)."
                "mshrs: %s\n", blk_addr, is_secure ? "s" : "ns",
                mshr->print());

        if (mshr->getNumTargets() > numTarget)
            warn("allocating bonus target for snoop"); //handle later
        return;
    }

    //We also need to check the writeback buffers and handle those
    WriteQueueEntry *wb_entry = writeBuffer.findMatch(blk_addr, is_secure);
    if (wb_entry) {
        DPRINTF(Cache, "Snoop hit in writeback to addr %#llx (%s)\n",
                pkt->getAddr(), is_secure ? "s" : "ns");
        // Expect to see only Writebacks and/or CleanEvicts here, both of
        // which should not be generated for uncacheable data.
        assert(!wb_entry->isUncacheable());
        // There should only be a single request responsible for generating
        // Writebacks/CleanEvicts.
        assert(wb_entry->getNumTargets() == 1);
        PacketPtr wb_pkt = wb_entry->getTarget()->pkt;
        assert(wb_pkt->isEviction() || wb_pkt->cmd == MemCmd::WriteClean);

        if (pkt->isEviction()) {
            // if the block is found in the write queue, set the BLOCK_CACHED
            // flag for Writeback/CleanEvict snoop. On return the snoop will
            // propagate the BLOCK_CACHED flag in Writeback packets and prevent
            // any CleanEvicts from travelling down the memory hierarchy.
            pkt->setBlockCached();
            DPRINTF(Cache, "%s: Squashing %s from lower cache on writequeue "
                    "hit\n", __func__, pkt->print());
            return;
        }

        // conceptually writebacks are no different to other blocks in
        // this cache, so the behaviour is modelled after handleSnoop,
        // the difference being that instead of querying the block
        // state to determine if it is dirty and writable, we use the
        // command and fields of the writeback packet
        bool respond = wb_pkt->cmd == MemCmd::WritebackDirty &&
            pkt->needsResponse();
        bool have_writable = !wb_pkt->hasSharers();
        bool invalidate = pkt->isInvalidate();

        if (!pkt->req->isUncacheable() && pkt->isRead() && !invalidate) {
            assert(!pkt->needsWritable());
            pkt->setHasSharers();
            wb_pkt->setHasSharers();
        }

        if (respond) {
            pkt->setCacheResponding();

            if (have_writable) {
                pkt->setResponderHadWritable();
            }

            doTimingSupplyResponse(pkt, wb_pkt->getConstPtr<uint8_t>(),
                                   false, false);
        }

        if (invalidate && wb_pkt->cmd != MemCmd::WriteClean) {
            // Invalidation trumps our writeback... discard here
            // Note: markInService will remove entry from writeback buffer.
            markInService(wb_entry);
            delete wb_pkt;
        }
    }

    // If this was a shared writeback, there may still be
    // other shared copies above that require invalidation.
    // We could be more selective and return here if the
    // request is non-exclusive or if the writeback is
    // exclusive.
    uint32_t snoop_delay = handleSnoop(pkt, blk, true, false, false);

    // Override what we did when we first saw the snoop, as we now
    // also have the cost of the upwards snoops to account for
    pkt->snoopDelay = std::max<uint32_t>(pkt->snoopDelay, snoop_delay +
                                         lookupLatency * clockPeriod());
}

Tick
Cache::recvAtomicSnoop(PacketPtr pkt)
{
    // no need to snoop requests that are not in range.
    if (!inRange(pkt->getAddr())) {
        return 0;
    }

    CacheBlk *blk = tags->findBlock(pkt->getAddr(), pkt->isSecure());
    uint32_t snoop_delay = handleSnoop(pkt, blk, false, false, false);
    return snoop_delay + lookupLatency * clockPeriod();
}

bool
Cache::isCachedAbove(PacketPtr pkt, bool is_timing)
{
    if (!forwardSnoops)
        return false;
    // Mirroring the flow of HardPFReqs, the cache sends CleanEvict and
    // Writeback snoops into upper level caches to check for copies of the
    // same block. Using the BLOCK_CACHED flag with the Writeback/CleanEvict
    // packet, the cache can inform the crossbar below of presence or absence
    // of the block.
    if (is_timing) {
        Packet snoop_pkt(pkt, true, false);
        snoop_pkt.setExpressSnoop();
        // Assert that packet is either Writeback or CleanEvict and not a
        // prefetch request because prefetch requests need an MSHR and may
        // generate a snoop response.
        assert(pkt->isEviction() || pkt->cmd == MemCmd::WriteClean);
        snoop_pkt.senderState = nullptr;
        cpuSidePort.sendTimingSnoopReq(&snoop_pkt);
        // Writeback/CleanEvict snoops do not generate a snoop response.
        assert(!(snoop_pkt.cacheResponding()));
        return snoop_pkt.isBlockCached();
    } else {
        cpuSidePort.sendAtomicSnoop(pkt);
        return pkt->isBlockCached();
    }
}

bool
Cache::sendMSHRQueuePacket(MSHR* mshr)
{
    assert(mshr);

    // use request from 1st target
    PacketPtr tgt_pkt = mshr->getTarget()->pkt;

    if (tgt_pkt->cmd == MemCmd::HardPFReq && forwardSnoops) {
        DPRINTF(Cache, "%s: MSHR %s\n", __func__, tgt_pkt->print());

        // we should never have hardware prefetches to allocated
        // blocks
        assert(!tags->findBlock(mshr->blkAddr, mshr->isSecure));

        // We need to check the caches above us to verify that
        // they don't have a copy of this block in the dirty state
        // at the moment. Without this check we could get a stale
        // copy from memory that might get used in place of the
        // dirty one.
        Packet snoop_pkt(tgt_pkt, true, false);
        snoop_pkt.setExpressSnoop();
        // We are sending this packet upwards, but if it hits we will
        // get a snoop response that we end up treating just like a
        // normal response, hence it needs the MSHR as its sender
        // state
        snoop_pkt.senderState = mshr;
        cpuSidePort.sendTimingSnoopReq(&snoop_pkt);

        // Check to see if the prefetch was squashed by an upper cache (to
        // prevent us from grabbing the line) or if a Check to see if a
        // writeback arrived between the time the prefetch was placed in
        // the MSHRs and when it was selected to be sent or if the
        // prefetch was squashed by an upper cache.

        // It is important to check cacheResponding before
        // prefetchSquashed. If another cache has committed to
        // responding, it will be sending a dirty response which will
        // arrive at the MSHR allocated for this request. Checking the
        // prefetchSquash first may result in the MSHR being
        // prematurely deallocated.
        if (snoop_pkt.cacheResponding()) {
            auto M5_VAR_USED r = outstandingSnoop.insert(snoop_pkt.req);
            assert(r.second);

            // if we are getting a snoop response with no sharers it
            // will be allocated as Modified
            bool pending_modified_resp = !snoop_pkt.hasSharers();
            markInService(mshr, pending_modified_resp);

            DPRINTF(Cache, "Upward snoop of prefetch for addr"
                    " %#x (%s) hit\n",
                    tgt_pkt->getAddr(), tgt_pkt->isSecure()? "s": "ns");
            return false;
        }

        if (snoop_pkt.isBlockCached()) {
            DPRINTF(Cache, "Block present, prefetch squashed by cache.  "
                    "Deallocating mshr target %#x.\n",
                    mshr->blkAddr);

            // Deallocate the mshr target
            if (mshrQueue.forceDeallocateTarget(mshr)) {
                // Clear block if this deallocation resulted freed an
                // mshr when all had previously been utilized
                clearBlocked(Blocked_NoMSHRs);
            }

            // given that no response is expected, delete Request and Packet
            delete tgt_pkt;

            return false;
        }
    }

    return BaseCache::sendMSHRQueuePacket(mshr);
}

Cache*
CacheParams::create()
{
    assert(tags);
    assert(replacement_policy);

    return new Cache(this);
}
