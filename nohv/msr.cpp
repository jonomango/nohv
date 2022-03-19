#include <intrin.h>

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

