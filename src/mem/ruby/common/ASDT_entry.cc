#include "mem/ruby/common/ASDT_entry.hh"
using namespace std;
std::vector<std::vector<lifetime_record>> lifetimes_of_hash_entries;
std::vector<bool> hash_entries_used;

// Function to return the next random number
int getNum(vector<int>& v)
{

    // Size of the vector
    int n = v.size();

    // Make sure the number is within
    // the index range
    int index = rand() % n;

    // Get random number from the vector
    int num = v[index];

    // Remove the number from the vector
    swap(v[index], v[n - 1]);
    v.pop_back();

    // Return the removed number
    return num;
}

// Function to generate n non-repeating random numbers
vector<int> generateRandom(int n)
{
    vector<int> v(n);
    vector<int> scheme;
    // Fill the vector with the values
    // 1, 2, 3, ..., n
    for (int i = 0; i < n; i++)
        v[i] = i + 1;

    // get a random number from the vector and print it
    for (int i=0;i<9;i++) {
        scheme.push_back(getNum(v)+14);
    }
    return scheme;
}

ASDT_entry::ASDT_entry(uint64_t VPN, uint64_t CR3, int num_lines_per_region,
                uint64_t lru_count){

  // init counter for counting the number of cached line for this entry
  counter = 0;

  set_virtual_page_number( VPN );

  //generate a random number to xor with
  //limit the random numbers between
  //0 and 99 for now. Later, change this.
  uint32_t random_number = rand()%100;
  set_random_number_to_hash_with(random_number);


  // set cr3 value
  set_cr3( CR3 );

  // initialize a cached_bit_vector
  cached_bit_vector.clear();
  cached_bit_vector.assign( num_lines_per_region , false );

  synonym_CR3.clear();
  synonym_VPN.clear();

  update_LRU(lru_count);

#ifdef Ongal_debug
  std::cout<<"New ASDT_entry "<<hex<<" CPA_VPN 0x"<<VPN<<" CPA_CR3 0x"<<CR3<<
    " num_max_lines "<<dec<< num_lines_per_region<<endl;
#endif
}

void
ASDT_entry::clear_bit_vector(){
        cached_bit_vector.clear();
}



void
ASDT_entry::set_bit_vector( int index ){

  if ( cached_bit_vector[index] || index >= cached_bit_vector.size()){
    std::cout<<"set_bit_vector "<<dec<<" index "<<index<<" Size_of_vector"
            <<cached_bit_vector.size()<<" cached?"
            <<cached_bit_vector[index]<<" Ignore this classic model"
            <<std::endl; abort();
  }else{
    cached_bit_vector[index] = true;
    ++counter;
  }
#ifdef Ongal_debug
  std::cout<<"set bit vector "<<index<<" line index, there are "<<counter<<
          "cached lines "<<endl;
#endif
}

bool
ASDT_entry::unset_bit_vector( int index ){

  if ( !(cached_bit_vector[index]) || index >= cached_bit_vector.size()){
    std::cout<<"unset_bit_vector "<<index<<std::endl;
    abort();
  }else{
    cached_bit_vector[index] = false;
    counter--;
  }

#ifdef Ongal_debug
  std::cout<<"unset bit vector "<<index<<" line index, there are "<<counter
          "cached lines "<<endl;
#endif
  // return info showing whether the region for this entry has cached
  // lines...in a virtual cache
  if (counter == 0)
    return true;
  else
    return false;
}

bool
ASDT_entry::get_bit_vector( int index ){
  return cached_bit_vector[index];
}

void
ASDT_entry::update_active_synonym_vector(uint64_t VPN, uint64_t CR3){

  int i = synonym_CR3.size();
  int j = synonym_VPN.size();

  if ( i != j ){
    cout<<"update_active_synonym_vector"<<endl;
    exit(0);
  }

  for ( i = 0 ; i < synonym_CR3.size() ; ++i ){

    // the same pair is found
    if (VPN == synonym_VPN[i] && CR3 == synonym_CR3[i])
      return;
  }

  // new pair will be added
  synonym_CR3.push_back(CR3);
  synonym_VPN.push_back(VPN);
}


