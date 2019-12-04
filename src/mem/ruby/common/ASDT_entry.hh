#ifndef __MEM_RUBY_COMMON_ASDT_entry_HH__
#define __MEM_RUBY_COMMON_ASDT_entry_HH__

#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "base/callback.hh"
#include "base/statistics.hh"
#include "cpu/simple/base.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/common/Other_VCs.hh"
#include "mem/ruby/common/Physical_Mem.hh"
#include "mem/ruby/common/TypeDefines.hh"
#include "sim/core.hh"

//#include "mem/ruby/structures/CacheMemory.hh"
class CacheMemory;

using namespace std;

class hash_function_lookup_table_entry{

public:

 void set_hash_entry_to_use(uint64_t _hash_entry_to_use)
 {
  //this function is called only when
  //this is the first cache line to be inserted
  //for a ASDT entry that we are just inserting.
  hash_entry_to_use_set = true;
  hash_entry_to_use = _hash_entry_to_use;
  notRecycled = false;
  lifetime_record temp;
  //temp.lifetime = curTick();
  temp.lifetime = memRefCommits;
  temp.subtraction_done = false;

  //Ongal
  //to indicate that this entry in the hash
  //lookup table has been used.
  hash_entries_used.at(entry_number) = true;
  //set the epoch id when we have a remap.
  //epoch_id = (curTick()&epoch_id_mask)>>number_of_bits_shift;
  epoch_id = (curTick()&epoch_id_mask)>>number_of_bits_shift;
  epoch_id = 0;
  temp.epoch_id = epoch_id;
//  printf("The epoch id is (set hash entry to use) %lu and entry number is
//  %d\n",epoch_id,entry_number);
  lifetimes_of_hash_entries.at(entry_number).push_back(temp);
#ifdef Smurthy_debug
  printf("Setting hash_entry to use %lu\n",hash_entry_to_use);
#endif
 }
 bool get_notRecycled()
 {
  return  notRecycled;
 }
 void set_notRecycled()
 {
  notRecycled=1;
  return;
 }
 uint64_t get_hash_entry_to_use()
 {
  return  hash_entry_to_use;
 }
 uint64_t get_epoch_id()
 {
  return epoch_id;
 }
 uint64_t get_epoch_id_validity_interval()
 {
  return (1<<number_of_bits_shift);
 }
 void inc_number_of_cache_lines(int total_number_of_hashing_functions)
 {
   if (number_of_cache_lines_using_this_entry == 0)
   {
#ifdef Smurthy_debug
      if (!valid)
        printf("Changing hash function\n");
#endif
      valid = true;
      //need to choose a fresh random number from 0 to total
      //number of hash functions -1 we have.
      if (!hash_entry_to_use_set)
      {
        hash_entry_to_use = (rand()%total_number_of_hashing_functions);
        lifetime_record temp;
        //temp.lifetime = curTick();
        temp.lifetime = memRefCommits;
        temp.subtraction_done = false;

        //Ongal
        //to indicate that this entry in the hash
        //lookup table has been used.


        //set the epoch id
        //epoch_id = (curTick()&epoch_id_mask)>>number_of_bits_shift;
        epoch_id = (curTick()&epoch_id_mask)>>number_of_bits_shift;
        epoch_id = 0;
        //printf("The epoch id is %lu\n",epoch_id);

        hash_entries_used.at(entry_number) = true;
        lifetimes_of_hash_entries.at(entry_number).push_back(temp);
#ifdef Smurthy_debug
        printf("Setting hash_entry to use %lu\n",hash_entry_to_use);
#endif
        hash_entry_to_use_set = true;
      }
   }
   number_of_cache_lines_using_this_entry++;
#ifdef Smurthy_debug
   printf("Number of cache lines for entry %d = %lu\n"
                   ,entry_number,number_of_cache_lines_using_this_entry);
#endif
 }
 void dec_number_of_cache_lines()
 {
   number_of_cache_lines_using_this_entry--;
#ifdef Smurthy_debug
   printf("Number of cache lines for entry %d = %lu\n"
                   ,entry_number,number_of_cache_lines_using_this_entry);
#endif
   if (number_of_cache_lines_using_this_entry == 0)
   {
     //lifetimes_of_hash_entries.at(entry_number).back().lifetime =
     //  curTick()-lifetimes_of_hash_entries.at(entry_number).back().lifetime;
     lifetimes_of_hash_entries.at(entry_number).back().lifetime =
     memRefCommits-lifetimes_of_hash_entries.at(entry_number).back().lifetime;
     //add the epoch id to the lifetime record additionally.
     lifetimes_of_hash_entries.at(entry_number).back().epoch_id =
        epoch_id;
     lifetimes_of_hash_entries.at(entry_number).back().subtraction_done = true;
     valid = false;
     hash_entry_to_use_set = false;
#ifdef Smurthy_debug
     printf("Count of lines mapped to this entry has fallen to"
                     "zero and we need to set the constant again\n");
#endif
   }
 }
 bool getValid()
 {
   return valid;
 }
 void setValid()
 {
   valid = true;
 }
 void invalidate()
 {
   valid = false;
   //this is invalidated, because
   //this entry is no longer valid.
   //We have to set the entry to be used
   //to some other value next time.
   hash_entry_to_use_set = false;
   number_of_cache_lines_using_this_entry = 0;
 }
 void set_entry_number(int _entry_number)
 {
  entry_number = _entry_number;
 }
 void set_epoch_id_mask_and_bits_to_shift(uint64_t _number_of_bits_to_shift,
                 uint64_t _epoch_id_mask)
 {
  number_of_bits_shift = _number_of_bits_to_shift;
  epoch_id_mask = _epoch_id_mask;
 }
private:
 //which entry in the hash function to use for
 //indexing.
 uint64_t hash_entry_to_use;
 //stored along with the hash to be used.
 uint64_t epoch_id;
 bool notRecycled;
 //one time, set when the entry is populated.
 //(could also have these as macros too).
 //below leads to unnecessary duplication.
 uint64_t number_of_bits_shift;
 uint64_t epoch_id_mask;

