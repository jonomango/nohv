#include <ia32.hpp>
#include <intrin.h>


// This detection tries to write to an XCR that is not supported.
// An exception should be raised under normal operation.
bool xsetbv_detected_1() {
  _disable();

  __try {
    // try to write to XCR69
    _xsetbv(69, _xgetbv(0));

    // an exception should have been raised...
    _enable();
    return true;
  }
  __except (1) {}

  _enable();
  return false;
}

