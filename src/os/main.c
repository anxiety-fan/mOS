#include "hard/idt.h"

#include "video/VGA_text.h"

#include "device/serial.h"
#include "device/ps2.h"

int os_main(){
    makeInterruptTable();
    serialInit();
    int ps2exists = ps2Init();


    clearScreen(black);

    writeText("Welcome To mOS!", (80 - 15)/2, 5, red);

    switch (ps2exists) {
        case 0: 
            println("PS2!!!!", green);
            break;
        case -1:
            println("Controller failed", red);
            if (debugch == 0xFC) println("Normal fail!", red);
            break;
        case -2: 
            println("All possible PS2 ports failed", red);
            break;
    }

    println("It booted!!!", green);

    serialWrite(COM1, (uint8_t*)("Hello, Serial!"), sizeof("Hello, Serial!"));

    VGA_Color colour = light_cyan;
    const char *string = "Hello, World!";
    println(string, colour);
    

    while (1==1)
        ;
        
    return 0;
}
