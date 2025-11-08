#include "../include/emulator.h"
#include "../include/cartridge.h"
#include "../include/cpu.h"
#include "../include/ui.h"
#include "../include/timer.h"
#include "../include/dma.h"
#include "../include/ppu.h"
#include <stdio.h>
#include <omp.h>
#include <unistd.h>

/* 
  Emu components:

  |Cart|
  |CPU|
  |Address Bus|
  |PPU|
  |Timer|

*/

static emu_context ctx;

emu_context *emu_get_context() {
    return &ctx;
}

void cpu_run() {
    timer_init();
    cpu_init();
    ppu_init();

    ctx.running = true;
    ctx.paused = false;
    ctx.ticks = 0;

    while(ctx.running) {
        if (ctx.paused) {
            delay(10);
            continue;
        }

        if (!cpu_step()) {
            printf("CPU Stopped\n");
            #pragma omp critical
            {
                ctx.running = false;
            }
            break;
        }
    }
}

void ui_run() {
    u32 prev_frame = 0;
    ui_init();

    while (!ctx.die) {
        delay(1);
        ui_handle_events();

        if (prev_frame != ppu_get_context()->current_frame) {
            ui_update();
        }

        prev_frame = ppu_get_context()->current_frame;
    }

    #pragma omp critical
    {
        ctx.running = false;
    }
}

int emu_run(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: emu <rom_file>\n");
        return -1;
    }

    if (!cart_load(argv[1])) {
        printf("Failed to load ROM file: %s\n", argv[1]);
        return -2;
    }

    printf("Cart loaded..\n");

    #pragma omp parallel sections
    {
        #pragma omp section
        {
            cpu_run();
        }

        #pragma omp section
        {
            ui_run();
        }
    }
    
    return 0;
}

void emu_cycles(int cpu_cycles) {
    
    for (int i=0; i<cpu_cycles; i++) {
        for (int n=0; n<4; n++) {
            ctx.ticks++;
            timer_tick();
            ppu_tick();
        }

        dma_tick();
    }
}
