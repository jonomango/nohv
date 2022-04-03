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
// types in the EPT paging structures after the guest modifies MTRR values.
// 
// Vol3[11.11(Memory Type Range Registers (MTRRs))]
bool timing_detected_4() {
  // a cacheline that we'll be using to determine whether the memory
  // typing is WB or UC.
  alignas(64) uint8_t cacheline[64] = {};

  // page frame number of cacheline[]
  auto const cacheline_pfn = MmGetPhysicalAddress(&cacheline).QuadPart >> 12;

  _disable();

  ia32_mtrr_def_type_register curr_mtrr_def_type;
  curr_mtrr_def_type.flags = __readmsr(IA32_MTRR_DEF_TYPE);

  ia32_mtrr_capabilities_register curr_mtrr_cap;
  curr_mtrr_cap.flags = __readmsr(IA32_MTRR_CAPABILITIES);

  // amount of time to access WB memory that is in the cache
  uint64_t wb_timing = MAXUINT64;

  for (int i = 0; i < 10; ++i) {
    auto const timing = time_cacheline(cacheline);
    if (timing < wb_timing)
      wb_timing = timing;
  }

  ia32_mtrr_physbase_register curr_mtrr_physbase;
  curr_mtrr_physbase.flags = 0;

  int mtrr_physbase_idx = -1;

  // find the variable-range MTRR that covers cacheline[]
  for (int i = 0; i < curr_mtrr_cap.variable_range_count; ++i) {
    ia32_mtrr_physmask_register physmask;
    physmask.flags = __readmsr(IA32_MTRR_PHYSMASK0 + i * 2);

    if (!physmask.valid)
      continue;

    ia32_mtrr_physbase_register physbase;
    physbase.flags = __readmsr(IA32_MTRR_PHYSBASE0 + i * 2);

    if (physbase.type != MEMORY_TYPE_WRITE_BACK)
      continue;

    auto const base = physbase.page_frame_number;
    auto const mask = physmask.page_frame_number;

    if ((cacheline_pfn & mask) == (base & mask)) {
      // change the memory type to UC
      auto test_physbase = physbase;
      test_physbase.type = MEMORY_TYPE_UNCACHEABLE;
      __writemsr(IA32_MTRR_PHYSBASE0 + i * 2, test_physbase.flags);

      // store current value so we can restore later on
      curr_mtrr_physbase = physbase;
      mtrr_physbase_idx = i;
      break;
    }
  }

  // make sure the default memory type is UC if we can't find the address
  // in the variable-range MTRRs
  auto test_mtrr_def_type = curr_mtrr_def_type;
  test_mtrr_def_type.default_memory_type = MEMORY_TYPE_UNCACHEABLE;
  __writemsr(IA32_MTRR_DEF_TYPE, test_mtrr_def_type.flags);

  // amount of time to access UC memory
  uint64_t uc_timing = MAXUINT64;

  for (int i = 0; i < 10; ++i) {
    auto const timing = time_cacheline(cacheline);
    if (timing < uc_timing)
      uc_timing = timing;
  }

  // restore MTRR_DEF_TYPE MSR
  __writemsr(IA32_MTRR_DEF_TYPE, curr_mtrr_def_type.flags);

  // restore MTRR_PHYSBASE MSR
  if (mtrr_physbase_idx != -1)
    __writemsr(IA32_MTRR_PHYSBASE0 + mtrr_physbase_idx * 2, curr_mtrr_physbase.flags);

  _enable();

  // accessing UC memory should be atleast 40x slower than accessing WB memory.
  // usually its around 400x slower, sometimes even reaching 700x slower if
  // no variable-range MTRRs are used and all memory is set to UC (since now
  // even instructions are uncached).
  return (uc_timing < wb_timing * 40);
}

// This detection is similar to timing_detected_4(), although it checks to
// see if the hypervisor properly updates the EPT memory types after CR0.CD
// is modified.
bool timing_detected_5() {
  _disable();

  cr0 curr_cr0;
  curr_cr0.flags = __readcr0();

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

  // amount of time to access WB memory that is in the cache
  uint64_t uc_timing = MAXUINT64;

  for (int i = 0; i < 10; ++i) {
    auto const timing = time_cacheline(cacheline);
    if (timing < uc_timing)
      uc_timing = timing;
  }

  // restore CR0
  __writecr0(curr_cr0.flags);

  _enable();
  return (uc_timing < wb_timing * 40);
}

