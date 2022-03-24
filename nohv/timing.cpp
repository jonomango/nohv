#include <ia32.hpp>
#include <intrin.h>
#include <ntddk.h>

inline constexpr size_t max_acceptable_tsc = 500;

// Classic timing detection that checks if the time to
// execute the CPUID instruction is suspiciously large.
bool timing_detected_1() {
  _disable();

  uint64_t lowest_tsc = MAXULONG64;

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
  return (lowest_tsc > max_acceptable_tsc);
}

// IPI callback that executes CPUID in a loop on every logical processor.
ULONG_PTR ipi_callback(ULONG_PTR const context) {
  size_t& detected_count = *reinterpret_cast<size_t*>(context);

  for (size_t i = 0; i < 100; ++i) {
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

// This detection uses CPU_CLK_UNHALTED.REF_TSC to measure the
// execution time of the CPUID instruction.
// 
// Vol3[19.2.2(Architectural Performance Monitoring Version 2)]
bool timing_detected_3() {
  _disable();

  ia32_fixed_ctr_ctrl_register curr_fixed_ctr_ctrl;
  curr_fixed_ctr_ctrl.flags = __readmsr(IA32_FIXED_CTR_CTRL);

  ia32_perf_global_ctrl_register curr_perf_global_ctrl;
  curr_perf_global_ctrl.flags = __readmsr(IA32_PERF_GLOBAL_CTRL);

  // enable fixed counter #2
  auto new_fixed_ctr_ctrl = curr_fixed_ctr_ctrl;
  new_fixed_ctr_ctrl.en2_os      = 1;
  new_fixed_ctr_ctrl.en2_usr     = 0;
  new_fixed_ctr_ctrl.en2_pmi     = 0;
  new_fixed_ctr_ctrl.any_thread2 = 0;
  __writemsr(IA32_FIXED_CTR_CTRL, new_fixed_ctr_ctrl.flags);

  // enable fixed counter #2
  auto new_perf_global_ctrl = curr_perf_global_ctrl;
  new_perf_global_ctrl.en_fixed_ctrn |= (1ull << 2);
  __writemsr(IA32_PERF_GLOBAL_CTRL, new_perf_global_ctrl.flags);

  bool detected = false;
  uint64_t lowest_tsc = MAXULONG64;

  // we only care about the lowest TSC for reliability since an NMI,
  // an SMI, or TurboBoost could fuck up our timings.
  for (int i = 0; i < 10; ++i) {
    int regs[4] = {};

    _mm_lfence();
    auto const start = __readmsr(IA32_FIXED_CTR2);
    _mm_lfence();

    __cpuid(regs, 0);

    _mm_lfence();
    auto const end = __readmsr(IA32_FIXED_CTR2);
    _mm_lfence();

    auto const delta = (end - start);
    if (delta < lowest_tsc)
      lowest_tsc = delta;

    // they over-accounted and TSC delta went negative
    if (delta & (1ull << 63))
      detected = true;
  }

  if (lowest_tsc > max_acceptable_tsc)
    detected = true;

  // restore MSRs
  __writemsr(IA32_PERF_GLOBAL_CTRL, curr_perf_global_ctrl.flags);
  __writemsr(IA32_FIXED_CTR_CTRL, curr_fixed_ctr_ctrl.flags);

  _enable();
  return detected;
}