VC_structure::VC_structure(string name,
                           uint64_t region_size,
                           uint64_t line_size,
                           int art_size,
                           int ss_size,
                           int set_size,
                           int way_size){

  std::cout<<"init_VC structure "<<region_size<<" region size "<<
          line_size<<" line size "<<std::endl;

  m_name = name;
  m_name.append(".VC_structure");

  bool dcache = false;

  if ( m_name.find("dcache") != string::npos )
  {
    //Ongal
    //setting the seed for us to perform the
    //xor with.

    srand(0);
    dcache = true;
  }

  // set region and line size
  set_region_size(region_size);
  set_line_size(line_size);
  set_num_sets(set_size);

  std::cout<<"ASDT_structure is cleared"<<std::endl;
  ASDT_structure.clear(); //init

  // ASDT Set Associative Array init
  int asdt_set = 0;
  int asdt_way = 0;

  if (dcache){
    asdt_set = 16;
    asdt_way = 16;
    //have this hash lookup table for
    //data cache alone.
    m_hash_lookup_table_size = 32 ;
    m_size_of_hash_function_list = 64;
  }else{
    asdt_set = 8;
    asdt_way = 16;
    m_hash_lookup_table_size = 0;
    m_size_of_hash_function_list = 0;
  }

  m_ASDT_SA_way_size = asdt_way;

  for ( int i = 0; i < asdt_set ; ++i ){
    std::vector< ASDT_SA_entry > temp;
    temp.assign(asdt_way, ASDT_SA_entry());
    ASDT_SA_structure.push_back(temp);
  }
  lifetimes_of_hash_entries.resize(m_hash_lookup_table_size);
  hash_entries_used.resize(m_hash_lookup_table_size);

  //tunable parameters.
  int epoch_rate = 12; //in microseconds
  int number_of_bits_to_use_for_epoch_id = 3;

  int epoch_id_mask_before_shifting;
  int epoch_id_mask;

  //multiply epoch rate by 10 to the power 6 because the one clock cycle has
  //1000 ticks
  //and the processor frequency in 1Ghz and hence 1 microsecond corresponds to
  //10 to the power 6 clock ticks.
  int number_of_bits_to_shift = log(float(epoch_rate)*pow(10,6))/log(2.0);
  cout<<"Number of bits needed to represent " <<number_of_bits_to_shift<<endl;

  for (int i = 0;i<m_hash_lookup_table_size;i++)
    hash_entries_used.at(i) = false;
  for (int i = 0; i<m_hash_lookup_table_size; i++){
    hash_function_lookup_table_entry temp;
    //invalidate this entry.Made valid when referenced
    //first time and assigned a hash function to use.
    temp.invalidate();
    //set the entry number in the hash lookup
    //table.
    temp.set_entry_number(i);
    epoch_id_mask_before_shifting =
            pow(2,number_of_bits_to_use_for_epoch_id)-1;
    epoch_id_mask = (epoch_id_mask_before_shifting<<(number_of_bits_to_shift));
    temp.set_epoch_id_mask_and_bits_to_shift(number_of_bits_to_shift,
                    epoch_id_mask);
    hash_lookup_table.push_back(temp);
  }
  std::cout<<"Hash function lookup table size is "<<
          hash_lookup_table.size();
  //for all entries in the list of hash functions, assign
  //a random constant to xor with. Figure out how we can do this.
  for (int i = 0;i<m_size_of_hash_function_list;i++)
  {
     hashing_functions_table_entry temp;
     //9-ints for hash scheme
     vector<int> scheme = generateRandom(17);
     temp.set_constant_to_xor_with(scheme);
     cout<<"Random numbers used for " <<i<<": "<<endl;
     vector<int> scheme_ = temp.get_constant_to_xor_with();
     for (int i=0; i<scheme_.size(); i++)
        cout << scheme_[i] << endl;
     temp.set_of_lines_using_entry(0);
     list_of_all_hashing_functions.push_back(temp);
  }
  std::cout<<"Hashing functions table entry size is "<<
          list_of_all_hashing_functions.size();
  std::cout<<"ASDT_SA Set: "<<ASDT_SA_structure.size();
  if (ASDT_SA_structure.size()>0){
    std::cout<<" Way: "<<ASDT_SA_structure[0].size()<<std::endl;
  }else{
    std::cout<<std::endl;
  }

  ASDT_map_LRU_counter = 0;

  // ART
  m_art_size = art_size;

  // SS
  m_ss_size = ss_size;
  ss_entries.assign(m_ss_size, SS_entry());
  ss_64KB_entries.assign(m_ss_size, SS_entry());
  ss_1MB_entries.assign(m_ss_size, SS_entry());

  // Profiling
  m_prev_lookuped_VPN   = 0;
  m_max_num_cached_page = 0;

  // OVC
  std::cout<<"OVC Setting: "<<ASDT_SA_structure.size()<<" ";
  OVC = new Virtual_Cache(set_size,way_size,line_size);
  // TVC
  std::cout<<"TVC Setting: "<<ASDT_SA_structure.size()<<" ";
  TVC = new Virtual_Cache(set_size,way_size,line_size);

  // SLBs
  std::cout<<"SLB Setting (8,16,32, and 48) Entries ";
  SLB_8 = new SLB(8);
  SLB_16 = new SLB(16);
  SLB_32 = new SLB(32);
  SLB_48 = new SLB(48);

  //Stats::registerDumpCallback(new VC_structure_Callback(this));
}

//Ongal
//define all the hash table lookup and hash function table functions
//here, were it is the same as in the respective class.
//However, we additionally pass the entry index as a parameter.
void
VC_structure::set_hash_entry_to_use(int index_of_entry, uint64_t
                _hash_entry_to_use)
{
   hash_function_lookup_table_entry &temp =
           hash_lookup_table.at(index_of_entry);
   return temp.set_hash_entry_to_use(_hash_entry_to_use);
}

int
VC_structure::get_hash_lookup_table_size()
{
 return m_hash_lookup_table_size;
}


void VC_structure::set_hash_entry_to_use_helper(int index_of_entry){

        int random_number_to_xor_with = rand()%m_size_of_hash_function_list;
#ifdef Smurthy_debug
        printf("Setting random number to xor (%d) for index (%d)\n",
                        random_number_to_xor_with,index_of_entry);
#endif
        return set_hash_entry_to_use(index_of_entry,
                        random_number_to_xor_with);
}

uint64_t
VC_structure::get_hash_entry_to_use(int index_of_entry)
{
  return hash_lookup_table.at(index_of_entry).get_hash_entry_to_use();
}


uint64_t
VC_structure::get_epoch_id_to_use(int index_of_entry)
{
  return hash_lookup_table.at(index_of_entry).get_epoch_id();
}


uint64_t
VC_structure::get_epoch_id_validity_interval_to_use(int index_of_entry)
{
  return hash_lookup_table.at(index_of_entry).get_epoch_id_validity_interval();
}



void
VC_structure::hash_entry_to_use_inc_number_of_cache_lines(int
                index_of_entry,int number_of_hashing_functions)
{
  int temp = index_of_entry;
  int temp1 = number_of_hashing_functions;
  return hash_lookup_table.at(temp).inc_number_of_cache_lines(temp1);
}
void
VC_structure::hash_entry_to_use_dec_number_of_cache_lines(int index_of_entry)
{
  return hash_lookup_table.at(index_of_entry).dec_number_of_cache_lines();
}
bool
VC_structure::hash_entry_to_use_getValid(int index_of_entry)
{
  return hash_lookup_table.at(index_of_entry).getValid();
}
void
VC_structure::hash_entry_to_use_setValid(int index_of_entry)
{
  return hash_lookup_table.at(index_of_entry).setValid();
}
void
VC_structure::hash_entry_to_use_invalidate(int index_of_entry)
{
  return hash_lookup_table.at(index_of_entry).invalidate();
}
vector<int>
VC_structure::hashing_function_to_use_get_constant_to_xor_with(int
                index_of_entry){
   hashing_functions_table_entry &temp =
           list_of_all_hashing_functions.at(index_of_entry);
   return temp.get_constant_to_xor_with();
  }
