/*
 * Copyright (c) 2012-2014,2017 ARM Limited
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
 * Copyright (c) 2003-2005,2014 The Regents of The University of Michigan
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
 */

/**
 * @file
 * Declaration of a base set associative tag store.
 */

#ifndef __MEM_CACHE_TAGS_BASE_SET_ASSOC_HH__
#define __MEM_CACHE_TAGS_BASE_SET_ASSOC_HH__

#include <functional>
#include <string>
#include <vector>

#include "base/logging.hh"
#include "base/types.hh"
#include "mem/cache/base.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/replacement_policies/base.hh"
#include "mem/cache/replacement_policies/replaceable_entry.hh"
#include "mem/cache/tags/base.hh"
#include "mem/cache/tags/indexing_policies/base.hh"
#include "mem/packet.hh"
#include "params/BaseSetAssoc.hh"

/**
 * A basic cache tag store.
 * @sa  \ref gem5MemorySystem "gem5 Memory System"
 *
 * The BaseSetAssoc placement policy divides the cache into s sets of w
 * cache lines (ways).
 */
class BaseSetAssoc : public BaseTags
{
  protected:
    /** The allocatable associativity of the cache (alloc mask). */
    unsigned allocAssoc;

    /** The cache blocks. */
    std::vector<CacheBlk> blks;

    /** Whether tags and data are accessed sequentially. */
    const bool sequentialAccess;

    /** Replacement policy */
    BaseReplacementPolicy *replacementPolicy;

  public:
    /** Convenience typedef. */
     typedef BaseSetAssocParams Params;

    /**
     * Construct and initialize this tag store.
     */
    BaseSetAssoc(const Params *p);

    /**
     * Destructor
     */
    virtual ~BaseSetAssoc() {};

    /**
     * Initialize blocks as CacheBlk instances.
     */
    void tagsInit() override;

    /**
     * This function updates the tags when a block is invalidated. It also
     * updates the replacement data.
     *
     * @param blk The block to invalidate.
     */
    void invalidate(CacheBlk *blk) override;

    //Ongal
    void invalidate_block(CacheBlk *blk);
    /**
     * Access block and update replacement data. May not succeed, in which case
     * nullptr is returned. This has all the implications of a cache access and
     * should only be used as such. Returns the tag lookup latency as a side
     * effect.
     *
     * @param addr The address to find.
     * @param is_secure True if the target memory space is secure.
     * @param lat The latency of the tag lookup.
     * @return Pointer to the cache block if found.
     */
    CacheBlk* accessBlock(Addr addr, bool is_secure, Cycles &lat) override
    {

        CacheBlk *blk = findBlock(addr, is_secure);

        // Access all tags in parallel, hence one in each way.  The data side
        // either accesses all blocks in parallel, or one block sequentially on
        // a hit.  Sequential access with a miss doesn't access data.
        tagAccesses += allocAssoc;
        if (sequentialAccess) {
            if (blk != nullptr) {
                dataAccesses += 1;
            }
        } else {
            dataAccesses += allocAssoc;
        }

        // If a cache hit
        if (blk != nullptr) {
            // Update number of references to accessed block
            blk->refCount++;

            // Update replacement data of accessed block
            replacementPolicy->touch(blk->replacementData);
        }

        // The tag lookup latency is the same for a hit or a miss
        lat = lookupLatency;

        return blk;
    }




#ifdef Ongal_VC
#ifdef LateMemTrap
    CacheBlk* access_VC_Block(PacketPtr pkt,
                              bool is_secure, Cycles &lat,
                              int context_src)
    {
      // Lookup L1 VCs with only virtual addresses
      CacheBlk* blk = NULL;

      uint64_t vaddr = pkt->req->getVaddr();
      uint64_t cr3   = pkt->req->getCR3();

      blk = findBlock_with_vaddr( (Addr)vaddr, cr3, is_secure);


      // when a valid block is found,
      if (blk != nullptr){
        // a trick, a leading virtual address can be used actually,
        // rather than using a physical address.
        Addr lineaddr_paddr = blk->paddr/getBlockSize();
        lineaddr_paddr *= getBlockSize();
        lineaddr_paddr += vaddr%getBlockSize();

        pkt->req->setPaddr(lineaddr_paddr);
        pkt->setAddr(lineaddr_paddr);

      }

      return blk;
    }
#endif
#endif