 //just a flag to indicate if we have
 //set the constant to xor with.
 bool hash_entry_to_use_set;
 //number of cache lines using this
 //entry
 uint64_t number_of_cache_lines_using_this_entry;
 //entry validity bit.
 bool valid;
 //set entry number in the hash lookup table.
 int entry_number;
};

//the 2nd level table holding all the hash constants
//can be an array of structures.
//this table needs to be a lot bigger than the first
//table because a lot of entries from the first table
//might use up the same hash function, and we want to
//use new functions after a particular function has been
//used up.
class hashing_functions_table_entry
{
private:
  vector<int> hash_scheme;
  //uint64_t constant_to_xor_with;
  int count_of_lines_using_entry;
  //when this entry falls to zero, we can use a new
  //hash function from our list of function.
public:
  vector<int> get_constant_to_xor_with(){
    return hash_scheme;
  }
  void set_constant_to_xor_with(vector<int> _set_constant_to_xor_with){
    hash_scheme = _set_constant_to_xor_with;
  }
  void set_of_lines_using_entry(int _number_of_cache_lines){
     count_of_lines_using_entry = _number_of_cache_lines;
  }
  void increment_number_of_lines_using_entry(){
      ++count_of_lines_using_entry;
  }
  void decrement_number_of_lines_using_entry(){
      --count_of_lines_using_entry;
      //TODO: if the count of lines using the entry
      //falls to zero, then we need to use a new
      //hash function from the freeList.
  }
};


class l2_l3_structure{

public:
  l2_l3_structure(string name, uint64_t region_size);

  void set_region_size( uint64_t region_size ){ m_region_size = region_size; }
  uint64_t get_region_size(){ return m_region_size; }
  void set_random_number_to_hash_with(uint32_t random_number)
  { random_number_to_hash_with = random_number; }
  uint32_t get_random_number_to_hash_with(){ return
          random_number_to_hash_with;}
  //hash lookup table operations
  void set_hash_entry_to_use(int index_of_entry,
                  uint64_t _hash_entry_to_use);
  void set_hash_entry_to_use_helper(int index_of_entry);
  uint64_t get_hash_entry_to_use(int index_of_entry);
  bool get_notRecycled(int index_of_entry);
  void set_notRecycled();
  void hash_entry_to_use_inc_number_of_cache_lines(int index_of_entry,int
                  number_of_hashing_functions);
  void hash_entry_to_use_dec_number_of_cache_lines(int index_of_entry);
  bool hash_entry_to_use_getValid(int index_of_entry);
  void hash_entry_to_use_setValid(int index_of_entry);
  void hash_entry_to_use_invalidate(int index_of_entry);

