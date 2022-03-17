#include <ia32.hpp>
#include <intrin.h>

// This detection tries to set reserved bits in CR0 (bits 63:32)
// that should trigger an exception.
// TODO:
//   I haven't tested this out yet but it seems like a vm-exit isn't
//   even triggered when the guest tries to write to reserved bits. If
//   this is the case then this detection is useless :(.
bool cr0_detected_1() {
  _disable();

  cr0 curr_cr0;
  curr_cr0.flags = __readcr0();

  for (int i = 32; i < 64; ++i) {
    __try {
      auto test_cr0 = curr_cr0;
      test_cr0.flags |= (1ull << i);
      __writecr0(test_cr0.flags);

      // restore CR0 after the hypervisor mucked it
      __writecr0(curr_cr0.flags);

      _enable();
      return true;
    } __except (1) {
      // maybe the write went through even though an exception was raised?
      if (curr_cr0.flags != __readcr0()) {
        // restore CR0 after the hypervisor mucked it
        __writecr0(curr_cr0.flags);

        _enable();
        return true;
      }
    }
  }

  _enable();
  return false;
}

