#include "acpi.h"

#include "../stdlib/string.h"
#include "acpi_tables.h"

#include <stdbool.h>

// slightly modified version of general table checksum in OSDevWiki
int tableChecksum(struct ACPITableHead *tableHeader) {
    uint32_t acc = 0;

    for (int i = 0; i < tableHeader->length; i++) {
        acc += ((char *)tableHeader)[i];
    }

    if (acc & 0x000000FF == 0) {
        return 0;
    } else {
        return -1;
    }
}

// generalized table lookup based on OSDevWiki
void *findTable(void *root_sdt, char *signature, bool isXSDT) {
    if (isXSDT) {
        struct XSDT *xsdt = (struct XSDT *)root_sdt;
        int entries = (xsdt->h.length - sizeof(xsdt->h)) / 8;

        for (int i = 0; i < entries; i++) {
            struct ACPITableHead *h = (struct ACPITableHead *)xsdt->entries[i];
            if (strncmp(h->signature, signature, 4) == 0 &&
                tableChecksum(h) == 0) {
                return (void *)h;
            }
        }
    } else {
        struct RSDT *rsdt = (struct RSDT *)root_sdt;
        int entries = (rsdt->h.length - sizeof(rsdt->h)) / 4;

        for (int i = 0; i < entries; i++) {
            struct ACPITableHead *h = (struct ACPITableHead *)rsdt->entries[i];
            if (strncmp(h->signature, signature, 4) == 0 &&
                tableChecksum(h) == 0) {
                return (void *)h;
            }
        }
    }
    return NULL;
}

int RSDPChecksum();
void *findRSDP() {

    // Finding root system description pointer (RSDP)

    // Searching w/n BIOS
    void *rsdp = NULL;
    for (char *i = BIOS_RSDP_START; i < BIOS_RSDP_END - 0xF;
         i += ACPI_START_LEN) {
        if (strncmp(ACPI_START, i, ACPI_START_LEN) == 0 &&
            RSDPChecksum((uint32_t *)i) == 0) {
            rsdp = i;
            break;
        }
    }

    // Searching w/n 1st KB of Extended BIOS Data Area
    if (!rsdp) {
        uint32_t ebda_seg = (*(uint32_t *)EBDA_SEG_PTR & 0x0000FFFF) * 0x10;
        for (char *i = ebda_seg; i < ebda_seg + E_BIOS_RSDP_LEN;
             i += ACPI_START_LEN) {
            if (strncmp(ACPI_START, i, ACPI_START_LEN) == 0 &&
                RSDPChecksum((uint32_t *)i) == 0) {
                rsdp = i;
                break;
            }
        }
    }

    if (!rsdp)
        return NULL;

    return rsdp;
}

/*
    (Vers. 1)
    char[8] "RSD PTR "
    uint_8 checksum
    char[6] OEM ID
    uint_8 revision
    uint_32 RSDT_addr

    (Vers.2) - only if revision == 2
    uint_32 length
    uint_64 XSDT_addr
    uint_8 extend_checksum
    uint_8[3] reserved
*/
int RSDPChecksum(uint32_t *start) {
    // &revision == start + 15
    uint32_t revision = *(start + 12) >> 24;
    uint32_t acc = 0;
    uint32_t len;

    if (revision == 2) {
        // total 36 bytes
        len = RSDP_V2_LEN;
    } else if (revision == 0) {
        // total 20 bytes
        len = RSDP_V1_LEN;
    } else {
        return -1;
    }

    for (uint32_t *c = start; c < start + len; c += sizeof(uint32_t)) {
        uint32_t block = *c;
        acc += (block & 0xFF000000) >> 24;
        acc += (block & 0x00FF0000) >> 16;
        acc += (block & 0x0000FF00) >> 8;
        acc += block & 0x000000FF;
    }

    if (acc & 0x000000FF == 0) {
        return 0;
    } else {
        return -1;
    }
}

int initACPI() {
    void *rsdp = findRSDP();
    bool isXSRP = false;
    uint32_t table = 0x0;
    uint64_t table_64 = 0x0;

    if (!rsdp) {
        return -1;
    }
    uint32_t revision = *((uint32_t *)rsdp + 12) >> 24;
    if (revision == 2) {
        isXSRP = true;
        table_64 = *(uint64_t *)rsdp + 24;
        if (table_64 >> 32 != 0 ||
            tableChecksum((struct ACPITableHead *)table_64) < 0) {
            return -1;
        }
    } else {
        table = *(uint32_t *)rsdp + 16;
        if (tableChecksum((struct ACPITableHead *)table) < 0) {
            return -1;
        }
    }

    return 0;
}