void
VC_structure::hashing_function_to_use_set_constant_to_xor_with(int
                index_of_entry, vector<int> _set_constant_to_xor_with){
  hashing_functions_table_entry &temp =
          list_of_all_hashing_functions.at(index_of_entry);
  return temp.set_constant_to_xor_with(_set_constant_to_xor_with);
}
void
VC_structure::hashing_function_to_use_set_of_lines_using_entry(int
                index_of_entry, int  _number_of_cache_lines){

  hashing_functions_table_entry &temp =
          list_of_all_hashing_functions.at(index_of_entry);
  return temp.set_of_lines_using_entry(_number_of_cache_lines);
}

void
VC_structure::hashing_function_to_use_increment_number_of_lines_using_entry(int
                index_of_entry){

    hashing_functions_table_entry &temp =
            list_of_all_hashing_functions.at(index_of_entry);
   return temp.increment_number_of_lines_using_entry();
}
void
VC_structure::hashing_function_to_use_decrement_number_of_lines_using_entry(int
                index_of_entry){
    hashing_functions_table_entry &temp =
            list_of_all_hashing_functions.at(index_of_entry);
    return temp.decrement_number_of_lines_using_entry();
}



void
VC_structure::update_ASDT( const Address addr,
                           bool allocate,
                           Stats::Scalar* num_CPA_change,
                           Stats::Scalar* num_CPA_change_check,
                           bool is_writable_page){

  // physical page number (PPN)
  uint64_t PPN = addr.getAddress() / get_region_size();
  int line_index = (addr.getAddress() % get_region_size())/get_line_size();
#ifdef Smurthy_debug
  printf("The PPN is %ld and address is %lld\n",PPN,addr.getAddress());
  printf("The line index is %d\n",line_index);
  if (allocate)
    printf("We have an allocate\n");
  else
    printf("No allocate\n");
#endif

  // search for the corresponding ASDT entry with addr
  std::map<uint64_t, ASDT_entry *>::iterator it =
    ASDT_structure.find(PPN);

  bool found_matching_entry = false;

  if (it != ASDT_structure.end())
    found_matching_entry = true;

#ifdef Ongal_debug
  std::cout<<"update_ASDT "<<std::hex
          <<addr<<" allocate? "<<allocate<<" PPN "<<hex<<"0x"<<PPN<<
          " line_index "<<dec<<line_index<<" matching_entry? "
          <<found_matching_entry<<endl;
#endif

  if ( allocate ){

    // when a new cache line is allocated

    if (!(found_matching_entry)){

      // new region entry
      // calculate VPN based on the region_size
      uint64_t VPN = addr.get_Vaddr() / get_region_size();

#ifdef Ongal_debug
      std::cout<<" NEW ASDT PPN "<<std::hex<<PPN
               <<" VPN "<<std::hex<<VPN
               <<" CR3 "<<addr.get_CR3()<<std::endl;
#endif
#ifdef Smurthy_debug
      printf("Added new ASDT map for PPN: %lx and"
                      "cleared bit vector\n",PPN);
#endif
      add_new_ASDT_map(PPN, VPN, addr.get_CR3());
    }

    #ifdef ASDT_Set_Associative_Array
    // an entry is already allocated for this PPN
    // now, unlock the entry to keep track of the line information
    unlock_ASDT_SA_entry(PPN);
    #endif

    //Ongal
    //increment the number of cache lines at VPN^CR3
    uint64_t index_into_hash_table =
            ((it->second->get_virtual_page_number())^
            (it->second->get_cr3()))&(m_hash_lookup_table_size-1);

    //Ongal
    //decrement the number of cache lines at VPN^CR3
    //entry in the hash lookup table.
#ifdef Smurthy_debug
    printf("Incrementing number of cache lines for"
                    "entry %lu\n",index_into_hash_table);
#endif
    hash_entry_to_use_inc_number_of_cache_lines(index_into_hash_table,
                m_size_of_hash_function_list);

    // set the corresponding bit
    ASDT_structure[PPN]->set_bit_vector(line_index);
    ASDT_structure[PPN]->update_LRU(inc_LRU_counter());
    ASDT_structure[PPN]->set_is_writable_page(is_writable_page);

  }else{

    // when a line is evicted, so the corresponding entry should exist
    if (!(found_matching_entry)){
      cout<<"update_ASDT, no matching entry... what?"<<endl;
      abort();
    }

    // unset bit
    bool empty = false;
    empty = it->second->unset_bit_vector(line_index);

    //Ongal
    //decrement the number of cache lines at VPN^CR3
    uint64_t index_into_hash_table =
            ((it->second->get_virtual_page_number())^
            (it->second->get_cr3()))&(m_hash_lookup_table_size-1);
   // printf("The index into the hash table is %ld\n",index_into_hash_table);
    //entry should be valid.
    if (!(hash_entry_to_use_getValid(index_into_hash_table)))
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
        hash_entry_to_use_dec_number_of_cache_lines(index_into_hash_table);

    }

    if (empty){
      // no more corresponding lines reside in a cache
      // let's deallocate this ASDT entry.
      // before doing this, first, let's de-allocate the corresponding info in
      // ART and SS

#ifdef Ongal_debug
      std::cout<<std::endl;
      std::cout<<name()<<" update_ASDT "<<addr
              <<" delete ASDT entry "
              "if no cached lines --> clear ART entries as well CPA_VPN 0x"
               <<hex<<it->second->get_virtual_page_number()<<" CPA_CR3 0x"
               <<it->second->get_cr3()<<" "
               <<endl;
#endif

      delete_ART(it->second->get_virtual_page_number(),
                 it->second->get_cr3());

      // CPA change analysis
      // only for cases where active synonyms have been detected,
      // if yes? set synonym bit for physical page
      if ( it->second->active_synonym_detected() > 0 ){

        m_main_memory.Check_CPA_change(PPN,
                        it->second->get_virtual_page_number(),
                                       it->second->get_cr3(),
                                       num_CPA_change,num_CPA_change_check);
      }

      // delete ASDT entry
      //delete it->second;
      ASDT_structure.erase(it);

      #ifdef ASDT_Set_Associative_Array
      // delete ASDT entry in set associative array
      invalid_matching_ASDT_SA_entry(PPN);
      #endif
    }
  }
  return;
}

bool
VC_structure::find_matching_ASDT_map(uint64_t PPN){

  // search for the corresponding ASDT entry with addr
  std::map<uint64_t, ASDT_entry *>::iterator it = ASDT_structure.find(PPN);

  if (it != ASDT_structure.end())
    return true;
  else
    return false;

}

