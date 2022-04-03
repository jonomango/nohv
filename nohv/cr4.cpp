#include <ia32.hpp>
#include <intrin.h>

// This detection checks to see if CR4.VMXE is set to 1.
// 
// Vol3[23.7(Enabling and Entering VMX Operation)]
bool cr4_detected_1() {
  cr4 curr_cr4;
  curr_cr4.flags = __readcr4();
  return curr_cr4.vmx_enable;
}

// This detection tries to flip CR4.VMXE and sees how the hypervisor reacts.
// 
// Vol3[23.7(Enabling and Entering VMX Operation)]
// Vol3[23.8(Restrictions on VMX Operation)]
bool cr4_detected_2() {
  _disable();

  cr4 curr_cr4;
  curr_cr4.flags = __readcr4();

  __try {
    auto test_cr4 = curr_cr4;
    test_cr4.vmx_enable = !test_cr4.vmx_enable;
    __writecr4(test_cr4.flags);

    // check if the write actually went through
    if (__readcr4() != test_cr4.flags) {
      // restore CR4
      __writecr4(curr_cr4.flags);

      _enable();
      return true;
    }

    // restore CR4
    __writecr4(curr_cr4.flags);

    // not sure how this would happen but might as well throw it in :)
    if (__readcr4() != curr_cr4.flags) {
      _enable();
      return true;
    }
  }
  __except (1) {
    // an exception should not have been raised...
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
bool cr4_detected_3() {
  _disable();

  cr4 curr_cr4;
  curr_cr4.flags = __readcr4();

  __try {
    auto test_cr4 = curr_cr4;

    // clear CR4.PAE
    test_cr4.physical_address_extension = 0;

    // flip CR4.VMXE to ensure that a vm-exit occurs
    test_cr4.vmx_enable = !test_cr4.vmx_enable;

    __writecr4(test_cr4.flags);

    // restore CR4
    __writecr4(curr_cr4.flags);

    // an exception should have been raised
    _enable();
    return true;
  }
  __except (1) {}

  __try {
    auto test_cr4 = curr_cr4;

    // set CR4.LA57
    test_cr4.linear_addresses_57_bit = 1;

    // flip CR4.VMXE to ensure that a vm-exit occurs
    test_cr4.vmx_enable = !test_cr4.vmx_enable;

    __writecr4(test_cr4.flags);

    // restore CR4
    __writecr4(curr_cr4.flags);

    // an exception should have been raised
    _enable();
    return true;
  }
  __except (1) {}

  // change CR4.PCIDE from 0 to 1 while CR3[11:0] != 000H
  __try {
    // TODO:
  }
  __except (1) {}

  _enable();
  return false;
}

// This d
bool cr4_detected_4() {
  _disable();

  cr4 curr_cr4;
  curr_cr4.flags = __readcr4();

  for (int i = 32; i < 64; ++i) {
    __try {
      auto test_cr4 = curr_cr4;

      // set a reserved bit
      test_cr4.flags |= (1ull << i);

      // flip CR4.VMXE to ensure that a vm-exit occurs
      test_cr4.vmx_enable = !test_cr4.vmx_enable;

      // this should trigger an exception
      __writecr4(test_cr4.flags);

      // restore CR4 after the hypervisor mucked it
      __writecr4(curr_cr4.flags);

      _enable();
      return true;
    } __except (1) {
      // maybe the write went through even though an exception was raised?
      if (curr_cr4.flags != __readcr4()) {
        // restore CR4 after the hypervisor mucked it
        __writecr0(curr_cr4.flags);

        _enable();
        return true;
      }
    }
  }

  _enable();
  return false;
}
