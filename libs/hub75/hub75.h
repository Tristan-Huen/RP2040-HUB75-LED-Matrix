#pragma once

#include <stdio.h>
#include "pico/stdlib.h"

#define ROWS 64
#define COLS 64
#define BITS_PER_CHAN 8
#define ARR_LEN (ROWS/2 * COLS * BITS_PER_CHAN)

#define DATA_BASE_PIN 0 
#define NUM_DATA_PINS 11

/**
* @brief Initializes the LED matrix
*
* @param img_ptr Pointer to the image array.
*/
void hub75_init(uint16_t *img_ptr);

/**
* @brief Draw a pixel on the screen.
*
* @param img_ptr Pointer to the image array.
* @param rgb A 24-bit color value.
* @param x The x-coordinate/column. 
* @param y The y-coordinate/row.
* @return True if successful, false otherwise.
*/
bool hub75_draw_pixel(uint32_t rgb, uint32_t x, uint32_t y);

/**
* @brief Clears all pixels on the screen.
*
*/
void hub75_clear_screen();


