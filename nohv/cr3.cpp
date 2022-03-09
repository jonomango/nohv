#include <ia32.hpp>
#include <intrin.h>

bool cr3_detected_1() {
  _disable();

  cr3 old_cr3;
  old_cr3.flags = __readcr3();

  cpuid_eax_80000008 cpuid_80000008;
  __cpuid(reinterpret_cast<int*>(&cpuid_80000008), 0x80000008);

  // try to set every reserved bit (besides last one, theres a seperate test for that)
  for (int i = cpuid_80000008.eax.number_of_linear_address_bits; i < 63; ++i) {
    auto test_cr3 = old_cr3;
    test_cr3.flags |= (1ull << i);

    __try {
      __writecr3(test_cr3.flags);

      // restore old CR3 after hypervisor pooped on it
      __writecr3(old_cr3.flags);

      // hypervisor should've raised an exception >:(
      _enable();

      return true;
    } __except (1) {
      // maybe the write passed through even though an exception was raised?
      if (__readcr3() != old_cr3.flags) {
        _enable();
        return true;
      }
    }
  }

  _enable();
  return false;
}

