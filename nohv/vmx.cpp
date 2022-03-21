#include <ia32.hpp>
#include <intrin.h>
#include <ntddk.h>

// This detection tries to execute VMXON while CR4.VMXE is
// clear and checks to see if a #UD was successfully raised.
// 
// Vol3[30.3(VMXON - Enter VMX Operaton)
bool vmx_detected_1() {
  _disable();

  cr4 curr_cr4;
  curr_cr4.flags = __readcr4();

  // clear CR4.VMXE
  __try {
    auto test_cr4 = curr_cr4;
    test_cr4.vmx_enable = 0;
    __writecr4(test_cr4.flags);
  }
  __except (1) {
    _enable();
    return true;
  }

  bool detected = false;
  unsigned long ecode = 0;

  // execute VMXON (VMXON region shouldn't matter since an
  // exception should be raised before operand is even checked)
  __try {
    __vmx_on(nullptr);

    // uh... how did we even end up here?
    detected = true;
  }
  __except (ecode = GetExceptionCode(), 1) {
    // check if a #UD was raised
    detected = (ecode != STATUS_ILLEGAL_INSTRUCTION);
  }

  // restore CR4
  __writecr4(curr_cr4.flags);

  _enable();
  return detected;
}

// This detection tries to execute VMXON with an invalid operand
// and checks to see if a VM error code was properly returned.
// 
// Vol3[30.3(VMXON - Enter VMX Operaton)
bool vmx_detected_2() {
  _disable();

  ia32_feature_control_register feature_control;
  feature_control.flags = __readmsr(IA32_FEATURE_CONTROL);

  // check if VMX has been disabled by BIOS
  if (!feature_control.lock_bit || !feature_control.enable_vmx_outside_smx) {
    _enable();
    return false;
  }

  cr0 curr_cr0;
  cr4 curr_cr4;

  curr_cr0.flags = __readcr0();
  curr_cr4.flags = __readcr4();

  // configure CR0 and CR4 for VMX operation
  __try {
    auto test_cr0 = curr_cr0;
    auto test_cr4 = curr_cr4;

    test_cr4.vmx_enable = 1;

    test_cr0.flags |= __readmsr(IA32_VMX_CR0_FIXED0);
    test_cr0.flags &= __readmsr(IA32_VMX_CR0_FIXED1);
    test_cr4.flags |= __readmsr(IA32_VMX_CR4_FIXED0);
    test_cr4.flags &= __readmsr(IA32_VMX_CR4_FIXED1);

    __writecr0(test_cr0.flags);
    __writecr4(test_cr4.flags);
  }
  __except (1) {
    // restore CR0 and CR4
    __writecr0(curr_cr0.flags);
    __writecr4(curr_cr4.flags);

    _enable();
    return true;
  }

  bool detected = false;

  __try {
    unsigned long long address = MAXULONG64;
    auto const ret = __vmx_on(&address);

    // VMXON was successful... bro?
    if (ret == 0)
      detected = true;
    // extended status should not be available since there's no current VMCS
    else if (ret == 1)
      detected = true;
  }
  __except (1) {
    // an exception should not have been raised...
    detected = true;
  }

  // restore CR0 and CR4
  __writecr0(curr_cr0.flags);
  __writecr4(curr_cr4.flags);

  _enable();
  return detected;
}

// This function executes the VMCALL instruction.
extern "C" void vmx_vmcall(uint64_t rcx, uint64_t rdx, uint64_t r8, uint64_t r9);

// This detection tries to execute VMCALL and checks if a #UD was
// correctly raised (since we're not in VMX operation).
bool vmx_detected_3() {
  // we do a lil' bruteforcin
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 20; ++j) {
      unsigned long ecode = 0;

      __try {
        uint64_t args[4] = {};
        args[i] = j;

        vmx_vmcall(args[0], args[1], args[2], args[3]);

        // an exception should've been raised
        return true;
      }
      __except (ecode = GetExceptionCode(), 1) {
        // make sure they injected the correct exception
        if (ecode != STATUS_ILLEGAL_INSTRUCTION)
          return true;
      }
    }
  }

  return false;
}

