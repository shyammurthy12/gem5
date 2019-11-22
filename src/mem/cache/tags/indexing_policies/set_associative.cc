/*
 * Copyright (c) 2018 Inria
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
 * Authors: Daniel Carvalho
 *          Erik Hallnor
 */

/**
 * @file
 * Definitions of a set associative indexing policy.
 */

#include "mem/cache/tags/indexing_policies/set_associative.hh"

#include "mem/cache/replacement_policies/replaceable_entry.hh"

#include "mem/ongal_VC.hh"
#include "mem/ruby/common/ASDT_entry.hh"

using namespace std;
SetAssociative::SetAssociative(const Params *p)
    : BaseIndexingPolicy(p)
{
}

uint32_t
SetAssociative::extractSet(const Addr addr) const
{

    return (addr >> setShift) & setMask;
}


uint32_t
SetAssociative::extractSet_Vaddr(Addr addr) const
{
      //printf("Set number is %lu\n",(addr >> setShift) & setMask);
      // addr should be a virtual address
      return ((addr >> setShift) & setMask);
}


uint32_t
SetAssociative::extractSet_Vaddr_with_hashing(Addr addr, vector<int>
                hash_scheme_for_xor) const
{
 //     printf("Address is %lx\n",addr>>setShift);
      // addr should be a virtual address
   //   return ((((addr >> setShift) &
   //   setMask)^random_constant_to_xor_with))&setMask;
  //  cout << "Hash scheme used" << endl;
 //   for (int i=0;i<hash_scheme_for_xor.size();i++) {
 //   	cout << hash_scheme_for_xor[i] << endl;
 //   }
      uint64_t num_to_xor_with=0;
      for (int i=0; i<hash_scheme_for_xor.size(); i++) {
        uint64_t temp = addr&(1<<hash_scheme_for_xor[i]);
//	printf("%d, %lu, %lu",i, temp, (addr>>hash_scheme_for_xor[i])&0x01);
        if (temp)
                num_to_xor_with = (num_to_xor_with<<1) + 1;
        else
                num_to_xor_with = (num_to_xor_with<<1);
//	printf("%lx\n", num_to_xor_with);
      }
#ifdef Smurthy_debug
      printf("The random constant to xor with"
                      "%lx\n",num_to_xor_with);
      printf("The setNumber is %lu\n",
                   (((addr >> setShift)^num_to_xor_with))&setMask);
#endif
      num_to_xor_with = 0;
      return (((addr >> setShift)^num_to_xor_with))&setMask;
}



Addr
SetAssociative::regenerateAddr(const Addr tag, const ReplaceableEntry* entry)
                                                                        const
{
    return (tag << tagShift) | (entry->getSet() << setShift);
}

std::vector<ReplaceableEntry*>
SetAssociative::getPossibleEntries(const Addr addr) const
{
    return sets[extractSet(addr)];
}

//Ongal
std::vector<ReplaceableEntry*>
SetAssociative::getPossibleEntries_with_Vaddr(const Addr addr, vector<int>
                hash_scheme_for_xor) const
{
 return sets[extractSet_Vaddr_with_hashing(addr,hash_scheme_for_xor)];
}

SetAssociative*
SetAssociativeParams::create()
{
    return new SetAssociative(this);
}