ASDT_entry*
VC_structure::access_matching_ASDT_map(uint64_t PPN){

  // search for the corresponding ASDT entry with addr
  std::map<uint64_t, ASDT_entry *>::iterator it = ASDT_structure.find(PPN);

  if (it != ASDT_structure.end())
    return it->second;
  else
    return NULL;
}

bool
VC_structure::get_leading_virtual_address(uint64_t Paddr, uint64_t &VPN,
                uint64_t &CR3){

  uint64_t PPN = Paddr / get_region_size();

  /*
  std::cout<<name()<<" get_leading_vaddr PPN 0x"<<std::hex<<PPN<<std::endl;
  std::cout<<name()<<" ALL ASDT entries"<<std::endl;

  std::map<uint64_t, ASDT_entry *>::iterator it = ASDT_structure.begin();

  for (; it != ASDT_structure.end() ; ++it)
    std::cout<<std::hex<<" PPN "<<it->first
             <<" CPA VADDR "<<it->second->get_virtual_page_number()
             <<" CPA CR3 "<<it->second->get_cr3()
             <<std::endl;
  std::cout<<std::endl;
  */

  ASDT_entry* entry = access_matching_ASDT_map(PPN);
  if ( entry != NULL ){
    VPN = entry->get_virtual_page_number();
    CR3 = entry->get_cr3();

    if (VPN == 0 || CR3 == 0){
      std::cout<<"ASDT hit but VPN or CR3 0"<<std::endl;
      abort();
    }
    return true;
  }
  return false; // no matching ASDT, so CPA will be the request CLA
}

bool
VC_structure::entry_with_active_synonym(uint64_t Paddr){
  uint64_t PPN = Paddr / get_region_size();

  ASDT_entry* entry = access_matching_ASDT_map(PPN);
  if ( entry == NULL )
    return false;
  else if ( entry->active_synonym_detected() > 0 )
    return true;
  else
    return false;
}

bool
VC_structure::invalidate_matching_ASDT_map(uint64_t PPN){

  // search for the corresponding ASDT entry with addr
  std::map<uint64_t, ASDT_entry *>::iterator it = ASDT_structure.find(PPN);

  if (it != ASDT_structure.end()){

    // delete ASDT entry
    delete it->second;
    ASDT_structure.erase(it);

    return true;
  }
  else{
    return false;
  }
}



int
VC_structure::push_back_ASDT_SA_entry(uint64_t PPN, int set_index){

  ASDT_SA_structure[set_index].push_back(ASDT_SA_entry());

  return ASDT_SA_structure[set_index].size()-1; // num of entries
}

void
VC_structure::allocate_ASDT_SA_entry(uint64_t PPN, int set_index, int
                way_index){

  if (way_index == -1){
    std::cout<<"allocate_ASDT_SA_entry, the way index should not be -1 "
            <<std::endl;
    std::cout<<"allocate_ASDT_SA_entry PPN "<<PPN<<" set "<<set_index
             <<" way "<<way_index<<" num_ways "
             <<get_num_ways_ASDT_SA(set_index)<<std::endl;
    abort();
  }else if (ASDT_SA_structure[set_index].size() <= way_index ){
    std::cout<<"allocate_ASDT_SA_entry, the way index "<<way_index
             <<" should be smaller than num of ways "
             <<ASDT_SA_structure[set_index].size()<<std::endl;
    std::cout<<"allocate_ASDT_SA_entry PPN "<<PPN<<" set "<<set_index
             <<" way "<<way_index<<" num_ways "
             <<get_num_ways_ASDT_SA(set_index)<<std::endl;
    abort();
  }else{

    // use the available entry for PPN and set valid bit
    ASDT_SA_structure[set_index][way_index].set_valid(true);
    ASDT_SA_structure[set_index][way_index].set_PPN(PPN);
    // lock the entry until the first line is actually cached
    ASDT_SA_structure[set_index][way_index].set_lock(true);
  }
}

int
VC_structure::find_available_ASDT_SA_entry(int set_index){

  int way_index = -1;
  int way_size  = ASDT_SA_structure[set_index].size();

  for (int index = 0 ; index < way_size ; ++index){

    if (!ASDT_SA_structure[set_index][index].get_valid()){
      way_index = index;
      break;
    }
  }

  return way_index;
}

void
VC_structure::unlock_ASDT_SA_entry(uint64_t PPN){

  int set_index = get_set_index_ASDT_SA(PPN);
  int way_index = find_matching_ASDT_SA_entry(set_index, PPN);

  if (way_index == -1){
    std::cout<<"unlock_ASDT_SA_entry should find a matching entry"<<std::endl;
    abort();
  }else{
    // use the available entry for PPN and unlock the entry
    ASDT_SA_structure[set_index][way_index].set_lock(false);
  }
}

int
VC_structure::find_matching_ASDT_SA_entry(int set_index, uint64_t PPN){

  int way_index = -1;
  int way_size  = ASDT_SA_structure[set_index].size();

  for (int index = 0 ; index < way_size ; ++index){

    if ( ASDT_SA_structure[set_index][index].get_valid()
        && ASDT_SA_structure[set_index][index].get_PPN() == PPN ){

      way_index = index;
      break;
    }
  }

  return way_index;
}


void
VC_structure::invalid_matching_ASDT_SA_entry(uint64_t PPN){

  int set_index = get_set_index_ASDT_SA(PPN);
  int way_size  = ASDT_SA_structure[set_index].size();
  int matching_way = -1;

  for (int index = 0 ; index < way_size ; ++index){

    if ( ASDT_SA_structure[set_index][index].get_valid()
        && ASDT_SA_structure[set_index][index].get_PPN() == PPN ){

      matching_way = index;
      break;
    }
  }

  if ( matching_way == -1 ){
    std::cout<<"invalid_matching_ASDT_SA_entry, no matching entry?"<<std::endl;
    abort();
  }else{
    // make the entry invalid (i.e., empty)
   ASDT_SA_structure[set_index][matching_way].set_valid(false);

    // The way size of the current set is larger than the base way size
    // delete the current entry in the set
    auto it = ASDT_SA_structure[set_index].begin()+matching_way;
    if ( ASDT_SA_structure[set_index].size() >= (m_ASDT_SA_way_size + 1) )
      ASDT_SA_structure[set_index].erase(it);
  }
}

