#include <ia32.hpp>
#include <intrin.h>

bool debug_detected_1() {
  _disable();

  dr7 curr_dr7;
  curr_dr7.flags = __readdr(7);

  // write to DR7
  __writedr(7, 0x4FF);

  // trigger a vm-exit
  int tmp[4];
  __cpuid(tmp, 0);

  if (__readdr(7) != 0x4FF) {
    // restore DR7, although hypervisor will fuck with it anyways
    __writedr(7, curr_dr7.flags);

    _enable();
    return true;
  }

  // restore DR7, although hypervisor will fuck with it anyways
  __writedr(7, curr_dr7.flags);

  _enable();
  return false;
}

