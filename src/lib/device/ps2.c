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

static struct PS2Device dev1;
static struct PS2Device dev2;


void ps2HandlerPort1(isr_registers_t* regs) {
    uint8_t b = inb(PS2_DATA);
    ring_buffer_push(&PS2Port1, b);
}
void ps2HandlerPort2(isr_registers_t* regs) {
    uint8_t b = inb(PS2_DATA);
    ring_buffer_push(&PS2Port2, b);
}

int writeController(uint8_t b) {
    for (int i = 0; inb(PS2_STAT_CMD) & 0b0010; i++) { //check status of ps2 write buffer
        if (i >= 65535) return -1; //if past timeout, error
        outb(PS2_STAT_CMD, b); //status and command are the same port (don't ask me why)
    }
    return 0;
}

int writePort(uint8_t b) {
    for (int i = 0; inb(PS2_STAT_CMD) & 0x01; i++) { //check status of ps2 write buffer
        if (i >= 65535) return -1; //if past timeout, error
    }
    outb(PS2_DATA, b);
    return 0;
}

bool writePortWithACK(uint8_t b) {
    if (0 > writePort(b)) return false;
    for (int i = 0; ring_buffer_empty(&PS2Port1); i++) {
        if (i >= 65535) return false;
    }
    if (ring_buffer_pop(&PS2Port1) != 0xFA) return writePortWithACK(b);
    return true;
}

int writePort2(uint8_t b) {
    if (!port2active) return -1;
    if (0 > writeController(0xD4)) return -1;
    return writePort(b);
}

bool writePort2WithACK(uint8_t b) {
    if (0 > writePort2(b)) return false;
    for (int i = 0; ring_buffer_empty(&PS2Port2); i++) {
        if (i >= 65535) return false;
    }
    if (ring_buffer_pop(&PS2Port2) != 0xFA) return writePort2WithACK(b);
    return true;
}

char readConfig() {
    if (0 > writeController(0x20)) return 0x0;
    return inb(PS2_STAT_CMD);
}

enum DeviceType translateDeviceType(uint8_t b) {
    switch (b)
    {
    case StandardMouse:
        return StandardMouse;
    case ScrollMouse:
        return ScrollMouse;
    case QuintMouse:
        return QuintMouse;
    case MF2Key1:
        return MF2Key1;
    case MF2Key2:
        return MF2Key2;
    case IBMShortKey:
        return IBMShortKey;
    case NCD97Or122:
        return NCD97Or122;
    case Standard122:
        return Standard122;
    case JPNGKey:
        return JPNGKey;
    case JPNPKey:
        return JPNPKey;
    case JPNAKey:
        return JPNAKey;
    case SunKey:
        return SunKey;
    default:
        return Unknown;
        break;
    }
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
    uint8_t stat = readConfig();
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

    //determining device types
    if (port1active) {
        writePortWithACK(0xF5);
        writePortWithACK(0xF2);
        // Get 1st byte of type
        for (int i = 0; ring_buffer_empty(&PS2Port1); i++) {
            if (i >= 65535) return false;
        }
        uint8_t p1_b1 = ring_buffer_pop(&PS2Port1);
        if (p1_b1 == 0xAB || p1_b1 == 0xAC) { 
            dev1.isKeyboard = true;
            //get 2nd byte 
            for (int i = 0; ring_buffer_empty(&PS2Port1); i++) {
                if (i >= 65535) return false;
            }
            uint8_t p1_b2 = ring_buffer_pop(&PS2Port1);
            dev1.type = translateDeviceType(p1_b2);
            dev1.scancode = SC2;
        }
        else { 
            dev1.isKeyboard = false;
            dev1.type = translateDeviceType(p1_b1); 
            dev1.scancode = None;
        }
        writePortWithACK(0xF4);
    }

    if (port2active) {
        writePort2WithACK(0xF5);
        writePort2WithACK(0xF2);
        // Get 1st byte of type
        for (int i = 0; ring_buffer_empty(&PS2Port2); i++) {
            if (i >= 65535) return false;
        }
        uint8_t p1_b1 = ring_buffer_pop(&PS2Port2);
        if (p1_b1 == 0xAB || p1_b1 == 0xAC) { 
            dev2.isKeyboard = 1;
            //get 2nd byte 
            for (int i = 0; ring_buffer_empty(&PS2Port2); i++) {
                if (i >= 65535) return false;
            }
            uint8_t p1_b2 = ring_buffer_pop(&PS2Port2);
            dev2.type = translateDeviceType(p1_b2);
            dev2.scancode = SC2;
        }
        else { 
            dev2.isKeyboard = false;
            dev2.type = translateDeviceType(p1_b1); 
            dev2.scancode = None;
        }
        writePort2WithACK(0xF4);
    }
    return 0;
}