  int get_hash_lookup_table_size();
  void update_hash_table_entry(const Addr addr, bool allocate);
  //hashing functions table entry

  vector<int> hashing_function_to_use_get_constant_to_xor_with(int
                  index_of_entry);
  void hashing_function_to_use_set_constant_to_xor_with(int index_of_entry,
                  vector<int> _set_constant_to_xor_with);
  void hashing_function_to_use_set_of_lines_using_entry(int index_of_entry, int
                  _number_of_cache_lines);
  //pass in the number of hashing functions, so we can pick one out of the
  //n at random.
  void hashing_function_to_use_increment_number_of_lines_using_entry(int
                  index_of_entry);
  void hashing_function_to_use_decrement_number_of_lines_using_entry(int
                  index_of_entry);
  string name(){return m_name;}
private:
  string m_name;
  uint64_t m_region_size;
  uint32_t random_number_to_hash_with;
  //hash_function_lookup_table
  std::vector<hash_function_lookup_table_entry> hash_lookup_table;
  //lookup table size
  int m_hash_lookup_table_size;
  //list of all hashing functions.
  std::vector<hashing_functions_table_entry> list_of_all_hashing_functions;
  //number of hash functions
  int m_size_of_hash_function_list;
};

// data structure of an ASDT map
class ASDT_entry{

public:
  ASDT_entry(uint64_t VPN, uint64_t CR3, int num_lines_per_region, uint64_t
                  lru_count);

  // variables
  // VPN
  void set_virtual_page_number( uint64_t virtual_page_number )
  { m_virtual_page_number = virtual_page_number; }
  uint64_t get_virtual_page_number(){ return m_virtual_page_number; }

  // CR3
  void set_cr3( uint64_t cr3 ){ m_cr3 = cr3; }
  uint64_t get_cr3(){ return m_cr3; }

  // Bit vector
  void clear_bit_vector();
  void set_bit_vector( int index );
  bool unset_bit_vector( int index );
  bool get_bit_vector( int index );

  int get_num_cached_lines(){ return counter; }

  // Synonyms
  int active_synonym_detected(){ return synonym_CR3.size(); }
  void update_active_synonym_vector(uint64_t VPN, uint64_t CR3);

  // LRU
  uint64_t get_LRU(){ return lru; }
  void update_LRU(uint64_t _lru){ lru = _lru; }
  // Write Permission of a leading virtual page
  void set_is_writable_page(bool a){ is_writable_page = a; }
  bool get_is_writable_page(){ return is_writable_page; }

  void set_random_number_to_hash_with(uint32_t random_number)
  { random_number_to_hash_with = random_number; }
  uint32_t get_random_number_to_hash_with(){ return
          random_number_to_hash_with;}
private:

  uint64_t m_virtual_page_number;          // VPN
  uint64_t m_cr3;                          // CR3
  std::vector<bool> cached_bit_vector;   // Bit vector

  int counter;   // line counter
  uint64_t lru;

  std::vector<uint64_t> synonym_CR3;
  std::vector<uint64_t> synonym_VPN;

  uint32_t random_number_to_hash_with;
  bool is_writable_page;
};

// data structure of an ASDT Set-Associative Array.
class ASDT_SA_entry{

public:
  ASDT_SA_entry(){
    m_ppn = 0;
    m_valid = false;
    m_lru = 0;
    m_locked = true;
  }

  uint64_t get_PPN(){ return m_ppn; }
  void set_PPN(uint64_t PPN){ m_ppn = PPN; }

  bool get_valid(){ return m_valid; }
  void set_valid(bool valid){ m_valid = valid; }

  uint64_t get_lru(){ return m_lru; }
  void set_lru(uint64_t LRU){ m_lru = LRU; }

  bool get_lock(){ return m_locked; }
  void set_lock(bool locked){ m_locked = locked; }

private:

  uint64_t m_ppn;     // PPN
  bool   m_valid;   // valid bit
  uint64_t m_lru;     // lru info
  bool   m_locked;    // to prevent eviction of this entry
};

