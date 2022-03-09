#include <ntddk.h>

#define EXEC_DETECTION(x)\
  if (x())\
    DbgPrint("[-] Failed check: " #x "().\n");\
  else\
    DbgPrint("[+] Passed check: " #x "().\n");

// cr0.cpp
bool cr0_detected_1();

// cr3.cpp
bool cr3_detected_1();
bool cr3_detected_2();

// timing.cpp
bool timing_detected_1();
bool timing_detected_2();

void driver_unload(PDRIVER_OBJECT) {
  DbgPrint("Driver unloaded.\n");
}

NTSTATUS driver_entry(PDRIVER_OBJECT driver, PUNICODE_STRING) {
  DbgPrint("Driver loaded.\n");

  driver->DriverUnload = driver_unload;

  // bind execution to a single logical processor
  auto const affinity = KeSetSystemAffinityThreadEx(1);

  // cr0.cpp
  EXEC_DETECTION(cr0_detected_1);

  // cr3.cpp
  EXEC_DETECTION(cr3_detected_1);
  EXEC_DETECTION(cr3_detected_2);

  // timing.cpp
  EXEC_DETECTION(timing_detected_1);
  EXEC_DETECTION(timing_detected_2);

  KeRevertToUserAffinityThreadEx(affinity);

  return STATUS_SUCCESS;
}

