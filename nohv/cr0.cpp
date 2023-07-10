#include <ia32.hpp>
#include <intrin.h>

// This detection checks to see if the hypervisor properly handles
// the guest modifying CR0.NE, which is usually reserved during VMX-operation.
bool cr0_detected_1() {
  _disable();

  cr0 curr_cr0;
  curr_cr0.flags = __readcr0();

  __try {
    // flip CR0.NE
    auto test_cr0 = curr_cr0;
    test_cr0.numeric_error = !test_cr0.numeric_error;
    __writecr0(test_cr0.flags);

    // check to see if the write actually went through
    if (__readcr0() != test_cr0.flags) {
      // restore CR0
      __writecr0(curr_cr0.flags);

      _enable();
      return true;
    }

    // restore CR0
    __writecr0(curr_cr0.flags);
  }
  __except (1) {
    _enable();
    return true;
  }

  _enable();
  return false;
}

// This detection tries to set reserved bits in CR0 (bits 63:32)
// that should trigger an exception.
// 
// Vol3[2.5(Control Registers)]
bool cr0_detected_2() {
  _disable();

  cr0 curr_cr0;
  curr_cr0.flags = __readcr0();

  for (int i = 32; i < 64; ++i) {
    __try {
      auto test_cr0 = curr_cr0;

      // set a reserved bit
      test_cr0.flags |= (1ull << i);

      // flip CR0.NE so that a vm-exit is triggered
      test_cr0.numeric_error = !test_cr0.numeric_error;

      // this should trigger an exception
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

// Some hypervisisors improperly handle reserved bits in cr0
// Attempting to set any reserved bits in CR0[31:0] is ignored.
bool cr0_detected_3() {
    _disable();

    cr0 curr_cr0;
    curr_cr0.flags = __readcr0();

    __try {
        auto test_cr0 = curr_cr0;

        // set reserved bits within cr0[31:0]
        test_cr0.reserved1 = 1;
        test_cr0.reserved2 = 1;
        test_cr0.reserved3 = 1;

        // flip CR0.NE so that a vm-exit is triggered
        test_cr0.numeric_error = !test_cr0.numeric_error;

        // this should not trigger an exception
        __writecr0(test_cr0.flags);

        // check that the bits were ignored
        if (__readcr0() == test_cr0.flags) {
            // restore correct cr0
            __writecr0(curr_cr0.flags);

            _enable();
            return true;
        }

        __writecr0(curr_cr0.flags);
    }
    __except (1) {
        // restore correct cr0
        __writecr0(curr_cr0.flags);

        _enable();
        return true;
    }

    _enable();
    return false;
}