ASDT_SA_entry*
VC_structure::access_ASDT_SA_entry(int set_index, int way_index){

  bool index_error = false;
  if ( set_index >= ASDT_SA_structure.size() )
    index_error = true;
  if ( way_index >= ASDT_SA_structure[set_index].size() )
    index_error = true;

  if (index_error){
    std::cout<<"access_ASDT_SA_entry, invalid indexes"<<std::endl;
    abort();
  }

  return &ASDT_SA_structure[set_index][way_index];
}


bool
VC_structure::profile_ASDT( const uint64_t region_tag, const int line_index ){

  // search for the corresponding ASDT entry with addr
  std::map<uint64_t, ASDT_entry *>::iterator it =
          ASDT_structure.find(region_tag);

  // cannot find a matching entry
  if (it == ASDT_structure.end()){
#ifdef Ongal_debug
  std::cout<<"no matching ASDT entry "<<endl;
#endif
    return false; // no active synonym access
  }else{
    return it->second->get_bit_vector(line_index);
  }
}


bool
VC_structure::lookup_ASDT( const Address addr,
                           bool *access_to_page_with_active_synonym,
                           uint64_t *CPA_VPN,
                           uint64_t *CPA_CR3
                           ){

#ifdef Ongal_debug
  std::cout<<"lookup_ASDT "<<addr<<endl;
#endif

  // physical page number (PPN)
  uint64_t PPN = addr.getAddress() / get_region_size();

  // search for the corresponding ASDT entry with addr
  std::map<uint64_t, ASDT_entry *>::iterator it = ASDT_structure.find(PPN);

  // cannot find a matching entry
  if (it == ASDT_structure.end()){
#ifdef Ongal_debug
  std::cout<<"no matching ASDT entry "<<addr<<endl;
#endif
    return false; // no active synonym access
  }

  // a matching asdt entry is found
  // check Vaddr and CR3
  uint64_t VPN = addr.get_Vaddr() / get_region_size();
  // calculate VPN based on the region_size
  bool access_with_leading_vaddr = ((addr.get_CR3() == it->second->get_cr3())
                       && (VPN == it->second->get_virtual_page_number()));

#ifdef Kernel_Op
  // Needs to detect cases where active synonym accesses in kernel space due to
  // difference in only ASID (CR3)
  // leading virtual address and the virtual address generated by a CPU are in
  // the same page with kernel virtual address space
  if ( is_kernel_space(addr) && (VPN == it->second->get_virtual_page_number()))
    access_with_leading_vaddr = true;
#endif

  // see if the access is to a page with active syonyms
  // regardless of non/leading virtual address
  if ( (it->second->active_synonym_detected() > 0) ||
                  !access_with_leading_vaddr ){
    (*access_to_page_with_active_synonym) = true;
  }

  if ( access_with_leading_vaddr ){

#ifdef Ongal_debug
    std::cout<<"same CPA "<<addr<<endl;
#endif

    return false; // leading virtual address
  } else {

#ifdef Ongal_debug
    std::cout<<"found active synonym "<<addr<<" CPA_VPN 0x"<<hex<<
      it->second->get_virtual_page_number()<<
      " CPA_CR3 0x"<<it->second->get_cr3()<<
      endl;
#endif

    *CPA_VPN = it->second->get_virtual_page_number();
    *CPA_CR3 = it->second->get_cr3();
    // update active synonym vector
    it->second->update_active_synonym_vector(VPN,addr.get_CR3());

    return true;
    // non-leading virtual address (Active synonym detection)
    // the corresponding entry is found but cr3 or VPN is different
  }
}

bool
VC_structure::invalidate_ASDT_with_VPN(uint64_t VPN, uint64_t *cr3, uint32_t
                *random_number_to_hash_with){ // for demap operations

  // Flush SS/ART entries
  art_entries.clear(); // ART
  flush_all_SS();      // SS

  std::map<uint64_t, ASDT_entry *>::iterator it = ASDT_structure.begin();

  for ( ; it != ASDT_structure.end() ; ++it ){

    if (it->second->get_virtual_page_number() == VPN){
      // find a matching entry
      (*cr3) = it->second->get_cr3();
      // get the corresponding entry's the PPN
      (*random_number_to_hash_with) =
              it->second->get_random_number_to_hash_with();
      ASDT_structure.erase(it);   // delete the entry from the map

      return true;
    }
  }

  return false;
}

std::set<uint64_t>
VC_structure::find_ASDT_entries_no_kernel(){

  std::set<uint64_t> target;

  std::map<uint64_t, ASDT_entry *>::iterator it = ASDT_structure.begin();

  for ( ; it != ASDT_structure.end() ; ++it ){

    uint64_t leading_page_address =
            it->second->get_virtual_page_number()*get_region_size();

    // leading virtual address is NOT in kernel space
    if (!is_kernel_space(leading_page_address)){
      target.insert(leading_page_address);
    }
  }

  return target;
}

