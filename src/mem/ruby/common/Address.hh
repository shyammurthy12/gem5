/*
 * Copyright (c) 1999 Mark D. Hill and David A. Wood
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
 */

#ifndef __MEM_RUBY_COMMON_ADDRESS_HH__
#define __MEM_RUBY_COMMON_ADDRESS_HH__

#include <cassert>
#include <iomanip>
#include <iostream>

#include "base/hashmap.hh"
#include "base/types.hh"
#include "mem/ongal_VC.hh"
#include "mem/ruby/common/TypeDefines.hh"

const uint32_t ADDRESS_WIDTH = 64; // address width in bytes

// selects bits inclusive
Addr bitSelect(Addr addr, unsigned int small, unsigned int big);
Addr bitRemove(Addr addr, unsigned int small, unsigned int big);
Addr maskLowOrderBits(Addr addr, unsigned int number);
Addr maskHighOrderBits(Addr addr, unsigned int number);
Addr shiftLowOrderBits(Addr addr, unsigned int number);
Addr getOffset(Addr addr);
Addr makeLineAddress(Addr addr);
Addr makeNextStrideAddress(Addr addr, int stride);
std::string printAddress(Addr addr);

class Address;
typedef Address PhysAddress;
typedef Address VirtAddress;

class Address
{
  public:
    Address()
        : m_address(0)
    { }

    explicit
    Address(physical_address_t address)
        : m_address(address)
    { }

    Address(const Address& obj);
    Address& operator=(const Address& obj);

    void setAddress(physical_address_t address) { m_address = address; }
    physical_address_t getAddress() const {return m_address;}
    // selects bits inclusive
    physical_address_t bitSelect(unsigned int small, unsigned int big) const;
    physical_address_t bitRemove(unsigned int small, unsigned int big) const;
#ifdef Ongal_VC
    physical_address_t maskLowOrderBits(physical_address_t addr, unsigned int
                    number) const;
#endif
    physical_address_t maskLowOrderBits(unsigned int number) const;
    physical_address_t maskHighOrderBits(unsigned int number) const;
    physical_address_t shiftLowOrderBits(unsigned int number) const;

    physical_address_t getLineAddress() const;
    physical_address_t getOffset() const;
    void makeLineAddress();
    void makeNextStrideAddress(int stride);

    int64 memoryModuleIndex() const;

    void print(std::ostream& out) const;
    void output(std::ostream& out) const;
    void input(std::istream& in);

    void
    setOffset(int offset)
    {
        // first, zero out the offset bits
        makeLineAddress();
        m_address |= (physical_address_t) offset;
    }

#ifdef Ongal_VC
    void setVaddr(physical_address_t _m_vir_addr){
      m_vir_addr = _m_vir_addr;
    }

    void setPaddr(physical_address_t _m_phy_addr){
      m_phy_addr = _m_phy_addr;
    }

    void setCR3(physical_address_t _cr3){
      m_CR3 = _cr3;
    }

    void switch_to_vir(){
      m_address = m_vir_addr;
    }

    void switch_to_phy(){
      m_address = m_phy_addr;
    }

    physical_address_t get_CR3() const { return m_CR3; }
    physical_address_t get_Vaddr() const { return m_vir_addr; }
    physical_address_t get_Paddr() const { return m_phy_addr; }

    bool vir_cache_tag_matching( const Address& addr ){
      if ( get_CR3() == addr.get_CR3() && get_Vaddr() == addr.get_Vaddr() )
        return true;
      else
        return false;
    }
#endif

  private:
    physical_address_t m_address;
    physical_address_t m_phy_addr;
    physical_address_t m_vir_addr;
    physical_address_t m_CR3;

};

inline Address
line_address(const Address& addr)
{
    Address temp(addr);
    temp.makeLineAddress();
    return temp;
}

inline bool
operator<(const Address& obj1, const Address& obj2)
{
    return obj1.getAddress() < obj2.getAddress();
}

inline std::ostream&
operator<<(std::ostream& out, const Address& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

inline bool
operator==(const Address& obj1, const Address& obj2)
{
    return (obj1.getAddress() == obj2.getAddress());
}

inline bool
operator!=(const Address& obj1, const Address& obj2)
{
    return (obj1.getAddress() != obj2.getAddress());
}

// rips bits inclusive
inline physical_address_t
Address::bitSelect(unsigned int small, unsigned int big) const
{
    physical_address_t mask;
    assert(big >= small);

    if (big >= ADDRESS_WIDTH - 1) {
        return (m_address >> small);
    } else {
        mask = ~((physical_address_t)~0 << (big + 1));
        // FIXME - this is slow to manipulate a 64-bit number using 32-bits
        physical_address_t partial = (m_address & mask);
        return (partial >> small);
    }
}

// removes bits inclusive
inline physical_address_t
Address::bitRemove(unsigned int small, unsigned int big) const
{
    physical_address_t mask;
    assert(big >= small);

    if (small >= ADDRESS_WIDTH - 1) {
        return m_address;
    } else if (big >= ADDRESS_WIDTH - 1) {
        mask = (physical_address_t)~0 >> small;
        return (m_address & mask);
    } else if (small == 0) {
        mask = (physical_address_t)~0 << big;
        return (m_address & mask);
    } else {
        mask = ~((physical_address_t)~0 << small);
        physical_address_t lower_bits = m_address & mask;
        mask = (physical_address_t)~0 << (big + 1);
        physical_address_t higher_bits = m_address & mask;

        // Shift the valid high bits over the removed section
        higher_bits = higher_bits >> (big - small + 1);
        return (higher_bits | lower_bits);
    }
}

inline physical_address_t
Address::maskLowOrderBits(unsigned int number) const
{
  physical_address_t mask;

  if (number >= ADDRESS_WIDTH - 1) {
      mask = ~0;
  } else {
      mask = (physical_address_t)~0 << number;
  }
  return (m_address & mask);
}

#ifdef Ongal_VC
inline physical_address_t
Address::maskLowOrderBits(physical_address_t addr, unsigned int number) const
{
  physical_address_t mask;

  if (number >= ADDRESS_WIDTH - 1) {
      mask = ~0;
  } else {
      mask = (physical_address_t)~0 << number;
  }
  return (addr & mask);
}
#endif

inline physical_address_t
Address::maskHighOrderBits(unsigned int number) const
{
    physical_address_t mask;

    if (number >= ADDRESS_WIDTH - 1) {
        mask = ~0;
    } else {
        mask = (physical_address_t)~0 >> number;
    }
    return (m_address & mask);
}

inline physical_address_t
Address::shiftLowOrderBits(unsigned int number) const
{
    return (m_address >> number);
}

Address next_stride_address(const Address& addr, int stride);

__hash_namespace_begin
template <> struct hash<Address>
{
    size_t
    operator()(const Address &s) const
    {
        return (size_t)s.getAddress();
    }
};
__hash_namespace_end

namespace std {
template <> struct equal_to<Address>
{
    bool
    operator()(const Address& s1, const Address& s2) const
    {
        return s1 == s2;
    }
};
} // namespace std

#endif // __MEM_RUBY_COMMON_ADDRESS_HH__
