#include "ball.h"
#include "../lib/src/ST7789/hardware.h"
#include "../lib/src/graphics/util.h"
#include <stdint.h>
#include <stdbool.h>

#define BORDER 1
#define MAX_R 32  // keep sane; adjust if you want bigger balls

static inline uint16_t swap565(uint16_t c) {
    return (uint16_t)((c << 8) | (c >> 8));
}

static uint8_t halfw[MAX_R + 1];
static int cached_r = -1;

// Precompute half-width for each dy: halfw[|dy|] = floor(sqrt(r^2 - dy^2))
static void precompute_circle(int r) {
    if (r == cached_r) return;
    cached_r = r;

    int rr = r * r;
    for (int dy = 0; dy <= r; dy++) {
        int x = r;
        while (x > 0 && (x * x + dy * dy) > rr) x--;
        halfw[dy] = (uint8_t)x;
    }
}

static uint16_t next_corner_color(uint16_t cur) {
    static const uint16_t colors[] = {
        0xF800, // red
        0x07E0, // green
        0x001F, // blue
        0xFFE0, // yellow
        0xF81F, // magenta
        0x07FF, // cyan
        0xFFFF, // white
    };
    const int n = (int)(sizeof(colors) / sizeof(colors[0]));
    for (int i = 0; i < n; i++) {
        if (colors[i] == cur) return colors[(i + 1) % n];
    }
    return colors[0];
}

// Draw a filled circle via horizontal spans. Color is native RGB565.
// Uses DMA with a small swapped buffer per span length.
static void draw_circle_spans(int cx, int cy, int r, uint16_t color565_native) {
    static uint16_t spanbuf[2 * MAX_R + 1];

    uint16_t pix = swap565(color565_native);

    for (int i = 0; i < 2 * r + 1; i++) spanbuf[i] = pix;

    for (int dy = -r; dy <= r; dy++) {
        int ay = dy < 0 ? -dy : dy;
        int dx = halfw[ay];

        int y = cy + dy;
        int x0 = cx - dx;
        int x1 = cx + dx;

        // Clamp to screen just in case
        if (y < 0 || y >= SCREEN_HEIGHT) continue;
        if (x0 < 0) x0 = 0;
        if (x1 >= SCREEN_WIDTH) x1 = SCREEN_WIDTH - 1;
        int len = x1 - x0 + 1;
        if (len <= 0) continue;

        set_address_window((uint16_t)x0, (uint16_t)y, (uint16_t)x1, (uint16_t)y);
        start_display_transfer(spanbuf, (size_t)len);
    }
}

// Draw the 1px border once (DMA full-screen is overkill; just draw lines to framebuffer and transfer once,
// or do direct spans like below).
static void draw_border(uint16_t border565) {
    uint16_t pix = swap565(border565);
    static uint16_t rowbuf[SCREEN_WIDTH];

    for (int x = 0; x < SCREEN_WIDTH; x++) rowbuf[x] = pix;

    // top
    set_address_window(0, 0, SCREEN_WIDTH - 1, 0);
    start_display_transfer(rowbuf, SCREEN_WIDTH);
    // bottom
    set_address_window(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
    start_display_transfer(rowbuf, SCREEN_WIDTH);

    // left/right columns: reuse a 1-pixel-wide buffer
    static uint16_t colbuf[SCREEN_HEIGHT];
    for (int y = 0; y < SCREEN_HEIGHT; y++) colbuf[y] = pix;

    set_address_window(0, 0, 0, SCREEN_HEIGHT - 1);
    start_display_transfer(colbuf, SCREEN_HEIGHT);

    set_address_window(SCREEN_WIDTH - 1, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
    start_display_transfer(colbuf, SCREEN_HEIGHT);
}

void bouncer_init(Bouncer *b, int radius, int vx, int vy,
                  uint16_t bg_color, uint16_t border_color, uint16_t initial_color) {
    if (radius > MAX_R) radius = MAX_R;

    b->r = radius;
    b->vx = vx;
    b->vy = vy;
    b->bg = bg_color;
    b->border = border_color;
    b->color = initial_color;

    precompute_circle(radius);

    // Start centered
    b->cx = SCREEN_WIDTH / 2;
    b->cy = SCREEN_HEIGHT / 2;

    // Clear background once (fast full-screen fill you already have)
    fill_screen(bg_color);
    draw_border(border_color);

    // Draw initial ball
    draw_circle_spans(b->cx, b->cy, b->r, b->color);
}

void bouncer_tick(Bouncer *b) {
    // Interior limits for center (respect 1px border)
    const int min_x = BORDER + b->r;
    const int max_x = (SCREEN_WIDTH - 1 - BORDER) - b->r;
    const int min_y = BORDER + b->r;
    const int max_y = (SCREEN_HEIGHT - 1 - BORDER) - b->r;

    int oldx = b->cx, oldy = b->cy;

    // Move
    b->cx += b->vx;
    b->cy += b->vy;

    bool hit_v = false, hit_h = false;

    if (b->cx <= min_x) { b->cx = min_x; b->vx = -b->vx; hit_v = true; }
    if (b->cx >= max_x) { b->cx = max_x; b->vx = -b->vx; hit_v = true; }
    if (b->cy <= min_y) { b->cy = min_y; b->vy = -b->vy; hit_h = true; }
    if (b->cy >= max_y) { b->cy = max_y; b->vy = -b->vy; hit_h = true; }

    // Corner hit => change color
    if (hit_v && hit_h) {
        b->color = next_corner_color(b->color);
    }

    // Erase old ball by drawing it in bg
    draw_circle_spans(oldx, oldy, b->r, b->bg);

    // Redraw border only if you want absolute safety.
    // Not needed because we clamp ball away from border.
    // draw_border(b->border);

    // Draw new ball
    draw_circle_spans(b->cx, b->cy, b->r, b->color);
}

