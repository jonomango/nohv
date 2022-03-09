#include <ia32.hpp>
#include <intrin.h>
#include <ntddk.h>

// Classic timing detection that checks if the time to
// execute the CPUID instruction is suspiciously large.
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

// IPI callback that executes CPUID in a loop on every logical processor.
ULONG_PTR ipi_callback(ULONG_PTR const context) {
  size_t& detected_count = *reinterpret_cast<size_t*>(context);

  for (size_t i = 0; i < 1000; ++i) {
    int regs[4] = {};

    _mm_lfence();
    auto const start = __rdtsc();
    _mm_lfence();

    __cpuid(regs, 0);

    _mm_lfence();
    auto const end = __rdtsc();
    _mm_lfence();

    auto const delta = (end - start);

    // TSC delta went negative
    if (delta & (1ull << 63))
      ++detected_count;
  }

  return 0;
}

// This timing detection tries to simultaneously execute an unconditionally
// vm-exiting instruction on every logical processor in order to catch
// hypervisors that use a shared TSC offset to bypass timing checks. Using
// a shared TSC offset will cause the TSC delta to go negative, since it
// was lowered in another logical processor.
bool timing_detected_2() {
  size_t detected_count = 0;
  KeIpiGenericCall(ipi_callback, reinterpret_cast<ULONG_PTR>(&detected_count));
  return (detected_count > 0);
}

