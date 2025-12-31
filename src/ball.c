/**************************************************************
 *
 *                          ball.c
 *
 *     Author:  AJ Romeo
 *
 *     Bouncing ball animation with optimized circle rendering
 *     using pre-computed geometry and DMA-accelerated drawing.
 *
 **************************************************************/

#include "ball.h"
#include "../lib/src/ST7789/hardware.h"
#include "../lib/src/graphics/util.h"
#include <stdint.h>
#include <stdbool.h>

#define BORDER 1
#define MAX_R 32

static uint8_t halfw[MAX_R + 1];
static int cached_r = -1;

static inline uint16_t swap565(uint16_t c);
static void precompute_circle(int r);
static uint16_t next_corner_color(uint16_t cur);
static void draw_circle_spans(int cx, int cy, int r, uint16_t color);
static void draw_border(uint16_t border565);

/********** swap565 ********
 *
 * Swap byte order of RGB565 color for display transfer
 *
 * Parameters:
 *      uint16_t c: color in RGB565 format
 *
 * Return: byte-swapped RGB565 color
 *
 * Expects:
 *      none
 *
 * Notes:
 *      Display hardware expects swapped byte order
 ************************/
static inline uint16_t swap565(uint16_t c)
{
        return (uint16_t)((c << 8) | (c >> 8));
}

/********** precompute_circle ********
 *
 * Pre-compute half-width values for each vertical offset of circle
 *
 * Parameters:
 *      int r: radius of circle in pixels
 *
 * Return: none
 *
 * Expects:
 *      r <= MAX_R
 ************************/
static void precompute_circle(int r)
{
        if (r == cached_r) {
                return;
        }
        cached_r = r;

        int rr = r * r;
        for (int dy = 0; dy <= r; dy++) {
                int x = r;
                while (x > 0 && (x * x + dy * dy) > rr) {
                        x--;
                }
                halfw[dy] = (uint8_t)x;
        }
}

/********** next_corner_color ********
 *
 * Cycle to next color in predefined palette
 *
 * Parameters:
 *      uint16_t cur: current color in RGB565 format
 *
 * Return: next color in sequence (RGB565)
 *
 * Expects:
 *      none
 *
 * Notes:
 *      Returns first color if current not found in palette
 ************************/
static uint16_t next_corner_color(uint16_t cur)
{
        static const uint16_t colors[] = {
                0xF800, /* red */
                0x07E0, /* green */
                0x001F, /* blue */
                0xFFE0, /* yellow */
                0xF81F, /* magenta */
                0x07FF, /* cyan */
                0xFFFF, /* white */
        };
        const int n = (int)(sizeof(colors) / sizeof(colors[0]));

        for (int i = 0; i < n; i++) {
                if (colors[i] == cur) {
                        return colors[(i + 1) % n];
                }
        }
        return colors[0];
}

/********** draw_circle_spans ********
 *
 * Draw filled circle using horizontal spans with DMA
 *
 * Parameters:
 *      int cx, cy:              center coordinates
 *      int r:                   radius in pixels
 *      uint16_t color:          fill color (RGB565)
 *
 * Return: none
 *
 * Expects:
 *      r <= MAX_R
 *      precompute_circle(r) has been called
 *
 * Notes:
 *      Clips to screen bounds
 ************************/
static void draw_circle_spans(int cx, int cy, int r, uint16_t color)
{
        static uint16_t spanbuf[2 * MAX_R + 1];
        uint16_t pix = swap565(color);

        for (int i = 0; i < 2 * r + 1; i++) {
                spanbuf[i] = pix;
        }

        for (int dy = -r; dy <= r; dy++) {
                int ay = dy < 0 ? -dy : dy;
                int dx = halfw[ay];

                int y = cy + dy;
                int x0 = cx - dx;
                int x1 = cx + dx;

                if (y < 0 || y >= SCREEN_HEIGHT) {
                        continue;
                }
                if (x0 < 0) {
                        x0 = 0;
                }
                if (x1 >= SCREEN_WIDTH) {
                        x1 = SCREEN_WIDTH - 1;
                }

                int len = x1 - x0 + 1;
                if (len <= 0) {
                        continue;
                }

                set_address_window((uint16_t)x0, (uint16_t)y,
                                   (uint16_t)x1, (uint16_t)y);
                start_display_transfer(spanbuf, (size_t)len);
        }
}

