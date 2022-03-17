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

// This detection tries to enable CR4.VMXE and sees how the hypervisor reacts.
// 
// Vol3[23.7(Enabling and Entering VMX Operation)]
// Vol3[23.8(Restrictions on VMX Operation)]
bool cr4_detected_2() {
  _disable();

  ia32_feature_control_register feature_control;
  feature_control.flags = __readmsr(IA32_FEATURE_CONTROL);

  // VMX not enabled by BIOS.
  // TODO:
  //   if this check fails, would setting CR4.VMXE cause a #GP?
  if (!feature_control.lock_bit || !feature_control.enable_vmx_outside_smx) {
    _enable();
    return false;
  }

  cr4 curr_cr4;
  curr_cr4.flags = __readcr4();

  __try {
    auto test_cr4 = curr_cr4;
    test_cr4.vmx_enable = 1;
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