bool
VC_structure::lookup_VC_structure( const Address addr,
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
  bool* active_synonym_access_non_leading){

#ifdef Ongal_debug
  std::cout<<endl<<"lookup_VC_structure "<<addr<<endl;
#endif

  if ( is_pagetable_walk(addr))
    return false;

  bool active_synonym_access_in_asdt = false;
  uint64_t CPA_VPN = 0;
  uint64_t CPA_CR3 = 0;
  bool access_to_page_with_active_synonym = false;

  // ASDT lookup
  // return true when a active synonym access is detected, i.e, the reference
  // address is NOT CPA!
  // That is, access to a page with active syonym using NON-CPA
  active_synonym_access_in_asdt = lookup_ASDT( addr ,
                                     &access_to_page_with_active_synonym,
                                     &CPA_VPN,
                                     &CPA_CR3);

  // Profiling
  // access to a page with active synonyms (CPA and non-CPA)
  if ( access_to_page_with_active_synonym ){

    //print_ART();
    ++(*num_active_synonym_access);

    if (store)
      ++(*num_active_synonym_access_store);

    if ( is_kernel_space( addr ) )
      ++(*num_kernel_space_access_with_active_synonym);
  }

  // Profiling
  // access to a page with active syonym using (only Non-CPA)
  if ( active_synonym_access_in_asdt ){

    ++(*num_active_synonym_access_with_non_CPA);
    (*active_synonym_access_non_leading) = true;

    if (store)
    ++(*num_active_synonym_access_with_non_CPA_store);

#ifdef Ongal_debug
    std::cout<<"NEW Active Synonym Detection "<<addr<<std::endl;
#endif
  }else{
#ifdef Ongal_debug
    std::cout<<"NEW Active Synonym NOT! Detection "<<addr<<std::endl;
#endif
  }

  // SS access - pass VPN
  bool ss_hit = lookup_SS( addr.get_Vaddr()/get_region_size(),  addr.get_CR3()
                  );
  bool ss_64KB_hit = lookup_SS_64KB( addr.get_Vaddr()/get_region_size(),
                  addr.get_CR3() );
  bool ss_1MB_hit = lookup_SS_1MB( addr.get_Vaddr()/get_region_size(),
                  addr.get_CR3() );

  bool art_lookup_hit = false;

  // Profiling
  // SS hit counts
  if (ss_hit){
    ++(*num_ss_hits); // 4KB granularity
    //++(*num_ss_64KB_hits);
    //++(*num_ss_1MB_hits);
  }
  if ( ss_64KB_hit ){
    ++(*num_ss_64KB_hits);
    //++(*num_ss_1MB_hits);
  }
  if ( ss_1MB_hit ){
    ++(*num_ss_1MB_hits);
  }

  // SS hit --> ART Access
  if (ss_hit){

#ifdef Ongal_debug
    std::cout<<"SS hits "<<addr<<std::endl;
#endif

    // ART lookup
    art_lookup_hit = lookup_ART(addr);

    // update LCPA impact on ART lookup and hits
    // consecutive ART access in the same page
    uint64_t temp_VPN = addr.get_Vaddr()/get_region_size();
    if ( m_prev_lookuped_VPN == temp_VPN ){
      ++(*num_LCPA_saving_consecutive_active_synonym_access_in_a_page);
      if (art_lookup_hit)
        ++(*num_LCPA_saving_ART_hits);
    }else{
      m_prev_lookuped_VPN = temp_VPN;
    }

    if (art_lookup_hit){     // art hit

      ++(*num_art_hits); // count art hits

      // Non-CPA access
      if (!active_synonym_access_in_asdt){
#ifdef Ongal_debug
        std::cout<<"No active synonym access but ART hit...what?"<<std::endl;
#endif
        return true;
      }
    }
  }

  // SS and ART update for Non-CPA access and ART miss
  if ((!art_lookup_hit) && active_synonym_access_in_asdt){

#ifdef Ongal_debug
    std::cout<<"new ART entry "<<addr<<" CPA_VPN 0x"<<hex<<
            CPA_VPN<<" CPA_CR3 0x"<<hex<<CPA_CR3<<std::endl;
#endif
    // store request
    // this may cause load bypassing fault
    if ( store ){
      ++(*num_active_synonym_access_with_non_CPA_store_ART_miss);
    }

    // Update ART
    update_ART(addr, CPA_VPN, CPA_CR3 );
  }
  return false;
}

bool
VC_structure::lookup_ART( const Address addr ){

  uint64_t VPN = addr.get_Vaddr()/get_region_size();

  std::vector<ART_entry>::iterator it = art_entries.begin();

#ifdef Ongal_debug
  std::cout<<"lookup_ART "<<addr<<endl;
#endif

  for ( ; it != art_entries.end() ; ++it ){

#ifdef Ongal_debug
  std::cout<<"-ART Entry"
           <<hex<<" VPN 0x"<<it->get_synonym_VPN()
           <<" CR3 0x"<<it->get_synonym_CR3()
           <<" CPA_VPN 0x"<<it->get_CPA_VPN()
           <<" CPA_CR3 0x"<<it->get_CPA_CR3()
           <<endl;
#endif

    if (it->get_synonym_VPN() == VPN &&
       it->get_synonym_CR3() == addr.get_CR3()){

      // a matching entry is found?!
      // update the location of the entry in the vector for LRU info
#ifdef Ongal_debug
      std::cout<<"Found! move it back to MRU place"<<endl;
#endif

      ART_entry temp = ART_entry(VPN, addr.get_CR3(), it->get_CPA_VPN(),
                      it->get_CPA_CR3());
      art_entries.erase(it); // erase
      art_entries.push_back(temp); // put it as an MRU

      return true;
    }
  }
  // cannot find a matchign entry
  return false;
}

bool
VC_structure::lookup_ART( const uint64_t vaddr, const uint64_t cr3, uint64_t
                &art_vaddr, uint64_t &art_cr3 ){

  uint64_t VPN = vaddr/get_region_size();

  std::vector<ART_entry>::iterator it = art_entries.begin();

#ifdef Ongal_debug
  std::cout<<"lookup_ART "<<vaddr<<endl;
#endif

  for ( ; it != art_entries.end() ; ++it ){

#ifdef Ongal_debug
  std::cout<<"-ART Entry"
           <<hex<<" VPN 0x"<<it->get_synonym_VPN()
           <<" CR3 0x"<<it->get_synonym_CR3()
           <<" CPA_VPN 0x"<<it->get_CPA_VPN()
           <<" CPA_CR3 0x"<<it->get_CPA_CR3()
           <<endl;
#endif

    if (it->get_synonym_VPN() == VPN &&
       it->get_synonym_CR3() == cr3){

      // a matching entry is found?!
      // update the location of the entry in the vector for LRU info
#ifdef Ongal_debug
      std::cout<<"Found! move it back to MRU place"<<endl;
#endif

      art_vaddr = (it->get_CPA_VPN()*get_region_size()) +
              (vaddr%get_region_size());
      art_cr3   = it->get_CPA_CR3();

      /*
      std::cout<<"ART Entry"
               <<hex<<" VPN 0x"<<it->get_synonym_VPN()
               <<" CR3 0x"<<it->get_synonym_CR3()
               <<" CPA_VPN 0x"<<it->get_CPA_VPN()
               <<" CPA_CR3 0x"<<it->get_CPA_CR3()
               <<" art_vaddr 0x"<<art_vaddr
               <<" art_cr3 0x"<<art_cr3
               <<endl;
      */
      ART_entry temp = ART_entry(VPN, cr3, it->get_CPA_VPN(),
                      it->get_CPA_CR3());
      art_entries.erase(it); // erase
      art_entries.push_back(temp); // put it as an MRU

      return true;
    }
  }
  // cannot find a matchign entry
  return false;
}

