/**************************************************************
 *
 *                          ball.h
 *
 *     Author:  AJ Romeo
 *
 *     Interface for bouncing ball animation with physics-based
 *     collision detection and color-changing effects.
 *
 **************************************************************/

#ifndef BALL_H
#define BALL_H

#include <stdint.h>

typedef struct {
        int cx, cy;
        int vx, vy;
        int r;
        uint16_t color;
        uint16_t bg;
        uint16_t border;
} Bouncer;

void bouncer_init(Bouncer *b, int radius, int vx, int vy,
                  uint16_t bg_color, uint16_t border_color,
                  uint16_t initial_color);
void bouncer_tick(Bouncer *b);

#endif
