/*
 * Copyright (c) 2012-2018 ARM Limited
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
 *          Steve Reinhardt
 *          Ron Dreslinski
 *          Andreas Hansson
 */

/**
 * @file
 * Describes a cache
 */

#ifndef __MEM_CACHE_CACHE_HH__
#define __MEM_CACHE_CACHE_HH__

#include <cstdint>
#include <map>
#include <unordered_set>

#include "base/types.hh"
#include "mem/cache/base.hh"
#include "mem/ongal_VC.hh"
#include "mem/packet.hh"
#include "mem/ruby/common/ASDT_entry.hh"

using namespace std;


class BasePrefetcher;

class CacheBlk;
struct CacheParams;
class MSHR;

/**
 * A coherent cache that can be arranged in flexible topologies.
 */
class Cache : public BaseCache
{
  protected:
    /**
     * This cache should allocate a block on a line-sized write miss.
     */
    const bool doFastWrites;

    /**
     * Store the outstanding requests that we are expecting snoop
     * responses from so we can determine which snoop responses we
     * generated and which ones were merely forwarded.
     */
    std::unordered_set<RequestPtr> outstandingSnoop;

  protected:
    /**
     * Turn line-sized writes into WriteInvalidate transactions.
     */
    void promoteWholeLineWrites(PacketPtr pkt);

    bool access(PacketPtr pkt, CacheBlk *&blk, Cycles &lat,
                PacketList &writebacks) override;

    #ifdef Ongal_VC
    #ifdef LateMemTrap
    bool access_virtual_cache(PacketPtr pkt, CacheBlk *&blk,
                              Cycles &lat, PacketList &writebacks);
    #endif
    #endif


    void handleTimingReqHit(PacketPtr pkt, CacheBlk *blk,
                            Tick request_time) override;

    void handleTimingReqMiss(PacketPtr pkt, CacheBlk *blk,
                             Tick forward_time,
                             Tick request_time) override;

    void recvTimingReq(PacketPtr pkt) override;

    void doWritebacks(PacketList& writebacks, Tick forward_time) override;

    void doWritebacksAtomic(PacketList& writebacks) override;

    void serviceMSHRTargets(MSHR *mshr, const PacketPtr pkt,
                            CacheBlk *blk) override;

    void recvTimingSnoopReq(PacketPtr pkt) override;

    void recvTimingSnoopResp(PacketPtr pkt) override;

    Cycles handleAtomicReqMiss(PacketPtr pkt, CacheBlk *&blk,
                               PacketList &writebacks) override;

    Tick recvAtomic(PacketPtr pkt) override;

    Tick recvAtomicSnoop(PacketPtr pkt) override;

    //Ongal
    //there seem to be some changes related to synonym related statistics
    //in Hongil's code-base, update later if necessary
    void satisfyRequest(PacketPtr pkt, CacheBlk *blk,
                        bool deferred_response = false,
                        bool pending_downgrade = false) override;

    #ifdef Ongal_VC
    #ifdef LateMemTrap
    bool satisfyCpuSideRequest_VC_Store(PacketPtr pkt, CacheBlk *blk,
                                        bool deferred_response = false,
                                        bool pending_downgrade = false);
    #endif
    #endif



    void doTimingSupplyResponse(PacketPtr req_pkt, const uint8_t *blk_data,
                                bool already_copied, bool pending_inval);

    /**
     * Perform an upward snoop if needed, and update the block state
     * (possibly invalidating the block). Also create a response if required.
     *
     * @param pkt Snoop packet
     * @param blk Cache block being snooped
     * @param is_timing Timing or atomic for the response
     * @param is_deferred Is this a deferred snoop or not?
     * @param pending_inval Do we have a pending invalidation?
     *
     * @return The snoop delay incurred by the upwards snoop
     */
    uint32_t handleSnoop(PacketPtr pkt, CacheBlk *blk,
                         bool is_timing, bool is_deferred, bool pending_inval);

