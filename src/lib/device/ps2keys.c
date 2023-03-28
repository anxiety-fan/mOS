#include "ps2keys.h"
#include "../../os/hard/port_io.h"
#include "../../os/hard/idt.h"

void kbdHandler(isr_registers_t* regs) {
    printf("Keyboard interrupt recieved :)");
}

void ps2Init() {
    irqSetHandler(1, kbdHandler);
}
