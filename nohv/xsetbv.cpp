#include <ia32.hpp>
#include <intrin.h>


// This detection tries to write to an XCR that is not supported.
bool xsetbv_detected_1() {
  _disable();

  __try {
    // try to write to XCR69
    _xsetbv(69, _xgetbv(0));

    // an exception should have been raised...
    _enable();
    return true;
  }
  __except (1) {}

  _enable();
  return false;
}

// This detection tries to set every unsupported bit in XCR0.
// 
// Vol3C[2.6(Extended Control Registers (Including XCR0))]
bool xsetbv_detected_2() {
  _disable();

  xcr0 curr_xcr0;
  curr_xcr0.flags = _xgetbv(0);

  cpuid_eax_0d_ecx_00 cpuid_0d;
  __cpuidex(reinterpret_cast<int*>(&cpuid_0d), 0x0D, 0x00);
  
  // features in XCR0 that are supported
  auto const supported_mask = (static_cast<uint64_t>(
    cpuid_0d.edx.flags) << 32) | cpuid_0d.eax.flags;

  for (int i = 0; i < 64; ++i) {
    // this is a bit dumb but it works well enough so whatever
    if (supported_mask & (1ull << i))
      continue;

    __try {
      auto test_xcr0 = curr_xcr0;
      test_xcr0.flags |= (1ull << i);

      _xsetbv(0, test_xcr0.flags);

      // restore XCR0 after the hypervisor mucked it
      _xsetbv(0, curr_xcr0.flags);

      // an exception should have been raised...
      _enable();
      return true;
    }
    __except (1) {
      // maybe the write went through even though an exception was raised?
      if (curr_xcr0.flags != _xgetbv(0)) {
        // restore XCR0 after the hypervisor mucked it
        _xsetbv(0, curr_xcr0.flags);

        _enable();
        return true;
      }
    }
  }

  _enable();
  return false;
}