class ART_entry{

public:
  ART_entry(){};
  ART_entry( uint64_t synonym_VPN, uint64_t synonym_CR3,
             uint64_t CPA_VPN, uint64_t CPA_CR3 ){
#ifdef Ongal_debug
    std::cout<<"NEW_ART "<<hex<<" VPN 0x"<<
      synonym_VPN<<
      " CR3 0x"<<synonym_CR3<<
      " CPA_VPN 0x"<<CPA_VPN<<" CPA_CR3 0x"<<CPA_CR3<<endl;
#endif
    set_ART_entry(synonym_VPN, synonym_CR3, CPA_VPN, CPA_CR3);
  }

  ART_entry(const ART_entry& obj){
    *this = obj;
  }
  ART_entry& operator=(const ART_entry& obj){
    if ( this == &obj ){
    }else{
      m_synonym_VPN = obj.m_synonym_VPN;
      m_synonym_CR3 = obj.m_synonym_CR3;
      m_CPA_VPN = obj.m_CPA_VPN;
      m_CPA_CR3 = obj.m_CPA_CR3;
    }
    return *this;
  }

  void set_ART_entry( uint64_t synonym_VPN, uint64_t synonym_CR3, uint64_t
                  CPA_VPN, uint64_t CPA_CR3 ){

    m_synonym_VPN = synonym_VPN;
    m_synonym_CR3 = synonym_CR3;
    m_CPA_VPN = CPA_VPN;
    m_CPA_CR3 = CPA_CR3;
  }

  uint64_t get_synonym_VPN(){ return m_synonym_VPN; }
  uint64_t get_synonym_CR3(){ return m_synonym_CR3; }
  uint64_t get_CPA_VPN(){ return m_CPA_VPN; }
  uint64_t get_CPA_CR3(){ return m_CPA_CR3; }

  void print(){
    std::cout<<"ART "<<hex
             <<" CPA_VPN 0x"<<m_CPA_VPN<<" CPA_CR3 0x"<<m_CPA_CR3
             <<" VPN 0x"<<m_synonym_VPN<< " CR3 0x"<<m_synonym_CR3
             <<endl;
  }

private:
  uint64_t m_synonym_VPN;
  uint64_t m_synonym_CR3;

  uint64_t m_CPA_VPN;
  uint64_t m_CPA_CR3;

};

class SS_entry{

public:
  SS_entry(){
    m_ss_bit = false;
    m_ss_counter = 0;
  }

  void set_ss_bit(){ m_ss_bit = true; }
  void unset_ss_bit(){ m_ss_bit = false; }
  bool get_ss_bit(){ return m_ss_bit; }

  void inc_ss_counter(){ ++m_ss_counter; }
  void dec_ss_counter(){ --m_ss_counter; }
  void set_ss_counter(int num){ m_ss_counter = num; }
  int get_ss_counter(){ return m_ss_counter; }

private:
  bool m_ss_bit;
  int m_ss_counter;
};

class VC_structure{

public:
  VC_structure(string name,
               uint64_t region_size,
               uint64_t line_size,
               int art_size,
               int ss_size,
               int set_size,
               int way_size);

bool lookup_VC_structure( const Address addr,
  bool store,
  Stats::Scalar* num_active_synonym_access,
  Stats::Scalar* num_active_synonym_access_with_non_CPA,
  Stats::Scalar* num_active_synonym_access_store,
  Stats::Scalar* num_active_synonym_access_with_non_CPA_store,
  Stats::Scalar* num_active_synonym_access_with_non_CPA_store_ART_miss,
  Stats::Scalar* num_LCPA_saving_consecutive_active_synonym_access_in_a_page,
  Stats::Scalar* num_LCPA_saving_ART_hits,
  Stats::Scalar* num_ss_hits,
  Stats::Scalar* num_ss_64KB_hits,
  Stats::Scalar* num_ss_1MB_hits,
  Stats::Scalar* num_art_hits,
  Stats::Scalar* num_kernel_space_access_with_active_synonym,
  bool* active_synonym_access_non_leading);

  bool is_kernel_space( const Address addr );
  bool is_kernel_space( const uint64_t addr );
  bool is_pagetable_walk( const Address addr );

  // ASDT
  void add_new_ASDT_map(uint64_t PPN, uint64_t CPA_VPN, uint64_t CPA_CR3);

  // for ruby memory model
  void update_ASDT( const Address addr,
                    bool allocate,
                    Stats::Scalar* num_CPA_change,
                    Stats::Scalar* num_CPA_change_check,
                    bool is_writable_page);