    /**
     * Find replacement victim based on address. The list of evicted blocks
     * only contains the victim.
     *
     * @param addr Address to find a victim for.
     * @param is_secure True if the target memory space is secure.
     * @param evict_blks Cache blocks to be evicted.
     * @return Cache block to be replaced.
     */
    CacheBlk* findVictim(Addr addr, const bool is_secure,
                         std::vector<CacheBlk*>& evict_blks) const override
    {
#ifdef Ongal_VC
#ifdef VIVT
     if (get_VC_structure() != NULL){

       // No Corresponding ASDT entry?
       uint64_t Region_Size = get_VC_structure()->get_region_size();
       uint64_t PPN = addr/Region_Size;
       ASDT_entry * ASDT_entry =
               get_VC_structure()->access_matching_ASDT_map(PPN);

       if (ASDT_entry == NULL){
         return NULL;
       }else{
         Addr CPA_VPN   = ASDT_entry->get_virtual_page_number();
         Addr CPA_Vaddr = (CPA_VPN * Region_Size) + (addr % Region_Size);
         uint32_t random_number_to_xor_with = 0;
         uint64_t CPA_CR3 = ASDT_entry->get_cr3();

         uint64_t index_into_hash_lookup_table = (CPA_VPN^CPA_CR3)&
                  (m_vc_structure->get_hash_lookup_table_size()-1);

#ifdef Smurthy_debug
         printf("Index into the hash lookup table(findVictim) is %ld\n",
                         index_into_hash_lookup_table);
#endif
         //if the entry in the hash lookup table is valid
         int temp;
         if (get_VC_structure()->hash_entry_to_use_getValid(temp))
         {
           //obtain the hash entry to use.
           uint64_t hash_entry_to_use =
                   get_VC_structure()->get_hash_entry_to_use(temp);
           //the hashing function table is always assumed
           //to have a valid entry that can be used.
   random_number_to_xor_with =
    get_VC_structure()->hashing_function_to_use_get_constant_to_xor_with(temp);

         }
         //absence of a valid entry, indicates
         //a miss in the cache.
         else
         {
            cout<<"What??. The entry should be valid when in findVictim()\n";
            abort();
         }
         #ifdef Smurthy_debug
         printf("Find victim with addr: %lx vtag: %lx, cr3: %lu and Paddr:"
                         "%lx\n",CPA_Vaddr,
                         extractTag(CPA_Vaddr),
                         CPA_CR3, addr);
         #endif

         const std::vector<ReplaceableEntry*> entries =
         indexingPolicy->getPossibleEntries_with_Vaddr(CPA_Vaddr,
                        random_number_to_xor_with);

        // Choose replacement victim from replacement candidates
        CacheBlk* victim = static_cast<CacheBlk*>(replacementPolicy->getVictim(
                                entries));

        // There is only one eviction for this replacement
        evict_blks.push_back(victim);

        return victim;
      }
    }
#endif
#endif
        // Get possible entries to be victimized
        const std::vector<ReplaceableEntry*> entries =
            indexingPolicy->getPossibleEntries(addr);

        // Choose replacement victim from replacement candidates
        CacheBlk* victim = static_cast<CacheBlk*>(replacementPolicy->getVictim(
                                entries));

        // There is only one eviction for this replacement
        evict_blks.push_back(victim);

        return victim;
    }

    /**
     * Insert the new block into the cache and update replacement data.
     *
     * @param pkt Packet holding the address to update
     * @param blk The block to update.
     */
    void insertBlock(const PacketPtr pkt, CacheBlk *blk) override
    {
        // Insert block
        BaseTags::insertBlock(pkt, blk);

        // Increment tag counter
        tagsInUse++;

        // Update replacement policy
        replacementPolicy->reset(blk->replacementData);
    }

    /**
     * Limit the allocation for the cache ways.
     * @param ways The maximum number of ways available for replacement.
     */
    virtual void setWayAllocationMax(int ways) override
    {
        fatal_if(ways < 1, "Allocation limit must be greater than zero");
        allocAssoc = ways;
    }

    /**
     * Get the way allocation mask limit.
     * @return The maximum number of ways available for replacement.
     */
    virtual int getWayAllocationMax() const override
    {
        return allocAssoc;
    }

    /**
     * Regenerate the block address from the tag and indexing location.
     *
     * @param block The block.
     * @return the block address.
     */
    Addr regenerateBlkAddr(const CacheBlk* blk) const override
    {
        return indexingPolicy->regenerateAddr(blk->tag, blk);
    }

    void forEachBlk(std::function<void(CacheBlk &)> visitor) override {
        for (CacheBlk& blk : blks) {
            visitor(blk);
        }
    }

    bool anyBlk(std::function<bool(CacheBlk &)> visitor) override {
        for (CacheBlk& blk : blks) {
            if (visitor(blk)) {
                return true;
            }
        }
        return false;
    }
};

#endif //__MEM_CACHE_TAGS_BASE_SET_ASSOC_HH__
