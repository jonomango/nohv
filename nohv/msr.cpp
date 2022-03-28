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
bool msr_detected_2() {
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

