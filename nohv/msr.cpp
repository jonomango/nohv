#include <ia32.hpp>
#include <intrin.h>
#include <ntddk.h>

// This detection tries to read from synthetic MSRs and checks if
// an exception is properly raised.
bool msr_detected_1() {
  for (unsigned int msr = 0x4000'0000; msr <= 0x4000'00FF; ++msr) {
    __try {
      __readmsr(msr);

      // an exception should have been raised
      return true;
    }
    __except (1) {}
  }

  return false;
}

// This detection checks to see if the hypervisor lets the guest read
// the MPERF and APERF MSRs while CPUID reports that they are not supported.
bool msr_detected_2() {
  _disable();

  cpuid_eax_06 cpuid_06;
  __cpuid(reinterpret_cast<int*>(&cpuid_06), 6);

  // IA32_MPERF/IA32_APERF MSRs are supported
  if (cpuid_06.ecx.hardware_coordination_feedback_capability) {
    _enable();
    return false;
  }

  __try {
    __readmsr(IA32_MPERF);

    // an exception should be thrown since these registers are not supported
    _enable();
    return true;
  }
  __except (1) {}

  __try {
    __readmsr(IA32_APERF);

    // an exception should be thrown since these registers are not supported
    _enable();
    return true;
  }
  __except (1) {}

  _enable();
  return false;
}

