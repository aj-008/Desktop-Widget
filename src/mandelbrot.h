/**************************************************************
 *
 *                       mandelbrot.h
 *
 *     Author:  AJ Romeo
 *
 *     Interface for Mandelbrot set renderer with progressive
 *     zoom animation using fixed-point arithmetic.
 *
 **************************************************************/

#ifndef MANDELBROT_H
#define MANDELBROT_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
        int32_t cx, cy;
        int32_t scale;
        uint16_t max_iter;
        uint16_t y_next;
} MandelAnim;

void mandelbrot_init(MandelAnim *m);
void mandelbrot_tick(MandelAnim *m, uint16_t lines_per_tick);

#endif
