#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int32_t cx, cy;     // Q4.28
    int32_t scale;      // Q4.28 units per pixel
    uint16_t max_iter;
    uint16_t y_next;    // scanline in [1, SCREEN_HEIGHT-2]
} MandelAnim;

void mandelbrot_init(MandelAnim *m);
void mandelbrot_tick(MandelAnim *m, uint16_t lines_per_tick);
