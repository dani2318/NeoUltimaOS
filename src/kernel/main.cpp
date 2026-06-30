#include <bootdefs.h>
#include "ScreenTextDevice.hpp"
#include "font.hpp"

extern "C" __attribute__((section(".text.start"))) void start(BootInfo* bootArgs){
    ScreenTextDevice screen((Framebuffer*)&bootArgs->framebuffer, (PsF1Header*)bootArgs->font_address);

    screen.clear();
    screen.print("K: Handoff compleated!\r\n", true);
    screen.printf("%s","Welcome to Neo-UltimaOS\r\n");

    while(1){ __asm__ volatile("hlt");}
}