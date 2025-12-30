#pragma once
#include <stdint.h>

typedef struct {
    int cx, cy;        // center
    int vx, vy;        // velocity (pixels per tick)
    int r;             // radius
    uint16_t color;    // RGB565 native
    uint16_t bg;       // RGB565 native background
    uint16_t border;   // RGB565 native border
} Bouncer;

void bouncer_init(Bouncer *b, int radius, int vx, int vy,
                  uint16_t bg_color, uint16_t border_color, uint16_t initial_color);

void bouncer_tick(Bouncer *b);