    M5_NODISCARD PacketPtr evictBlock(CacheBlk *blk) override;

    /**
     * Create a CleanEvict request for the given block.
     *
     * @param blk The block to evict.
     * @return The CleanEvict request for the block.
     */
    PacketPtr cleanEvictBlk(CacheBlk *blk);

    PacketPtr createMissPacket(PacketPtr cpu_pkt, CacheBlk *blk,
                               bool needs_writable,
                               bool is_whole_line_write) const override;

    /**
     * Send up a snoop request and find cached copies. If cached copies are
     * found, set the BLOCK_CACHED flag in pkt.
     */
    bool isCachedAbove(PacketPtr pkt, bool is_timing = true);

  public:
    /** Instantiates a basic cache object. */
    Cache(const CacheParams *p);

    set<uint64_t> physical_pages_touched;
    //Ongal (redefinition to use some Cache class related
    //related functionality)
    CacheBlk *handleFill(PacketPtr pkt, CacheBlk *blk,
                         PacketList &writebacks, bool allocate);

    void recvTimingResp(PacketPtr);

    bool BaseCache_access_dup(PacketPtr pkt, CacheBlk *&blk, Cycles &lat,
                                PacketList &writebacks);
#ifdef Ongal_VC

    VC_structure *m_vc_structure; // virtual cache structures

    int64 m_num_accesses;         // num cache access count

    uint64_t prev_cr3;              // track cr3 for previous access
    bool   m_is_l1cache;
    uint64_t m_region_size;

    int m_cache_num_sets;
    int m_cache_assoc;

    VC_structure *get_VC_Structure_ptr(){ return m_vc_structure; }

    void set_is_l1cache( bool _m_is_l1cache){ m_is_l1cache = _m_is_l1cache; }
    bool get_is_l1cache(){ return m_is_l1cache; }

    // Find victims ASDT Set Associative Array
    // Additionally also return the leading virtual page as well as
    // the CR3 value.
    int find_victim_few_lines(int set_index, uint64_t* victim_PPN,
                    uint64_t* victim_VPN, uint64_t* victim_CR3);
    int find_victim_LRU(int set_index, uint64_t* victim_PPN);

    /* unmap_vmas_Handler
       different from Demap_ASDT_Handler.
       based on the current linux implementation,
       cannot get the explicit VPN for the unmapped pages.
       so, flush ASDT entries with non-kernel VPN as Leading virtual pages
       and flush SS/ART
     */
    void unmap_vmas_Handler( PacketList &writebacks){

      // do it only for L1 Virtual Cache
      if ( get_is_l1cache() != true )
        return ;

      // iterating all of the ASDT and keep the virtual page address
      // that is not kernel virtual pages in a set
      std::set<uint64_t> target_virtual_page_address =
        m_vc_structure->find_ASDT_entries_no_kernel();

      // based on the set call Demap_ASDT_Handler
      // parameter should be virtual page address (not virtual page number)
      if (target_virtual_page_address.size() > 0){

        //std::cout<<"unmap_vmas_Handler is called "<<std::endl;

        std::set<uint64_t>::iterator it = target_virtual_page_address.begin();

        for (; it != target_virtual_page_address.end() ; ++it ){
          Demap_ASDT_Handler(*it,writebacks);
          //std::cout<<"-demapped "<<std::hex<<*it<<std::endl;
        }
      }

      // get the entry's PPN and invalidate the entry in the set associative
      // array.
#ifdef ASDT_Set_Associative_Array
#endif
    }