  // for classic memory model
  void update_ASDT( uint64_t Vaddr, uint64_t Paddr, uint64_t CR3,
                    bool allocate,
                    Stats::Scalar* num_CPA_change,
                    Stats::Scalar* num_CPA_change_check,
                    bool is_writable_page){

    Address addr;
    addr.setVaddr(Vaddr);
    addr.setPaddr(Paddr);
    addr.setAddress(Paddr);  // this is important!!
    addr.setCR3(CR3);

    update_ASDT( addr, allocate, num_CPA_change, num_CPA_change_check,
                    is_writable_page);

  }

  // see if the current access is for active synonym
  bool lookup_ASDT( const Address addr,
                    bool *access_to_page_with_active_synonym,
                    uint64_t *CPA_VPN,
                    uint64_t *CPA_CR3);
  bool profile_ASDT( const uint64_t region_tag, const int line_index );

  // ASDT map operations
  bool get_leading_virtual_address(uint64_t Paddr, uint64_t &VPN, uint64_t
                  &CR3);
  bool find_matching_ASDT_map(uint64_t PPN);
  ASDT_entry* access_matching_ASDT_map(uint64_t PPN);
  bool entry_with_active_synonym(uint64_t Paddr);
  bool invalidate_matching_ASDT_map(uint64_t PPN);
  uint64_t inc_LRU_counter(){ return ++ASDT_map_LRU_counter; }
  bool invalidate_ASDT_with_VPN(uint64_t VPN, uint64_t *cr3, uint32_t
                  *random_number_to_hash_with); // for demap operations
  std::set<uint64_t> find_ASDT_entries_no_kernel();

  // ASDT Set Associative Array operations
  int get_set_index_ASDT_SA(uint64_t PPN){ return PPN %
          ASDT_SA_structure.size(); }
  int push_back_ASDT_SA_entry(uint64_t PPN, int set_index);
  // push back a new entry in a set for PPN
  void allocate_ASDT_SA_entry(uint64_t PPN, int set_index, int way_index);
  // allocate a new entry for PPN and lock it until the first line
  void unlock_ASDT_SA_entry(uint64_t PPN);    // unlock a new entry
  void invalid_matching_ASDT_SA_entry(uint64_t PPN);
  // invalidate (set invalid) an ASDT entry for PPN
  int find_available_ASDT_SA_entry(int set_index);
  // search for an invalid (available) entry
  int find_matching_ASDT_SA_entry(int set_index, uint64_t PPN);
  // search for a matching entry for PPN
  int get_num_ways_ASDT_SA(int set_index){ return
          ASDT_SA_structure[set_index].size(); }
  ASDT_SA_entry* access_ASDT_SA_entry(int set_index, int way_index);

  //hash lookup table operations
  void set_hash_entry_to_use(int index_of_entry,
                  uint64_t _hash_entry_to_use);
  void set_hash_entry_to_use_helper(int index_of_entry);
  uint64_t get_hash_entry_to_use(int index_of_entry);
  bool get_notRecycled(int index_of_entry);
  void set_notRecycled();
  uint64_t get_epoch_id_to_use(int index_of_entry);
  uint64_t get_epoch_id_validity_interval_to_use(int index_of_entry);
  void hash_entry_to_use_inc_number_of_cache_lines(int index_of_entry,int
                  number_of_hashing_functions);
  void hash_entry_to_use_dec_number_of_cache_lines(int index_of_entry);
  bool hash_entry_to_use_getValid(int index_of_entry);
  void hash_entry_to_use_setValid(int index_of_entry);
  void hash_entry_to_use_invalidate(int index_of_entry);

  int get_hash_lookup_table_size();
  //hashing functions table entry

  vector<int> hashing_function_to_use_get_constant_to_xor_with(int
                  index_of_entry);
  void hashing_function_to_use_set_constant_to_xor_with(int index_of_entry,
                  vector<int> _set_constant_to_xor_with);
  void hashing_function_to_use_set_of_lines_using_entry(int index_of_entry, int
                  _number_of_cache_lines);
  //pass in the number of hashing functions, so we can pick one out of the
  //n at random.
  void hashing_function_to_use_increment_number_of_lines_using_entry(int
                  index_of_entry);
  void hashing_function_to_use_decrement_number_of_lines_using_entry(int
                  index_of_entry);

