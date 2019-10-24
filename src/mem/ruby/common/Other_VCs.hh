#ifndef __MEM_RUBY_COMMON_Other_VC_HH__
#define __MEM_RUBY_COMMON_Other_VC_HH__

#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#include "base/callback.hh"
#include "base/statistics.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/common/TypeDefines.hh"

class VC_entry{

public:

  VC_entry(){

    m_tag   = 0;
    m_vtag  = 0;
    m_cr3   = 0;
    m_valid = false;
    m_lru   = 0;
  }

  void set_tag(uint64_t _tag){ m_tag = _tag; }
  void set_vtag(uint64_t _vtag){ m_vtag = _vtag; }
  void set_cr3(uint64_t _cr3){ m_cr3 = _cr3; }
  void set_valid(bool _valid){ m_valid = _valid; }
  void set_lru(int64 _lru){ m_lru = _lru; }

  uint64_t get_tag(){ return m_tag; }
  uint64_t get_vtag(){ return m_vtag; }
  uint64_t get_cr3(){ return m_cr3; }
  bool get_valid(){ return m_valid; }
  int64 get_lru(){ return m_lru; }

private:

  uint64_t m_tag;
  uint64_t m_vtag;
  uint64_t m_cr3;
  bool   m_valid;
  int64 m_lru;
};

class Virtual_Cache{

public:

  Virtual_Cache();

  Virtual_Cache(int set, int way, int line_size){

    m_line_size = line_size;
    m_set_size = set;
    m_assoc_size = way;

    m_prev_cr3 = 0;
    m_prev_line = 0;

    cache_array.clear();

    int index = 0;

    for ( ; index < set ; ++index ){
      cache_array.push_back( std::vector<VC_entry>() );
      cache_array[cache_array.size()-1].assign(way,VC_entry());
    }

    std::cout<<"VC set: "<<cache_array.size();
    if (cache_array.size() > 0 )
      std::cout<<" VC assoc: "<<cache_array[0].size();
    std::cout<<std::endl;

  }

  void flushing_cache(){

    int set = cache_array.size();
    int way = 0;

    if ( set > 0 ){
      way = cache_array[0].size();

      int set_index = 0;
      int way_index = 0;

      for ( ; set_index < set ; ++set_index )
        for ( ; way_index < way ; ++way_index )
          cache_array[set_index][way_index].set_valid(false);

    }
  }

  int get_line_size(){ return m_line_size; }

  void OVC_lookup_vc( Address addr,
                  Stats::Scalar* Cache_Hits);

  void TVC_lookup_vc( Address addr,
                  Stats::Scalar* Cache_Hits);

  bool is_kernel_space( const Address addr );

private:

  std::vector< std::vector<VC_entry> > cache_array;

  int m_line_size;
  int m_set_size;
  int m_assoc_size;

  uint64_t m_prev_cr3;
  uint64_t m_prev_line;

};

class SLB_entry{

public:
  SLB_entry(){
    m_primary_VPN = 0;
    m_primary_CR3 = 0;
    m_secondary_VPN = 0;
    m_secondary_CR3 = 0;
  }

  SLB_entry( uint64_t _primary_VPN,
              uint64_t _primary_CR3,
              uint64_t _secondary_VPN,
              uint64_t _secondary_CR3){
    m_primary_VPN = _primary_VPN;
    m_primary_CR3 = _primary_CR3;
    m_secondary_VPN = _secondary_VPN;
    m_secondary_CR3 = _secondary_CR3;
  }

  uint64_t get_secondary_VPN(){ return m_secondary_VPN; }
  uint64_t get_secondary_CR3(){ return m_secondary_CR3; }
  uint64_t get_primary_VPN(){ return m_primary_VPN; }
  uint64_t get_primary_CR3(){ return m_primary_CR3; }

private:
  uint64_t m_primary_VPN;
  uint64_t m_primary_CR3;
  uint64_t m_secondary_VPN;
  uint64_t m_secondary_CR3;
};

class SLB{


public:
  SLB( int _max_size ){
    m_SLB.clear();
    m_max_size = _max_size;
  }


  // this method is called only when synonym access is detected.

  void SLB_lookup( uint64_t secondary_VPN,
                   uint64_t secondary_CR3,
                   uint64_t primary_VPN,
                   uint64_t primary_CR3,
                   Stats::Scalar* SLB_trap_counter){

    std::vector< SLB_entry >::reverse_iterator it = m_SLB.rbegin();
    int index = 0;

    for ( ; it != m_SLB.rend() ; ++it, ++index ){

      // found?
      if ( (it->get_secondary_VPN() == secondary_VPN)
          && ( it->get_secondary_CR3() == secondary_CR3 ) ){

        if ( it != m_SLB.rbegin() ){
          // Set as an MRU entry
          uint64_t primary_VPN = it->get_primary_VPN();
          uint64_t primary_CR3 = it->get_primary_CR3();
          m_SLB.erase( m_SLB.begin() + (m_SLB.size() -1 -index) );
          // delete the entry
          // put it at the end again (MRU position)
          m_SLB.push_back(SLB_entry(primary_VPN,
                                  primary_CR3,secondary_VPN,secondary_CR3));
        }

        return;
      }
    }

    // here? no corresponding entry

    // then increase the counter
    ++(*SLB_trap_counter);

    if ( m_SLB.size() >= m_max_size ){

      // need to evict one of entries. (LRU entry)
      m_SLB.erase( m_SLB.begin() );

      // RANDOM
      // int rand_num = rand();
      // m_SLB.erase( m_SLB.begin() + (rand_num % m_SLB.size()) );
    }

    // add new entry
    m_SLB.push_back(SLB_entry(primary_VPN,primary_CR3,
                            secondary_VPN,secondary_CR3));
  }


private:

  int m_max_size;
  std::vector< SLB_entry > m_SLB;
};

#endif