    /* Demap handler
       1. invalidate the corresponding ASDT entry
       2. flush the all ART/SS
       3. do some work for ASDT set associative
       array if it is found with #ifdef ASDT_Set_Associative_Array
     */
    void Demap_ASDT_Handler( uint64_t Vaddr, PacketList &writebacks){

      // do it only for L1 Virtual Cache
      if ( get_is_l1cache() != true )
        return ;

      // calculate the virtual page number (VPN)
      uint64_t VPN = Vaddr / m_vc_structure->get_region_size();
      uint64_t matching_cr3 = 0;
      uint32_t random_number_to_hash_with = 0;
      // find the corresponding ASDT entry with the VPN as the leading virtual
      // page (LVP)
      // invalidate the entry in the map
      // flush ART/SS in the method
      bool found = false;
      vector<int> hash_scheme_for_xor{0};
      do{

        found = m_vc_structure->invalidate_ASDT_with_VPN(VPN, &matching_cr3,
                        &random_number_to_hash_with);

        // invalidate all lines in the page
        if ( found ){


          num_page_info_change++;

          int line_size = m_vc_structure->get_line_size();
          int num_lines = m_vc_structure->get_region_size()/line_size;
          uint64_t region_vaddr = VPN*m_vc_structure->get_region_size();
          //find the random number to xor with.

          uint64_t index_into_hash_lookup_table = (VPN^matching_cr3)&
                  (m_vc_structure->get_hash_lookup_table_size()-1);
         // printf("Index into the hash lookup table is %ld\n",
         // index_into_hash_lookup_table);
          //if the entry in the hash lookup table is valid
  if (m_vc_structure->hash_entry_to_use_getValid(index_into_hash_lookup_table))
          {
            //obtain the hash entry to use.
          uint64_t hash_entry_to_use =
           m_vc_structure->get_hash_entry_to_use(index_into_hash_lookup_table);
            //the hashing function table is always assumed
            //to have a valid entry that can be used.
           int temp = hash_entry_to_use;
           hash_scheme_for_xor = m_vc_structure->
      hashing_function_to_use_get_constant_to_xor_with(temp);

          }
          else
           cout<<"What?? Entry should be valid. In Demap_ASDT_Handler\n";
          // iterating all lines in the PPN
          for ( int line_index = 0 ; line_index < num_lines ; ++line_index ){

            uint64_t line_vaddr = region_vaddr + (line_index*line_size);
            // find a block
            CacheBlk *blk = tags->findBlock_vaddr(line_vaddr, matching_cr3,
                            hash_scheme_for_xor);

            if (blk && blk->isValid()) {
              // if it is dirty, put it in a write back buffer
              if (blk->isDirty()) {
                // Save writeback packet for handling by caller
                writebacks.push_back(writebackBlk(blk));
              }

              // invalidate block
              //
              //
              tags->invalidate_block(blk);
              blk->invalidate();
            }
          }// invalidation of the page from the cache
        }// found matching ASDT entry

      }while ( found );

      // 3.
      // get the entry's PPN and invalidate the entry in the set associative
      // array.
#ifdef ASDT_Set_Associative_Array
#endif
    }

