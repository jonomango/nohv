#include <ntddk.h>

void driver_unload(PDRIVER_OBJECT) {

}

NTSTATUS driver_entry(PDRIVER_OBJECT driver, PUNICODE_STRING) {
  if (driver)
    driver->DriverUnload = driver_unload;

  return STATUS_SUCCESS;
}

