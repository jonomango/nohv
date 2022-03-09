#include <ia32.hpp>
#include <intrin.h>

// classing timing detection that checks if the time to
// execute the cpuid instruction is suspiciously large.
bool timing_detected_1() {
  _disable();

  uint64_t lowest_tsc = ULLONG_MAX;

  // we only care about the lowest TSC for reliability since an NMI,
  // an SMI, or TurboBoost could fuck up our timings.
  for (int i = 0; i < 10; ++i) {
    int regs[4] = {};

    _mm_lfence();
    auto const start = __rdtsc();
    _mm_lfence();

    __cpuid(regs, 0);

    _mm_lfence();
    auto const end = __rdtsc();
    _mm_lfence();

    auto const delta = (end - start);
    if (delta < lowest_tsc)
      lowest_tsc = delta;

    // they over-accounted and TSC delta went negative
    if (delta & (1ull << 63)) {
      _enable();
      return true;
    }
  }

  _enable();
  return (lowest_tsc > 500);
}