    // ASDT eviction check
    void ASDT_Invalidation_Check( uint64_t Paddr,
                                  PacketList &writebacks){

      // do it only for L1 Virtual Cache
      if ( get_is_l1cache() != true )
        return ;

      // calculate physical page number (PPN)
      uint64_t PPN = Paddr / m_vc_structure->get_region_size();

      // find a corresponding entry in a map
      if ( m_vc_structure->find_matching_ASDT_map( PPN ) ){
        // do nothing
      }else{
        //have a stat to here that tells how many insertions we
        //had into the ASDT.
        num_asdt_insertions++;
        //we are touching this page for the
        //first time.
        if (physical_pages_touched.find(PPN) == physical_pages_touched.end())
        {
          num_unique_pages_referenced++;
          physical_pages_touched.insert(PPN);
        }
        // need to allocate an ASDT SA entry for the PPN

        // check an available entry in the set
        int set_index = m_vc_structure->get_set_index_ASDT_SA(PPN);
        // set index
        int way_index =
                m_vc_structure->find_available_ASDT_SA_entry(set_index);
        // find an empty entry


        if ( way_index == -1 ){
          // no available entry,
          // find victim ASDT entry
          uint64_t victim_PPN;
          //leading VPN, leading CR3
          uint64_t victim_VPN;
          uint64_t victim_CR3;
          //Ongal.
          //currently we don't have aliases, so things are easier.
          //With aliases, might get a lot trickier.
          //currently, for a physical page, we have only a single virtual page
          //that is cached.
          //leading victim VPN and the victim CR3 value.

          //way_index = find_victim_LRU(set_index, &victim_PPN);
          //cout <<"Conflicting PPN is "<<PPN<<endl;
          way_index = find_victim_few_lines(set_index, &victim_PPN,
                                &victim_VPN, &victim_CR3);

          if ( way_index == -1 ){
            // no? victim? then push_back one more entry
            way_index =  m_vc_structure->push_back_ASDT_SA_entry(PPN,
                            set_index);

          }else{


            // invalidation of lines in a cache
            // get the PPN of the victim the entry, did this from find_victim()

            int line_size = m_vc_structure->get_line_size();
            int num_lines = m_vc_structure->get_region_size()/line_size;
            uint64_t region_paddr =
                    victim_PPN*m_vc_structure->get_region_size();

            // iterating all lines in the PPN
            for ( int line_index = 0 ; line_index < num_lines ; ++line_index ){

              uint64_t line_paddr =  region_paddr + (line_index*line_size);
              // find a block

              CacheBlk *blk = tags->findBlock(line_paddr, true);
              if ( !(blk && blk->isValid()) ) {
                blk = tags->findBlock(line_paddr, false);
              }

              if (blk && blk->isValid()) {
                // if it is dirty, put it in a write back buffer
                if (blk->isDirty()) {
                  // Save writeback packet for handling by caller
                  DPRINTF(Cache,"Pushing writeback from ASDT_Invalidation\n");
                  writebacks.push_back(writebackBlk(blk));
                }

                // invalidate tag rather than doing tags->invalidate(blk)
                // why? update_ASDT() could delete a way when current size (due
                // to push_back above) > basic way size.

                // This could result in abort in allocate_ASDT_SA_entry (below)
                // why? this deleted way should have been used as a new entry
                // for the request
                // thus, deleting a way causes an issue: invalidate_block does
                // not call update_ASDT method - purelly invalidating lines

                //Ongal:
                //Update the hash lookup table at this point, because we don't
                //have an
                //ASDT update where we can intercept to update the hash lookup
                //table.
                //Invalidate block
                    //Ongal
                //decrement the number of cache lines at VPN^CR3
                uint64_t index_into_hash_table =
                        ((victim_VPN)^(victim_CR3))&
                        (m_vc_structure->get_hash_lookup_table_size()-1);
               // printf("The index into the hash table is
               // %ld\n",index_into_hash_table);
                //entry should be valid.
      if (!(m_vc_structure->hash_entry_to_use_getValid(index_into_hash_table)))
      {
        cout <<"What?? This entry in the hash lookup table should be valid\n";
        abort();
      }
          else
          {
           //decrement the number of cache lines by one corresponding to this
           //entry in the hash lookup table.
#ifdef Smurthy_debug
              printf("Decrementing number of cache lines for"
                          "entry %lu\n",index_into_hash_table);
#endif
          int temp = index_into_hash_table;
          m_vc_structure->hash_entry_to_use_dec_number_of_cache_lines(temp);

          }

                tags->invalidate_block(blk);
                // invalidate block
                blk->invalidate();
              }
            }

            // invalidate ASDT in a map
            bool result =
                    m_vc_structure->invalidate_matching_ASDT_map(victim_PPN);
            if (!result){
              std::cout<<"ASDT_Invalidation_Check, have to invalidate an ASDT"
                     " entry in a map for invalidated PPN due to lack of"
                     "available entry"<<std::endl;
              abort();
            }

            // no need to set invalid variable for this ASDT entry, as
            // allocate_ASDT_SA_entry (below) will update a particular entry!

          }
        }

        m_vc_structure->allocate_ASDT_SA_entry(PPN, set_index, way_index);
      }
    }

