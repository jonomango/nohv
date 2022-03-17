#include <ia32.hpp>
#include <intrin.h>


// This detection tries to write to an XCR that is not supported.
// 
// Vol3[2.6(Extended Control Registers (Including XCR0))]
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
// Vol3[2.6(Extended Control Registers (Including XCR0))]
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

// This function executes XSETBV with full
// 64-bit registers set to whatever specified.
extern "C" void xsetbv_full(uint64_t rcx, uint64_t rdx, uint64_t rax);

// This detection tries to catch hypervisors that incorrectly include
// the high part of the RAX register when emulating XSETBV.
// 
// Vol2[5.2(XSETBV - Set Extended Control Register)]
// https://github.com/wbenny/hvpp/blob/84b3f3c241e1eec3ab42f75cad9deef3ad67e6ab/src/hvpp/hvpp/vmexit/vmexit_passthrough.cpp#L959
// https://github.com/eyalz800/zpp_hypervisor/blob/master/hypervisor/src/hypervisor/hypervisor.cpp#L1021
// https://github.com/ionescu007/SimpleVisor/blob/989d33b1bc6569965d7aad3bd50a8d35fa4c359e/shvvmxhv.c#L163
// https://github.com/HyperDbg/HyperDbg/blob/06c4ea79d93fe6e9851e4ce9c0a8bdb4eb0fb0a6/hyperdbg/hprdbghv/code/vmm/vmx/Vmexit.c#L402
bool xsetbv_detected_3() {
  _disable();

  xcr0 curr_xcr0;
  curr_xcr0.flags = _xgetbv(0);

  cpuid_eax_0d_ecx_00 cpuid_0d;
  __cpuidex(reinterpret_cast<int*>(&cpuid_0d), 0x0D, 0x00);
  
  // features that are unsupported in the high part of XCR0
  auto const unsupported_mask = static_cast<uint64_t>(cpuid_0d.edx.flags);

  __try {
    xsetbv_full(0, curr_xcr0.flags >> 32,
      (curr_xcr0.flags & 0xFFFF'FFFF) | unsupported_mask);
  }
  __except (1) {
    // no exception should be raised since the high part of RAX should be ignored...
    _enable();
    return true;
  }

  _enable();
  return false;
}

// This detection tries to catch hypervisors that incorrectly include
// the high part of the RCX register when emulating XSETBV.
// 
// Vol2[5.2(XSETBV - Set Extended Control Register)]
bool xsetbv_detected_4() {
  _disable();

  xcr0 curr_xcr0;
  curr_xcr0.flags = _xgetbv(0);

  __try {
    xsetbv_full(69ull << 32, curr_xcr0.flags << 32,
      curr_xcr0.flags & 0xFFFF'FFFF);
  }
  __except (1) {
    // no exception should be raised since the high part of RCX should be ignored...
    _enable();
    return true;
  }

  _enable();
  return false;
}

// This detection tries to write an invalid combination to XCR0.
// 
// Vol3[2.6(Extended Control Registers (Including XCR0))]
bool xsetbv_detected_5() {
  _disable();

  xcr0 curr_xcr0;
  curr_xcr0.flags = _xgetbv(0);

  // clear XCR0.x87
  __try {
    auto test_xcr0 = curr_xcr0;
    test_xcr0.x87 = 0;
    _xsetbv(0, test_xcr0.flags);

    // an exception should have been raised
    _enable();
    return true;
  }
  __except (1) {}

  // clear XCR0.SSE and set XCR0.AVX
  __try {
    auto test_xcr0 = curr_xcr0;
    test_xcr0.sse = 0;
    test_xcr0.avx = 1;
    _xsetbv(0, test_xcr0.flags);

    // an exception should have been raised
    _enable();
    return true;
  }
  __except (1) {}

  // clear XCR0.AVX and set any of XCR0.opmask, XCR0.ZMM_Hi256, and XCR0.Hi16_ZMM
  __try {
    auto test_xcr0 = curr_xcr0;
    test_xcr0.avx    = 0;
    test_xcr0.opmask = 1;
    _xsetbv(0, test_xcr0.flags);

    // an exception should have been raised
    _enable();
    return true;
  }
  __except (1) {}

  // clear XCR0.AVX and set any of XCR0.opmask, XCR0.ZMM_Hi256, and XCR0.Hi16_ZMM
  __try {
    auto test_xcr0 = curr_xcr0;
    test_xcr0.avx       = 0;
    test_xcr0.zmm_hi256 = 1;
    _xsetbv(0, test_xcr0.flags);

    // an exception should have been raised
    _enable();
    return true;
  }
  __except (1) {}

  // clear XCR0.AVX and set any of XCR0.opmask, XCR0.ZMM_Hi256, and XCR0.Hi16_ZMM
  __try {
    auto test_xcr0 = curr_xcr0;
    test_xcr0.avx      = 0;
    test_xcr0.zmm_hi16 = 1;
    _xsetbv(0, test_xcr0.flags);

    // an exception should have been raised
    _enable();
    return true;
  }
  __except (1) {}

  // set either XCR0.BNDREG and XCR0.BNDCSR while not setting the other
  __try {
    auto test_xcr0 = curr_xcr0;
    test_xcr0.bndreg = 0;
    test_xcr0.bndcsr = 1;
    _xsetbv(0, test_xcr0.flags);

    // an exception should have been raised
    _enable();
    return true;
  }
  __except (1) {}

  // set either XCR0.BNDREG and XCR0.BNDCSR while not setting the other
  __try {
    auto test_xcr0 = curr_xcr0;
    test_xcr0.bndreg = 1;
    test_xcr0.bndcsr = 0;
    _xsetbv(0, test_xcr0.flags);

    // an exception should have been raised
    _enable();
    return true;
  }
  __except (1) {}

  // set any of XCR0.opmask, XCR0.ZMM_Hi256, and
  // XCR0.Hi16_ZMM while not setting all of them
  __try {
    auto test_xcr0 = curr_xcr0;
    test_xcr0.opmask    = 0;
    test_xcr0.zmm_hi256 = 1;
    test_xcr0.zmm_hi16  = 1;
    _xsetbv(0, test_xcr0.flags);

    // an exception should have been raised
    _enable();
    return true;
  }
  __except (1) {}

  // set any of XCR0.opmask, XCR0.ZMM_Hi256, and
  // XCR0.Hi16_ZMM while not setting all of them
  __try {
    auto test_xcr0 = curr_xcr0;
    test_xcr0.opmask    = 1;
    test_xcr0.zmm_hi256 = 0;
    test_xcr0.zmm_hi16  = 1;
    _xsetbv(0, test_xcr0.flags);

    // an exception should have been raised
    _enable();
    return true;
  }
  __except (1) {}

  // set any of XCR0.opmask, XCR0.ZMM_Hi256, and
  // XCR0.Hi16_ZMM while not setting all of them
  __try {
    auto test_xcr0 = curr_xcr0;
    test_xcr0.opmask    = 1;
    test_xcr0.zmm_hi256 = 1;
    test_xcr0.zmm_hi16  = 0;
    _xsetbv(0, test_xcr0.flags);

    // an exception should have been raised
    _enable();
    return true;
  }
  __except (1) {}

  _enable();
  return false;
}