  // ART
  bool lookup_ART( const Address addr );
  bool lookup_ART(const uint64_t vaddr,const uint64_t cr3, uint64_t &art_vaddr,
                  uint64_t &art_cr3 );
  void update_ART( const Address addr, uint64_t CPA_VPN, uint64_t CPA_CR3 );
  void delete_ART( uint64_t CPA_VPN, uint64_t CPA_CR3 );
  void print_ART();
  // SS
  void flush_all_SS();
  void update_SS( const uint64_t VPN, const uint64_t CR3, bool add_or_delete );
  bool lookup_SS( const uint64_t VPN, const uint64_t CR3 );
  bool lookup_SS_64KB( const uint64_t VPN, const uint64_t CR3 );
  bool lookup_SS_1MB( const uint64_t VPN, const uint64_t CR3 );

  void Sampling_for_Motivation(Stats::Scalar* num_valid_asdt_entry,
                       Stats::Scalar* num_valid_asdt_entry_with_active_synonym,
                       Stats::Scalar* num_virtual_pages_with_active_synonym,
                       Stats::Scalar* num_max_num_valid_asdt_entry,
                       Stats::Scalar* num_valid_asdt_entry_one_line,
                       Stats::Scalar* num_valid_asdt_entry_two_lines
                       );

  // region and line size
  void set_region_size( uint64_t region_size ){ m_region_size = region_size; }
  uint64_t get_region_size(){ return m_region_size; }

  void set_line_size( uint64_t line_size ){ m_line_size = line_size; }
  uint64_t get_line_size(){ return m_line_size; }

  void set_num_sets(uint64_t num_sets) {m_num_sets = num_sets; }
  uint64_t get_num_sets(){return m_num_sets; }

  void printStats(); // print profile numbers

  // For SLB operation
  // normal synonym check from main memory
  void SLB_trap_check( Address addr,
                       Stats::Scalar* num_SLB_traps_8,
                       Stats::Scalar* num_SLB_traps_16,
                       Stats::Scalar* num_SLB_traps_32,
                       Stats::Scalar* num_SLB_traps_48,
                       Stats::Scalar* num_SLB_traps_Lookup
                       );

  string name(){ return m_name; }

protected:

  //! Callback class used for collating statistics from all the
  //! controller of this type.
  class VC_structure_Callback : public Callback
  {
  private:
    VC_structure *ctr;

  public:
    virtual ~VC_structure_Callback() {}
    VC_structure_Callback(VC_structure *_ctr) : ctr(_ctr) {}
    void process() {ctr->printStats();}
  };

private:

  string m_name;

  // ASDT
  // pointer map for ASDT entries
  std::map<uint64_t, ASDT_entry *> ASDT_structure;
  uint64_t ASDT_map_LRU_counter;

  // Set Associative Array
  std::vector< std::vector< ASDT_SA_entry > > ASDT_SA_structure;
  int m_ASDT_SA_way_size;

  // ART
  // vector for LRU info of ART entries
  std::vector<ART_entry> art_entries;
  // art size
  int m_art_size;

  //hash_function_lookup_table
  std::vector<hash_function_lookup_table_entry> hash_lookup_table;
  //lookup table size
  int m_hash_lookup_table_size;
  //list of all hashing functions.
  std::vector<hashing_functions_table_entry> list_of_all_hashing_functions;
  //number of hash functions
  int m_size_of_hash_function_list;
  // SS
  int m_ss_size;
  std::vector<SS_entry> ss_entries; // 4KB granularity
  std::vector<SS_entry> ss_64KB_entries; // 64KB granularity
  std::vector<SS_entry> ss_1MB_entries; // 1024KB granularity

  // Region
  uint64_t m_region_size;
  uint64_t m_line_size;
  uint64_t m_num_sets;



  // Profiling
  uint64_t m_prev_lookuped_VPN;
  int m_max_num_cached_page;

  // Physical_Page
  Physical_mem m_main_memory;

public:
  // Other virtual caches

  // OVC
  Virtual_Cache *OVC;
  // Conventional Two Level
  Virtual_Cache *TVC;

  // SLBs
  SLB *SLB_8;
  SLB *SLB_16;
  SLB *SLB_32;
  SLB *SLB_48;
};

#endif