    void Add_NEW_ASDT_map_entry(PacketPtr pkt){

      // do it only for L1 Virtual Cache
      if ( get_is_l1cache() != true )
        return ;

      uint64_t PPN = pkt->getAddr() / m_vc_structure->get_region_size();
#ifdef Smurthy_debug
      printf("Add_NEW_ASDT_map_entry: PPN is %lu\n",
                      PPN);
#endif
      if ( !(m_vc_structure->find_matching_ASDT_map( PPN ))){
        // ADD a new entry to ASDT map
        uint64_t VPN = pkt->req->getVaddr() /
                m_vc_structure->get_region_size();
        uint64_t CR3 = pkt->req->getCR3();
        m_vc_structure->add_new_ASDT_map(PPN, VPN, CR3);
        //upon insertion of the entry, set the corresponding
        //entry in the hash lookup table to valid
        uint64_t index_into_hash_lookup_table = (VPN^CR3)&
        (m_vc_structure->get_hash_lookup_table_size()-1);

#ifdef Smurthy_debug
        printf("Adding new ASDT map entry for Physical address for %lx\n",
                pkt->getAddr());
        printf("Index into the hash lookup table is (Add new ASDT map) %ld\n",
                        index_into_hash_lookup_table);
#endif
        //this entry in the table might have been validated by another
        //page that maps to the same entry in the hash lookup table
        int temp = index_into_hash_lookup_table;
        if (!m_vc_structure->hash_entry_to_use_getValid(temp))
        {
         m_vc_structure->hash_entry_to_use_setValid(temp);
         m_vc_structure->set_hash_entry_to_use_helper(temp);
        }
      }

    }

  // This can be deleted never used.
    void Update_ASDT( uint64_t Vaddr, uint64_t Paddr, uint64_t CR3,
                      bool allocate,
                      Stats::Scalar* num_CPA_change,
                      Stats::Scalar* num_CPA_change_check){

      if ( get_is_l1cache() != true )
        return ;

      Address addr;
      addr.setVaddr(Vaddr);
      addr.setPaddr(Paddr);
      addr.setAddress(Paddr);  // this is important!!
      addr.setCR3(CR3);

      m_vc_structure->update_ASDT( addr, allocate, num_CPA_change,
                      num_CPA_change_check, false);

    }

    // Update Pkt's Virtual Address and CR3 to CPA
    void Update_Pkt_CPA( PacketPtr pkt ){

      if (m_vc_structure != NULL){
        uint64_t Region_Size = m_vc_structure->get_region_size();
        uint64_t PPN = pkt->req->getPaddr()/Region_Size;
        uint64_t CPA_VPN = 0;
        uint64_t CPA_CR3 = 0;
        ASDT_entry * ASDT_entry =
                m_vc_structure->access_matching_ASDT_map(PPN);
        /*
        std::cout<<"Update_Pkt_CPA "
                 <<" Paddr "<<std::hex<<pkt->req->getPaddr()
                 <<" PPN "<<PPN;
        */
        if (ASDT_entry != NULL){
          CPA_VPN = ASDT_entry->get_virtual_page_number();
          CPA_CR3 = ASDT_entry->get_cr3();

          uint64_t CPA_Vaddr = (CPA_VPN * Region_Size) + (pkt->req->getPaddr()
                          % Region_Size);

          /*
          std::cout<<" Pkt_Vaddr "<< pkt->req->getVaddr()
                   <<" Pkt_cr3 "<< pkt->req->getCR3();
          */
          pkt->req->setVaddr((Addr)CPA_Vaddr);
          pkt->req->setCR3(CPA_CR3);
          /*
          std::cout<<" asdt_Vaddr "<< pkt->req->getVaddr()
                   <<" asdt_cr3 "<< pkt->req->getCR3();
          */
        }
        /*
        std::cout<<std::endl;
        */
      }
    }

