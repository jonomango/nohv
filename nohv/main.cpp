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

  // cr0.cpp
  DbgPrint("Testing cr0.cpp:\n");
  EXEC_DETECTION(cr0_detected_1);

  // cr3.cpp
  DbgPrint("Testing cr3.cpp:\n");
  EXEC_DETECTION(cr3_detected_1);
  EXEC_DETECTION(cr3_detected_2);

  // xsetbv.cpp
  DbgPrint("Testing xsetbv.cpp:\n");
  EXEC_DETECTION(xsetbv_detected_1);
  EXEC_DETECTION(xsetbv_detected_2);
  EXEC_DETECTION(xsetbv_detected_3);
  EXEC_DETECTION(xsetbv_detected_4);

  // timing.cpp
  DbgPrint("Testing timing.cpp:\n");
  EXEC_DETECTION(timing_detected_1);
  EXEC_DETECTION(timing_detected_2);

  KeRevertToUserAffinityThreadEx(affinity);

  return STATUS_SUCCESS;
}

