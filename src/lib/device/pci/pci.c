#include <stdbool.h>

static bool PCI_compatable;
static bool PCI_mode2;
static bool PCI_memorymapped;

int initPCI(void) {
    int instr = 0xb1a1;
    __asm__ volatile("clc" "movl $ax, %0" "int 0x1A" : "=r" (instr)); //check what config of PCI is supported (1 or 2)
    __asm__ volatile("jc .Error" "movl %0, $ah" : : "r" (instr));
    if (instr == 0x86 || instr == 0x80) {
        PCI_compatable = false;
        return -1;
    }
    PCI_compatable = true;
    PCI_mode2 = (bool)(instr - 1);

    if (!PCI_mode2) {
        //waiting for ACPI: if mode 1, non memory mapped operation
    }
}