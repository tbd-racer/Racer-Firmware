#include "driver/ws2812.h"
#include <hardware/pio.h>

#include "hardware/dma.h"
#include "pico/sync.h"
#include "pico/time.h"
#include "ws2812.pio.h"

#define LED_UPDATE_INTERVAL_MS 50
// Note this should be less than 256 to avoid multiplication overflows and divisible by 2
#define LED_TIMER_PERIOD_TICKS 120
// This should be divisible by 2 and LED_TIMER_PERIOD_TICKS divisible by this
#define LED_FAST_FLASH_PERIOD 6
// This should be divisible by 2 and LED_TIMER_PERIOD_TICKS divisible by this. Note breath uses LED_TIMER_PERIOD
#define LED_SLOW_FLASH_PERIOD 40

static struct status_strip_inst {
    // Configuration
    PIO pio;
    uint sm;
    uint offset;
    uint num_leds;
} local_inst;
static struct status_strip_inst *const inst = &local_inst;

void ws2812_init(PIO pio, uint sm, uint pin, uint num_leds){
    inst->pio = pio;
    inst->sm = sm;
    inst->num_leds = num_leds;

    // Configure PIO
    inst->offset = pio_add_program(pio, &ws2812_program);
    pio_sm_claim(inst->pio, inst->sm);
    ws2812_program_init(inst->pio, inst->sm, inst->offset, pin, 800000, false);
}

void ws2812_strip_set(union ws2812_command commands[]){
    for(uint i =0; i < inst->num_leds; i++){
        pio_sm_put_blocking(inst->pio, inst->sm, commands[i].data);
    }
}
