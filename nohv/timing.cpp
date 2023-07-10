#include <ia32.hpp>
#include <intrin.h>
#include <ntddk.h>

// Hardcoded execution times for CPUID instruction.
inline constexpr size_t max_acceptable_tsc   = 500;
inline constexpr size_t max_acceptable_mperf = 500;
inline constexpr size_t max_acceptable_aperf = 500;

// Classic timing detection that checks if the time to
// execute the CPUID instruction is suspiciously large. This
// check uses the TSC to measure execution time.
bool timing_detected_1() {
  _disable();

  uint64_t lowest_tsc = MAXULONG64;

  // we only care about the lowest TSC delta for reliability since an NMI,
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
static ULONG_PTR ipi_callback(ULONG_PTR const context) {
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

// Classic timing detection that checks if the time to
// execute the CPUID instruction is suspiciously large. This
// check uses the MPERF to measure execution time.
bool timing_detected_4() {
  _disable();

  cpuid_eax_06 cpuid_06;
  __cpuid(reinterpret_cast<int*>(&cpuid_06), 6);

  // IA32_MPERF/IA32_APERF MSRs are not supported
  if (!cpuid_06.ecx.hardware_coordination_feedback_capability) {
    _enable();
    return false;
  }

  uint64_t lowest_mperf = MAXULONG64;

  // we only care about the lowest MPERF delta for reliability since an NMI,
  // an SMI, or TurboBoost could fuck up our timings.
  for (int i = 0; i < 10; ++i) {
    int regs[4] = {};

    _mm_lfence();
    auto const start = __readmsr(IA32_MPERF);
    _mm_lfence();

    __cpuid(regs, 0);

    _mm_lfence();
    auto const end = __readmsr(IA32_MPERF);
    _mm_lfence();

    auto const delta = (end - start);
    if (delta < lowest_mperf)
      lowest_mperf = delta;

    // they over-accounted and MPERF delta went negative
    if (delta & (1ull << 63)) {
      _enable();
      return true;
    }
  }

  _enable();
  return (lowest_mperf > max_acceptable_mperf)
      || (lowest_mperf <= 10);
}

// Classic timing detection that checks if the time to
// execute the CPUID instruction is suspiciously large. This
// check uses the APERF to measure execution time.
bool timing_detected_5() {
  _disable();

  cpuid_eax_06 cpuid_06;
  __cpuid(reinterpret_cast<int*>(&cpuid_06), 6);

  // IA32_MPERF/IA32_APERF MSRs are not supported
  if (!cpuid_06.ecx.hardware_coordination_feedback_capability) {
    _enable();
    return false;
  }

  uint64_t lowest_aperf = MAXULONG64;

  // we only care about the lowest APERF delta for reliability since an NMI,
  // an SMI, or TurboBoost could fuck up our timings.
  for (int i = 0; i < 10; ++i) {
    int regs[4] = {};

    _mm_lfence();
    auto const start = __readmsr(IA32_APERF);
    _mm_lfence();

    __cpuid(regs, 0);

    _mm_lfence();
    auto const end = __readmsr(IA32_APERF);
    _mm_lfence();

    auto const delta = (end - start);
    if (delta < lowest_aperf)
      lowest_aperf = delta;

    // they over-accounted and APERF delta went negative
    if (delta & (1ull << 63)) {
      _enable();
      return true;
    }
  }

  _enable();
  return (lowest_aperf > max_acceptable_aperf)
      || (lowest_aperf <= 10);
}

// Measures the amount of time it takes to read+write
// to every byte in the specified array.
static uint64_t time_cacheline(uint8_t cacheline[64]) {
  // touch the memory and ensure that it is in the cache
  cacheline[0] = 1;

  _mm_lfence();
  auto const start = __rdtsc();
  _mm_lfence();

  for (int i = 0; i < 64; ++i)
    cacheline[i] += 1;

  _mm_lfence();
  auto const end = __rdtsc();
  _mm_lfence();

  return (end - start);
}

// This detection tries to catch hypervisors that fail to update the memory
// types in the EPT paging structures after the guest disables caching.
// 
// Vol3[11.5.3(Preventing Caching)]
// Vol3[11.11(Memory Type Range Registers (MTRRs))]
bool timing_detected_6() {
  _disable();

  cr0 curr_cr0;
  curr_cr0.flags = __readcr0();

  ia32_mtrr_def_type_register curr_mtrr_def_type;
  curr_mtrr_def_type.flags = __readmsr(IA32_MTRR_DEF_TYPE);

  // a cacheline that we'll be using to determine whether the memory
  // typing is WB or UC.
  alignas(64) uint8_t cacheline[64] = {};

  // amount of time to access WB memory that is in the cache
  uint64_t wb_timing = MAXUINT64;

  for (int i = 0; i < 10; ++i) {
    auto const timing = time_cacheline(cacheline);
    if (timing < wb_timing)
      wb_timing = timing;
  }

  // set CR0.CD to 1
  __try {
    auto test_cr0 = curr_cr0;
    test_cr0.cache_disable = 1;
    __writecr0(test_cr0.flags);
  }
  __except (1) {
    // an exception shouldn't be thrown
    _enable();
    return true;
  }

  // invalidate the cache since the processor can still use
  // existing cache lines if they exist
  __wbinvd();

  // disable caching through the MTRRs
  auto test_mtrr_def_type = curr_mtrr_def_type;
  test_mtrr_def_type.mtrr_enable         = 0;
  test_mtrr_def_type.default_memory_type = MEMORY_TYPE_UNCACHEABLE;
  __writemsr(IA32_MTRR_DEF_TYPE, test_mtrr_def_type.flags);

  // invalidate the cache again for Pentium 4 and Intel Xeon processors
  __wbinvd();

  // amount of time to access UC memory that is in the cache
  uint64_t uc_timing = MAXUINT64;

  for (int i = 0; i < 10; ++i) {
    auto const timing = time_cacheline(cacheline);
    if (timing < uc_timing)
      uc_timing = timing;
  }

  // restore MTRRs
  __writemsr(IA32_MTRR_DEF_TYPE, curr_mtrr_def_type.flags);

  // restore CR0
  __writecr0(curr_cr0.flags);

  _enable();
  return (uc_timing < wb_timing * 40);
}

extern "C" bool check_rdtscp_regs();

// This detection occurs due to an improper implementation of rdtscp
// On processors that support the Intel 64 architecture, the high - order 32 bits of each of RAX, RDX, and RCX are cleared.
bool timing_detected_7() {
  return check_rdtscp_regs();
}