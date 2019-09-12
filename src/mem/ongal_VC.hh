#ifndef _ongal_VC_hh
#define _ongal_VC_hh

#define Sampling_Interval 40000 // Sampling Interval
#define Ongal_VC // to use virtual cache modeling (should be always turned on)
#define VIVT     // user virtually index and virtually tagged cache design
#define SS_HASH  // to use a hash to manage SS. Page Number + ASID
#define REGION_SIZE 4096 // Analysis and ASDT Granularity

/* Kernel Optimization */
/*
   1. Atomic + Classic: depending on the evaluation.
   - With this op: SS hit will NOT consider the access to the kernel space as
   active synonym accesses
   - Without this op: SS hit will reflect the kernel space accesses
   2. O3 + Ruby: should be used.
*/
//#define Kernel_Op

/* TLB access in cache controller
   for VIVT and for VIPT caches: VIVT flag should be set

   "LateMemTrap" is for new features and cache controller,
   that is, this should be defined for LateMemTrap
   Others for each operation from CPU side. We can selectively choose one of
   them.
*/
//#define LateMemTrap
//#define LateMemTrapInst      // for instruction fetch of CPU side
//#define LateMemTrapDataRead  // for data read of CPU side
//#define LateMemTrapDataWrite // for data wrtie of CPU side

//////////////////////////////////////
// O3 + Ruby System for Timing results
#define Ongal_Three_Level // Cache hierarchy setting
#define O3CPU_Ongal_VC // it allows LSQ operations for VC-DSR to be executed.


/////////////////////////////
// ASDT/SS/ART manage options

/* to flush all bits in an SS and all entries in an ART
   when ASDT with active synonym eviction
*/
//#define FLUSH_SS_ART

/* now this only work with a classic model */
#define ASDT_Set_Associative_Array

//////////////////////////////
// Test related work together
//#define Related_Work


//#define Ongal_debug

#define IFETCH 0x0
#define DATA_LOAD 0x1
#define DATA_STORE 0x2
#define DATA_RETRY 0x3
#define OTHERS 0x4

// Fake ASIDs
#define Pagetable_Walk_CR3 0xffffffffffffffff
#define Kernel_Space_CR3   0xffffffffffff0000
#define Interrupt_CR3      0xffffffff0000ffff
#define Null_CR3 0x0

// Demap and unmap_vmas() handling
#define UNMAP_REQ 0x0000

#endif
