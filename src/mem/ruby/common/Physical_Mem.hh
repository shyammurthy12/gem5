#ifndef __MEM_RUBY_COMMON_PHYSICAL_HH__
#define __MEM_RUBY_COMMON_PHYSICAL_HH__

#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#include "base/callback.hh"
#include "base/statistics.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/common/TypeDefines.hh"

class Physical_mem_entry{

public:
  Physical_mem_entry(){
    m_first_VPN  = 0;
    m_recent_VPN = 0;
    m_synonym    = false;
  }

  void set_first_VPN( uint64_t first_VPN, uint64_t first_cr3 )
  { m_first_VPN = first_VPN; m_first_cr3 = first_cr3; }
  void set_recent_VPN( uint64_t recent_VPN, uint64_t recent_cr3 )
  { m_recent_VPN = recent_VPN; m_recent_cr3 = recent_cr3;}
  void set_synonym( uint64_t synonym ){ m_synonym = synonym; }

  uint64_t get_first_VPN(){ return m_first_VPN; }
  uint64_t get_recent_VPN(){ return m_recent_VPN; }
  uint64_t get_first_cr3(){ return m_first_cr3; }
  uint64_t get_recent_cr3(){ return m_recent_cr3; }
  bool get_synonym(){ return m_synonym; }

private:

  uint64_t m_first_VPN;
  uint64_t m_first_cr3;
  uint64_t m_recent_VPN;
  uint64_t m_recent_cr3;
  bool   m_synonym;

};


class Physical_mem{

public:
  Physical_mem(){
    phy_mem_map.clear();
  }

  void Update_Physical_Page(uint64_t PPN,
                            uint64_t VPN,
                            uint64_t cr3){

    std::map< uint64_t, Physical_mem_entry>::iterator it =
            phy_mem_map.find(PPN);

    if ( it == phy_mem_map.end() ){
      // new physical page
      phy_mem_map[PPN] = Physical_mem_entry();
      phy_mem_map[PPN].set_first_VPN(VPN,cr3);
      phy_mem_map[PPN].set_recent_VPN(0,0);
    }
  }

  void Check_CPA_change(uint64_t PPN,
                   uint64_t VPN,
                   uint64_t cr3,
                   Stats::Scalar* num_CPA_change,
                   Stats::Scalar* num_CPA_change_check ){

    std::map< uint64_t, Physical_mem_entry>::iterator it =
            phy_mem_map.find(PPN);

    if ( it != phy_mem_map.end() ){

      // find the corresponding physical page
      ++(*num_CPA_change_check);

      if ( (it->second.get_recent_VPN() != VPN) ||
                      (it->second.get_recent_cr3() != cr3) ){
        // different CPA was used
        ++(*num_CPA_change);
      }
      // update CPA for later
      it->second.set_recent_VPN(VPN,cr3);
    }
  }

  bool synonym_access_SLB(uint64_t PPN, uint64_t VPN, uint64_t CR3){

    std::map< uint64_t, Physical_mem_entry>::iterator it =
            phy_mem_map.find(PPN);

    // entry is found
    if ( it != phy_mem_map.end() ){

      if ( (it->second.get_first_VPN() != VPN )
          || (it->second.get_first_cr3() != CR3 ))
        return true;

    }

    return false;
  }

private:

  std::map< uint64_t, Physical_mem_entry> phy_mem_map;

};

#endif
