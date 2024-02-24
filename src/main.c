#include <stdio.h>
#include "pico/stdlib.h"
#include "hub75.h"
#include "img_array.h"

bool repeating_timer_callback(struct repeating_timer *t) {

    static uint16_t row = 0;
    static uint16_t col = 0;

    hub75_draw_pixel(0xFAB6C5, col,row);

    if(col == 63) {
        row = (row + 1) % 64;
    }

    col = (col + 1) % 64;

    return true;
}

int main() {
    stdio_init_all();

    hub75_init(img);

    struct repeating_timer timer;
    // add_repeating_timer_ms(10, repeating_timer_callback, NULL, &timer);

    while(1) {
        tight_loop_contents();
    }
}