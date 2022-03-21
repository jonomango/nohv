#include <ntddk.h>

#include "detections.h"

#define EXEC_DETECTION(x)\
  if (x())\
    DbgPrint("[-] Failed check: " #x "().\n");\
  else\
    DbgPrint("[+] Passed check: " #x "().\n");

void driver_unload(PDRIVER_OBJECT) {
  DbgPrint("Driver unloaded.\n");
}

NTSTATUS driver_entry(PDRIVER_OBJECT driver, PUNICODE_STRING) {
  DbgPrint("Driver loaded.\n");

  driver->DriverUnload = driver_unload;

  // bind execution to a single logical processor
  auto const affinity = KeSetSystemAffinityThreadEx(1);

  // cpuid.cpp
  DbgPrint("Testing cpuid.cpp:\n");
  EXEC_DETECTION(cpuid_detected_1);

  // msr.cpp
  DbgPrint("Testing msr.cpp:\n");
  EXEC_DETECTION(msr_detected_1);

  // cr0.cpp
  DbgPrint("Testing cr0.cpp:\n");
  EXEC_DETECTION(cr0_detected_1);
  EXEC_DETECTION(cr0_detected_2);

  // cr3.cpp
  DbgPrint("Testing cr3.cpp:\n");
  EXEC_DETECTION(cr3_detected_1);
  EXEC_DETECTION(cr3_detected_2);

  // cr4.cpp
  DbgPrint("Testing cr4.cpp:\n");
  EXEC_DETECTION(cr4_detected_1);
  EXEC_DETECTION(cr4_detected_2);
  EXEC_DETECTION(cr4_detected_3);

  // xsetbv.cpp
  DbgPrint("Testing xsetbv.cpp:\n");
  EXEC_DETECTION(xsetbv_detected_1);
  EXEC_DETECTION(xsetbv_detected_2);
  EXEC_DETECTION(xsetbv_detected_3);
  EXEC_DETECTION(xsetbv_detected_4);
  EXEC_DETECTION(xsetbv_detected_5);

  // timing.cpp
  DbgPrint("Testing timing.cpp:\n");
  EXEC_DETECTION(timing_detected_1);
  EXEC_DETECTION(timing_detected_2);

  // debug.cpp
  DbgPrint("Testing debug.cpp:\n");
  EXEC_DETECTION(debug_detected_1);

  // vmx.cpp
  DbgPrint("Testing vmx.cpp:\n");
  EXEC_DETECTION(vmx_detected_1);
  EXEC_DETECTION(vmx_detected_2);
  EXEC_DETECTION(vmx_detected_3);

  KeRevertToUserAffinityThreadEx(affinity);

  return STATUS_SUCCESS;
}