void
VC_structure::update_ART( const Address addr, uint64_t CPA_VPN, uint64_t
                CPA_CR3 ){

  // add the new pair at the end of the vector
  uint64_t VPN = addr.get_Vaddr()/get_region_size();
  // new art entry is added
  art_entries.push_back(ART_entry(VPN, addr.get_CR3(), CPA_VPN, CPA_CR3));
  // ss update, new art entry by passing true
  update_SS(VPN, addr.get_CR3(), true);

#ifdef Ongal_debug
  std::cout<<"update_ART "<<addr<<hex<<" CPA_VPN 0x"<<CPA_VPN<<
          " CPA_CR3 0x"<<CPA_CR3<<endl;
#endif

  if (m_art_size < art_entries.size()){
    // over the size limit, we need to erase the first one (LRU)

    // ss update (delete = false)
    update_SS(art_entries.begin()->get_synonym_VPN(),
                    art_entries.begin()->get_synonym_CR3() ,false);
    // art entry eviction (LRU)
    art_entries.erase(art_entries.begin());
  }
}

void
VC_structure::print_ART(){

  std::vector<ART_entry>::iterator it;
  it = art_entries.begin(); // first entry

  for ( ; it != art_entries.end() ; ++it ){
    it->print();
  }
  cout<<endl;
}

void
VC_structure::delete_ART( uint64_t CPA_VPN, uint64_t CPA_CR3 ){

  // this method will be called when ASDT entry has no cached lines.
  // called in update_ASDT
#ifdef Ongal_debug
  std::cout<<"Delete_ART "<<" CPA_VPN 0x"<<hex<<CPA_VPN<<
          " CPA_CR3 0x"<<CPA_CR3<<endl;
#endif

#ifdef FLUSH_SS_ART

  // Flush all SS and ART entries
  art_entries.clear(); // ART
  flush_all_SS();      // SS
  return;

#endif

  // it is possible multiple entries with the same CPA (VPN and CR3)
  // so , search it until no corresponding entries remain
  bool found;
  std::vector<ART_entry>::iterator it;

  do{

    found = false;  // initialize no entries are found
    it = art_entries.begin(); // first entry

    for ( ; it != art_entries.end() ; ++it ){

#ifdef Ongal_debug
      std::cout<<"-ART Entry"
               <<hex<<" VPN 0x"<<it->get_synonym_VPN()
               <<" CR3 0x"<<it->get_synonym_CR3()
               <<" CPA_VPN 0x"<<it->get_CPA_VPN()
               <<" CPA_CR3 0x"<< it->get_CPA_CR3()
               <<endl;
#endif

      if (it->get_CPA_VPN() == CPA_VPN && it->get_CPA_CR3() == CPA_CR3){

#ifdef Ongal_debug
        std::cout<<"Found!"<<endl;
#endif
        // ss update (delete = false)
        update_SS(it->get_synonym_VPN(), it->get_synonym_CR3(), false);
        // art entry eviction
        art_entries.erase(it);

        found = true;
        break;
      }
    } // for
  }while ( found );

}

void
VC_structure::flush_all_SS(){

  int SS_size;

  SS_size = ss_entries.size();

  for ( int index = 0 ; index < SS_size ; ++index ){
    ss_entries[index].unset_ss_bit();    // unset bit
    ss_entries[index].set_ss_counter(0); // initialize a counter
  }

  SS_size = ss_64KB_entries.size();

  for ( int index = 0 ; index < SS_size ; ++index ){
    ss_64KB_entries[index].unset_ss_bit();    // unset bit
    ss_64KB_entries[index].set_ss_counter(0); // initialize a counter
  }

  SS_size = ss_1MB_entries.size();

  for ( int index = 0 ; index < SS_size ; ++index ){
    ss_1MB_entries[index].unset_ss_bit();    // unset bit
    ss_1MB_entries[index].set_ss_counter(0); // initialize a counter
  }
}

void
VC_structure::add_new_ASDT_map(uint64_t PPN, uint64_t CPA_VPN, uint64_t
                CPA_CR3){

//      if ( CPA_VPN == 0 || CPA_CR3 == 0 ){

      if ( CPA_VPN == 0){
      //  std::cout<<" NEW ASDT PPN "<<std::hex<<PPN
      //  	 <<" VPN "<<std::hex<<CPA_VPN
      //  	 <<" CR3 "<<CPA_CR3<<std::endl;

      //  abort();
      }


      ASDT_structure[PPN] =
        new ASDT_entry( CPA_VPN,
                        CPA_CR3,
                        get_region_size()/get_line_size(),
                        inc_LRU_counter());

      // update physical memory
      m_main_memory.Update_Physical_Page(PPN, CPA_VPN, CPA_CR3);
}

