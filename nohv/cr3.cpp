#include <ia32.hpp>
#include <intrin.h>

// This function tries to detect hypervisors that don't properly check
// reserved bits in CR3 (aka bits [63:MAXPHYSADDR]).
// 
// Vol3C[26.3.1.1(Checks on Guest Control Registers, Debug Registers, and MSRs)]
bool cr3_detected_1() {
  _disable();

  cr3 curr_cr3;
  curr_cr3.flags = __readcr3();

  cpuid_eax_80000008 cpuid_80000008;
  __cpuid(reinterpret_cast<int*>(&cpuid_80000008), 0x80000008);

  // try to set every reserved bit (besides last one, theres a seperate test for that)
  for (int i = cpuid_80000008.eax.number_of_linear_address_bits; i < 63; ++i) {
    __try {
      auto test_cr3 = curr_cr3;
      test_cr3.flags |= (1ull << i);
      __writecr3(test_cr3.flags);

      // restore old CR3 after hypervisor pooped on it
      __writecr3(curr_cr3.flags);

      // hypervisor should've raised an exception >:(
      _enable();

      return true;
    } __except (1) {
      // maybe the write passed through even though an exception was raised?
      if (__readcr3() != curr_cr3.flags) {
        _enable();
        return true;
      }
    }
  }

  _enable();
  return false;
}

// This function tries to detect hypervisors that don't properly ignore
// bit 63 of CR3 while CR4.PCIDE=1.
// 
// Vol3C[4.10.4.1(Operations that Invalidate TLBs and Paging-Structure Caches)]
// Vol3C[26.3.1.1(Checks on Guest Control Registers, Debug Registers, and MSRs)]
bool cr3_detected_2() {
  _disable();

  cr3 curr_cr3;
  curr_cr3.flags = __readcr3();

  cr4 curr_cr4;
  curr_cr4.flags = __readcr4();

  // PCIDE=1
  if (curr_cr4.pcid_enable) {
    __try {
      auto test_cr3 = curr_cr3;
      test_cr3.flags |= (1ull << 63);
      __writecr3(test_cr3.flags);
    }
    __except (1) {
      // shouldn't raise an exception
      _enable();
      return false;
    }
  }
  // PCIDE=0
  else {
    __try {
      auto test_cr3 = curr_cr3;

      // set CR3[11:0] to 0 before enabling PCIDE
      test_cr3.flags &= ~0xFFFull;
      __writecr3(test_cr3.flags);

      // set PCIDE to 1
      auto test_cr4 = curr_cr4;
      test_cr4.pcid_enable = 1;
      __writecr4(test_cr4.flags);

      // set bit 63 of CR3 (should NOT raise an exception in a proper hypervisor)
      test_cr3.flags |= (1ull << 63);
      __writecr3(test_cr3.flags);

      // restore CR4 and CR3
      __writecr4(curr_cr4.flags);
      __writecr3(curr_cr3.flags);
    }
    __except (1) {
      // restore CR4 and CR3
      __writecr4(curr_cr4.flags);
      __writecr3(curr_cr3.flags);

      // shouldn't raise an exception
      _enable();
      return false;
    }
  }

  _enable();
  return false;
}

