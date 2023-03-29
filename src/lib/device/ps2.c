#include "ps2.h"
#include "../../os/hard/port_io.h"
#include "../../os/hard/idt.h"

#include "container/ring_buffer.h"

typedef ring_buffer(64) ps2_buffer_t;

ps2_buffer_t PS2Port1;
ps2_buffer_t PS2Port2;

static bool secondDeviceEnabled = true;

static bool port1active = false;
static bool port2active = false;

void ps2HandlerPort1(isr_registers_t* regs) {
    char b = inb(PS2_DATA);
    ring_buffer_push(&PS2Port1, b);
}
void ps2HandlerPort2(isr_registers_t* regs) {
    char b = inb(PS2_DATA);
    ring_buffer_push(&PS2Port2, b);
}

int writeController(char b) {
    for (int i = 0; inb(PS2_STAT_CMD) & 0b0010; i++) { //check status of ps2 write buffer
        if (i >= 65535) return -1; //if past timeout, error
        outb(PS2_STAT_CMD, b); //status and command are the same port (don't ask me why)
    }
    return 0;
}

int writePort(char b) {
    for (int i = 0; inb(PS2_STAT_CMD) & 0x01; i++) { //check status of ps2 write buffer
        if (i >= 65535) return -1; //if past timeout, error
    }
    outb(PS2_DATA, b);
    return 0;
}


bool writePortWithACK(char b) {
    if (0 > writePort(b)) return false;
    for (int i = 0; ring_buffer_empty(&PS2Port1); i++) {
        if (i >= 65535) return -1;
    }
    if (ring_buffer_pop(&PS2Port1) != 0xFA) return false;
    return true;
}

bool writePort2WithACK(char b) {
    if (0 > writePort2(b)) return false;
    for (int i = 0; ring_buffer_empty(&PS2Port2); i++) {
        if (i >= 65535) return -1;
    }
    if (ring_buffer_pop(&PS2Port2) != 0xFA) return false;
    return true;
}

int writePort2(char b) {
    if (!port2active) return -1;
    if (0 > writeController(0xD4)) return -1;
    return writePort(b);
}


char readConfig() {
    if (0 > writeController(0x20)) return 0x0;
    return inb(PS2_STAT_CMD);
}

int ps2Init() { 
    irqSetHandler(1, ps2HandlerPort1);
    irqSetHandler(12, ps2HandlerPort2);

    disableInterrupts();
    //assuming a PS2 controller exists so I don't have to switch hardware modes just to check
    //also assuming that USB doesn't exist (should disable USB Legacy as soon as OS inits)
        //(USB support is probably a stretch goal)
    writeController(0xAD); //disable 1st device
    writeController(0xA7); //disable 2nd (if it exists)
    while (inb(PS2_STAT_CMD) & 0b0001) inb(PS2_DATA); //flush read buffer while not empty
    char stat = readConfig();
    if (!(stat & 0b00100000)) secondDeviceEnabled = false; //check if second device is supported
    stat = stat & 0b10111100; // disable interrupts and translation
    writeController(0x60);
    writeController(stat); //write new status config

    writeController(0xAA);
    if (inb(PS2_STAT_CMD) != 0x55)  {
        enableInterrupts();
        return -1; //test controller failed
    }
    
    writeController(0x60);
    writeController(stat); //write statis config again (some machines reset the controller after testing)

    if (secondDeviceEnabled) {
        writeController(0xA8);
        if (readConfig() & 0b00100000) { //check whether second device can be enabled (i.e. it exists)
            secondDeviceEnabled = false;
        }
        else {
            writeController(0xA7);
        }
    }

    //testing devices themselves
    //erroring at sign of any error (there is probably a better way to do this)
    writeController(0xAB);
    if(inb(PS2_STAT_CMD) != 0x0) {
        enableInterrupts();
        return -1;
    }

    if (secondDeviceEnabled) {
        writeController(0xA9);
        if(inb(PS2_STAT_CMD) != 0x0) {
            enableInterrupts();
            return -1;
        }
    }

    stat = readConfig(); //reenable interrupts for PS2 devices
    writeController(0xAE);
    stat = stat & 0b0001;
    if (secondDeviceEnabled) {
        writeController(0xA8);
        stat = stat & 0b0010;
    }
    writeController(0x60);
    writeController(stat); //write new status config
    enableInterrupts();

    //reseting devices
    port1active = writePortWithACK(0xFF);
    if (secondDeviceEnabled)
        port2active = writePort2WithACK(0xFF);
    return 0;
}