void
VC_structure::update_SS( const uint64_t VPN, const uint64_t CR3,
                bool add_or_delete ){

  // 4KB SS
#ifdef SS_HASH
  int index = (VPN+CR3) % ss_entries.size();
#else
  int index = VPN % ss_entries.size();
#endif

  if (add_or_delete ){
    // when a new ART entry is added
    ss_entries[index].set_ss_bit();
    ss_entries[index].inc_ss_counter();
  }else{
    // when an ART entry is evicted
    ss_entries[index].dec_ss_counter();

    if (ss_entries[index].get_ss_counter() == 0){
      ss_entries[index].unset_ss_bit();
    }else if (ss_entries[index].get_ss_counter() < 0){
      // error
      std::cout<<"update_SS error.... counter is smaller than 0"<<std::endl;
      abort();
    }
  }

  // 64KB SS
#ifdef SS_HASH
  index = (VPN/16+CR3) % ss_64KB_entries.size();
#else
  index = (VPN/16) % ss_64KB_entries.size();
#endif

  if (add_or_delete ){
    // when a new ART entry is added
    ss_64KB_entries[index].set_ss_bit();
    ss_64KB_entries[index].inc_ss_counter();
  }else{
    // when an ART entry is evicted
    ss_64KB_entries[index].dec_ss_counter();

    if (ss_64KB_entries[index].get_ss_counter() == 0){
      ss_64KB_entries[index].unset_ss_bit();
    }else if (ss_64KB_entries[index].get_ss_counter() < 0){
      // error
      std::cout<<"update_SS error.... counter is smaller than 0"<<std::endl;
      abort();
    }
  }


  // 1MB SS
#ifdef SS_HASH
  index = (VPN/256+CR3) % ss_1MB_entries.size();
#else
  index = (VPN/256) % ss_1MB_entries.size();
#endif

  if (add_or_delete ){
    // when a new ART entry is added
    ss_1MB_entries[index].set_ss_bit();
    ss_1MB_entries[index].inc_ss_counter();
  }else{
    // when an ART entry is evicted
    ss_1MB_entries[index].dec_ss_counter();

    if (ss_1MB_entries[index].get_ss_counter() == 0){
      ss_1MB_entries[index].unset_ss_bit();
    }else if (ss_1MB_entries[index].get_ss_counter() < 0){
      // error
      std::cout<<"update_SS error.... counter is smaller than 0"<<std::endl;
      abort();
    }
  }
}

bool
VC_structure::lookup_SS( const uint64_t VPN, const uint64_t CR3 ){

#ifdef SS_HASH
  int index = (VPN+CR3) % ss_entries.size();
#else
  int index = VPN % ss_entries.size();
#endif

  return ss_entries[index].get_ss_bit();
}

bool
VC_structure::lookup_SS_64KB( const uint64_t VPN, const uint64_t CR3 ){

#ifdef SS_HASH
  int index = (VPN/16+CR3) % ss_64KB_entries.size();
#else
  int index = (VPN/16) % ss_64KB_entries.size();
#endif

  return ss_64KB_entries[index].get_ss_bit();
}

bool
VC_structure::lookup_SS_1MB( const uint64_t VPN, const uint64_t CR3 ){

#ifdef SS_HASH
  int index = (VPN/256+CR3) % ss_1MB_entries.size();
#else
  int index = (VPN/256) % ss_1MB_entries.size();
#endif

  return ss_1MB_entries[index].get_ss_bit();
}


bool
VC_structure::is_pagetable_walk(const Address addr ){

  if ( addr.get_Vaddr() == 0 && addr.get_CR3() == 0 )
    return true;
  return false;
}

bool
VC_structure::is_kernel_space(const Address addr ){


  uint64_t target_bit = (1UL << 47);
  uint64_t comparison = target_bit & addr.get_Vaddr();

  if (comparison > 0)
    return true; // kernel part
  else
    return false; // non-kernel part
}

bool
VC_structure::is_kernel_space(const uint64_t addr ){


  uint64_t target_bit = (1UL << 47);
  uint64_t comparison = target_bit & addr;

  if (comparison > 0)
    return true; // kernel part
  else
    return false; // non-kernel part
}

void
VC_structure::printStats(){


}


void
VC_structure::Sampling_for_Motivation(
                     Stats::Scalar* num_valid_asdt_entry,
                     Stats::Scalar* num_valid_asdt_entry_with_active_synonym,
                     Stats::Scalar* num_virtual_pages_with_active_synonym,
                     Stats::Scalar* num_max_num_valid_asdt_entry,
                     Stats::Scalar* num_valid_asdt_entry_one_line,
                     Stats::Scalar* num_valid_asdt_entry_two_lines
                            ){

  // 1. count num_valid_asdt_entry;
  (*num_valid_asdt_entry) += ASDT_structure.size();

  // track max num valid asdt entries
  if ( ASDT_structure.size() > m_max_num_cached_page ){
    m_max_num_cached_page           = ASDT_structure.size();
    (*num_max_num_valid_asdt_entry) = ASDT_structure.size();
  }

  // 2. count num_valid_asdt_entry_with_active_synonym;
  std::map<uint64_t, ASDT_entry *>::iterator it = ASDT_structure.begin();

  for ( ; it != ASDT_structure.end() ; ++it ){

    // 1. a page with active synonym
    int num_vpns = (it->second)->active_synonym_detected();
    if ( num_vpns > 0 ){

      // count the page
      ++(*num_valid_asdt_entry_with_active_synonym);

      // 3. count num_virtual_pages_with_active_synonym
      // count the number of Virtual Pages
      (*num_virtual_pages_with_active_synonym) += num_vpns;
    }

    int num_cached_lines = (it->second)->get_num_cached_lines();
    // 2. Count Pages with one cached lines
    if (num_cached_lines == 1)
      ++(*num_valid_asdt_entry_one_line);
    // 3. Count Pages with one cached lines
    if (num_cached_lines == 2)
      ++(*num_valid_asdt_entry_two_lines);

  }
}

void
VC_structure::SLB_trap_check( Address addr,
                              Stats::Scalar* num_SLB_traps_8,
                              Stats::Scalar* num_SLB_traps_16,
                              Stats::Scalar* num_SLB_traps_32,
                              Stats::Scalar* num_SLB_traps_48,
                              Stats::Scalar* num_SLB_traps_Lookup
                              ){

  // physical page number (PPN)
  //uint64_t PPN = addr.getAddress() / get_region_size();
  // calculate VPN based on the region_size
  uint64_t VPN = addr.get_Vaddr() / get_region_size();


  // access main meory to see if the first touch is same as the current access
  //if ( m_main_memory.synonym_access_SLB(PPN,
  //VPN,
  //addr.get_CR3()) ){

  // non-primary virtual address access
  ++(*num_SLB_traps_Lookup);
  // need to check SLB traps
  SLB_8->SLB_lookup(VPN,addr.get_CR3(), 0, 0, num_SLB_traps_8);
  SLB_16->SLB_lookup(VPN,addr.get_CR3(), 0, 0, num_SLB_traps_16);
  SLB_32->SLB_lookup(VPN,addr.get_CR3(), 0, 0, num_SLB_traps_32);
  SLB_48->SLB_lookup(VPN,addr.get_CR3(), 0, 0, num_SLB_traps_48);

  //}
}
