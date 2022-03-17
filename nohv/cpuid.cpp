#include <ia32.hpp>
#include <intrin.h>

// This detection checks to see if the hypervisor-present bit
// is set in CPUID leaf 0x1.
bool cpuid_detected_1() {
  cpuid_eax_01 cpuid_01;
  __cpuid(reinterpret_cast<int*>(&cpuid_01), 1);

  // bit 31 of ECX is the hypervisor present bit
  return cpuid_01.cpuid_feature_information_ecx.flags & (1 << 31);
}