/********** draw_border ********
 *
 * Draw 1-pixel border around screen perimeter
 *
 * Parameters:
 *      uint16_t border565: border color (RGB565)
 *
 * Return: none
 *
 * Expects:
 *      none
 ************************/
static void draw_border(uint16_t border565)
{
        uint16_t pix = swap565(border565);
        static uint16_t rowbuf[SCREEN_WIDTH];
        static uint16_t colbuf[SCREEN_HEIGHT];

        for (int x = 0; x < SCREEN_WIDTH; x++) {
                rowbuf[x] = pix;
        }

        set_address_window(0, 0, SCREEN_WIDTH - 1, 0);
        start_display_transfer(rowbuf, SCREEN_WIDTH);

        set_address_window(0, SCREEN_HEIGHT - 1,
                           SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
        start_display_transfer(rowbuf, SCREEN_WIDTH);

        for (int y = 0; y < SCREEN_HEIGHT; y++) {
                colbuf[y] = pix;
        }

        set_address_window(0, 0, 0, SCREEN_HEIGHT - 1);
        start_display_transfer(colbuf, SCREEN_HEIGHT);

        set_address_window(SCREEN_WIDTH - 1, 0,
                           SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
        start_display_transfer(colbuf, SCREEN_HEIGHT);
}

/********** bouncer_init ********
 *
 * Initialize bouncing ball animation state
 *
 * Parameters:
 *      Bouncer *b:             pointer to Bouncer structure
 *      int radius:             ball radius (clamped to MAX_R)
 *      int vx, vy:             initial velocity components
 *      uint16_t bg_color:      background color (RGB565)
 *      uint16_t border_color:  border color (RGB565)
 *      uint16_t initial_color: starting ball color (RGB565)
 *
 * Return: none
 *
 * Expects:
 *      b is not NULL
 *
 * Notes:
 *      Clears screen, draws border, positions ball at center
 *      Pre-computes circle geometry for efficient rendering
 ************************/
void bouncer_init(Bouncer *b, int radius, int vx, int vy, uint16_t bg_color, 
                  uint16_t border_color, uint16_t initial_color)
{
        if (radius > MAX_R) {
                radius = MAX_R;
        }

        b->r = radius;
        b->vx = vx;
        b->vy = vy;
        b->bg = bg_color;
        b->border = border_color;
        b->color = initial_color;

        precompute_circle(radius);

        b->cx = SCREEN_WIDTH / 2;
        b->cy = SCREEN_HEIGHT / 2;

        fill_screen(bg_color);
        draw_border(border_color);
        draw_circle_spans(b->cx, b->cy, b->r, b->color);
}

/********** bouncer_tick ********
 *
 * Update ball position and appearance for one frame
 *
 * Parameters:
 *      Bouncer *b: pointer to initialized Bouncer structure
 *
 * Return: none
 *
 * Expects:
 *      b is not NULL
 *      bouncer_init has been called on b
 *
 * Notes:
 *      Handles collision detection and velocity reversal
 *      Changes color on corner impacts
 *      Erases old position and draws new position
 ************************/
void bouncer_tick(Bouncer *b)
{
        const int min_x = BORDER + b->r;
        const int max_x = (SCREEN_WIDTH - 1 - BORDER) - b->r;
        const int min_y = BORDER + b->r;
        const int max_y = (SCREEN_HEIGHT - 1 - BORDER) - b->r;

        int oldx = b->cx;
        int oldy = b->cy;

        b->cx += b->vx;
        b->cy += b->vy;

        bool hit_v = false;
        bool hit_h = false;

        if (b->cx <= min_x) {
                b->cx = min_x;
                b->vx = -b->vx;
                hit_v = true;
        }
        if (b->cx >= max_x) {
                b->cx = max_x;
                b->vx = -b->vx;
                hit_v = true;
        }
        if (b->cy <= min_y) {
                b->cy = min_y;
                b->vy = -b->vy;
                hit_h = true;
        }
        if (b->cy >= max_y) {
                b->cy = max_y;
                b->vy = -b->vy;
                hit_h = true;
        }

        if (hit_v && hit_h) {
                b->color = next_corner_color(b->color);
        }

        draw_circle_spans(oldx, oldy, b->r, b->bg);
        draw_circle_spans(b->cx, b->cy, b->r, b->color);
}
