#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define PS2_DATA 0x60
#define PS2_STAT_CMD 0x64


enum DeviceType {
    StandardMouse = 0x00,
    ScrollMouse = 0x03,
    QuintMouse = 0x04,

    //after 0xAB
    MF2Key1 = 0x83,
    MF2Key2 = 0xC1,
    IBMShortKey = 0x84,
    NCD97Or122 = 0x85,
    Standard122 = 0x86,
    JPNGKey = 0x90,
    JPNPKey,
    JPNAKey,

    //after 0xAC
    SunKey = 0xA1,
};

struct PS2Device {
    enum DeviceType type;

};

int ps2Init();