    void Lookup_VCs( uint64_t Vaddr, uint64_t Paddr, uint64_t CR3, PacketPtr
                    pkt ){

      if ( get_is_l1cache() != true )
        return ;

      ++num_other_approach_access; // increase the num of access

      Address addr;

      addr.setVaddr(Vaddr);
      addr.setPaddr(Paddr);
      addr.setAddress(Paddr);  // this is important!!
      addr.setCR3(CR3);

      // my proposal
      //Update_Pkt_CPA(pkt); // switch non-CPA to CPA by looking up an ASDT
      Lookup_VC_structures( addr, pkt->req->get_store() );

#ifdef Related_Work

      // OVC
      m_vc_structure->OVC->OVC_lookup_vc(addr,&num_OVC_hits);

      // Conventional (TVC)

      if ( prev_cr3 != addr.get_CR3()){
        // flush cache on a context switch
        //m_vc_structure->TVC->flushing_cache();
        //prev_cr3 = addr.get_CR3();
      }

      m_vc_structure->TVC->TVC_lookup_vc(addr,&num_TVC_hits);

      // SLB
      /*
      m_vc_structure->SLB_trap_check(addr,
                                     &num_SLB_traps_8,
                                     &num_SLB_traps_16,
                                     &num_SLB_traps_32,
                                     &num_SLB_traps_48,
                                     &num_SLB_Lookup
                                     );
      */
#endif
    }// end of method

    void Lookup_VC_structures( Address addr, bool store ){

      ++m_num_accesses; // count cache access

      if ( m_vc_structure->is_kernel_space(addr) )
        ++num_kernel_space_access; // count access to the kernel address space

      bool active_synonym_access_non_leading = false;

      m_vc_structure->lookup_VC_structure(addr,
                store,
                &num_active_synonym_access,
                &num_active_synonym_access_with_non_CPA,
                &num_active_synonym_access_store,
                &num_active_synonym_access_with_non_CPA_store,
                &num_active_synonym_access_with_non_CPA_store_ART_miss,
                &num_LCPA_saving_consecutive_active_synonym_access_in_a_page,
                &num_LCPA_saving_ART_hits,
                &num_ss_hits,
                &num_ss_64KB_hits,
                &num_ss_1MB_hits,
                &num_art_hits,
                &num_kernel_space_access_with_active_synonym,
                &active_synonym_access_non_leading);

      if (active_synonym_access_non_leading){
#ifdef Related_Work
        // SLB
        m_vc_structure->SLB_trap_check(addr,
                                       &num_SLB_traps_8,
                                       &num_SLB_traps_16,
                                       &num_SLB_traps_32,
                                       &num_SLB_traps_48,
                                       &num_SLB_Lookup
                                       );
#endif
      }

      // Sampling for Motivation
      if ( (m_num_accesses % Sampling_Interval) == 0 ){
        ++num_samples;
        m_vc_structure->Sampling_for_Motivation(
                                   &num_valid_asdt_entry,
                                   &num_valid_asdt_entry_with_active_synonym,
                                   &num_virtual_pages_with_active_synonym,
                                   &num_max_num_valid_asdt_entry,
                                   &num_valid_asdt_entry_one_line,
                                   &num_valid_asdt_entry_two_lines
                                   );
      }
    }

#endif

    /**
     * Take an MSHR, turn it into a suitable downstream packet, and
     * send it out. This construct allows a queue entry to choose a suitable
     * approach based on its type.
     *
     * @param mshr The MSHR to turn into a packet and send
     * @return True if the port is waiting for a retry
     */
    bool sendMSHRQueuePacket(MSHR* mshr) override;
};

#endif // __MEM_CACHE_CACHE_HH__
