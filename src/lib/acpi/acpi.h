#include <stdint.h>

// ACPI_START is 16-byte aligned

#define ACPI_START "RSD PTR "
#define ACPI_START_LEN 16
#define BIOS_RSDP_START 0x000E0000
#define BIOS_RSDP_END 0x000FFFFF
#define E_BIOS_RSDP_LEN 0x00000400

#define RSDP_V1_LEN 20
#define RSDP_V2_LEN 36

#define EBDA_SEG_PTR 0x40E

int initACPI();