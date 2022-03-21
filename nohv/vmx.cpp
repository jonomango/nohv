#include <ia32.hpp>
#include <intrin.h>
#include <ntddk.h>

// This detection tries to execute the VMXON instruction while CR4.VMXE is
// clear and checks to see if a #UD was successfully raised.
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

