#include "mandelbrot.h"
#include "../lib/src/graphics/util.h"
#include <stdint.h>
#include <stdbool.h>
#include "pico/time.h"

typedef int32_t fx;
#define FX_SHIFT 28
#define FX_ONE   ((fx)1 << FX_SHIFT)
#define FX_FOUR  ((fx)4 << FX_SHIFT)

#define BORDER 1
#define DISP_W (SCREEN_WIDTH  - 2*BORDER)   // 318
#define DISP_H (SCREEN_HEIGHT - 2*BORDER)   // 238
#define SAMPLE_H (DISP_H / 2)               // 119 (1×2 upscale)

#define ZOOM_INTERVAL_MS 10u              
#define ZOOM_FACTOR      0.985              

static inline fx fx_mul(fx a, fx b) { return (fx)((int64_t)a * (int64_t)b >> FX_SHIFT); }
static inline fx fx_add(fx a, fx b) { return a + b; }
static inline fx fx_sub(fx a, fx b) { return a - b; }
static inline fx fx_from_double(double d) { return (fx)(d * (double)(1u << FX_SHIFT)); }

static uint16_t pal[256];
static uint32_t last_zoom_ms = 0;

static void palette_init(void) {
    for (int i = 0; i < 256; i++) {
        uint8_t r = (uint8_t)i;
        uint8_t g = (uint8_t)((i * 5) ^ (i << 1));
        uint8_t b = (uint8_t)(255 - i);
        pal[i] = color565(r, g, b);
    }
    pal[0] = color565(0, 0, 0);
}

static inline void pixel_to_complex(const MandelAnim *m, int x, int y, fx *cr, fx *ci) {
    int32_t dx = x - (SCREEN_WIDTH / 2);
    int32_t dy = y - (SCREEN_HEIGHT / 2);
    *cr = m->cx + (fx)((int64_t)dx * (int64_t)m->scale);
    *ci = m->cy + (fx)((int64_t)dy * (int64_t)m->scale);
}

// Cardioid + period-2 bulb tests (big speedup on interior points)
static inline bool in_cardioid_or_bulb(fx cr, fx ci) {
    fx y2 = fx_mul(ci, ci);

    // period-2 bulb: (x+1)^2 + y^2 <= 1/16
    fx x1 = fx_add(cr, FX_ONE);
    fx x1_2 = fx_mul(x1, x1);
    fx one_over_16 = (fx)(FX_ONE >> 4);
    if (fx_add(x1_2, y2) <= one_over_16) return true;

    // main cardioid:
    // q = (x - 1/4)^2 + y^2
    // q*(q + (x - 1/4)) <= y^2/4
    fx quarter = (fx)(FX_ONE >> 2);
    fx xm = fx_sub(cr, quarter);
    fx q = fx_add(fx_mul(xm, xm), y2);

    fx left = fx_mul(q, fx_add(q, xm));
    fx right = (fx)(y2 >> 2);
    return left <= right;
}

static inline uint16_t mandel_color_native(const MandelAnim *m, int x, int y) {
    fx cr, ci;
    pixel_to_complex(m, x, y, &cr, &ci);

    if (in_cardioid_or_bulb(cr, ci)) {
        return color565(0, 0, 0);
    }

    fx zr = 0, zi = 0;
    uint16_t it = 0;

    while (it < m->max_iter) {
        fx zr2 = fx_mul(zr, zr);
        fx zi2 = fx_mul(zi, zi);
        if (fx_add(zr2, zi2) > FX_FOUR) break;

        fx two_zr_zi = (fx_mul(zr, zi) << 1);
        zr = fx_add(fx_sub(zr2, zi2), cr);
        zi = fx_add(two_zr_zi, ci);
        it++;
    }

    if (it == m->max_iter) return color565(0, 0, 0);
    uint8_t idx = (uint8_t)((it * 255u) / (uint32_t)m->max_iter);
    return pal[idx];
}

static void render_scanline_fullwidth_swapped(const MandelAnim *m, int y, uint16_t *out_swapped) {
    for (int x = 1; x <= SCREEN_WIDTH - 2; x++) {
        uint16_t c = mandel_color_native(m, x, y);
        out_swapped[x - 1] = (uint16_t)((c << 8) | (c >> 8));
    }
}

static void do_zoom_step(MandelAnim *m) {
    const fx zoom = fx_from_double(ZOOM_FACTOR);
    m->scale = fx_mul(m->scale, zoom);

    if (m->max_iter < 140) m->max_iter++;
}

void mandelbrot_init(MandelAnim *m) {
    palette_init();

    m->cx = fx_from_double(-0.743643887037151);
    m->cy = fx_from_double( 0.131825904205330);
    m->scale = fx_from_double(0.010);

    m->max_iter = 64;   // start fast
    m->y_next = 0;      // sample-row index: 0..SAMPLE_H-1

    last_zoom_ms = to_ms_since_boot(get_absolute_time());
}

void mandelbrot_tick(MandelAnim *m, uint16_t lines_per_tick) {
    static uint16_t line_swapped[DISP_W];

    if (lines_per_tick == 0) lines_per_tick = 1;

    for (uint16_t i = 0; i < lines_per_tick; i++) {
        int sy = (int)m->y_next;       // 0..118
        int y0 = BORDER + sy * 2;      // 1..237 step 2
        int y1 = y0 + 1;               // duplicate for 1×2 upscale

        render_scanline_fullwidth_swapped(m, y0, line_swapped);

        push_scanline_swapped_xy(BORDER, (uint16_t)y0, line_swapped, DISP_W);
        push_scanline_swapped_xy(BORDER, (uint16_t)y1, line_swapped, DISP_W);

        m->y_next++;

        if (m->y_next >= SAMPLE_H) {
            m->y_next = 0;

            uint32_t now_ms = to_ms_since_boot(get_absolute_time());
            if ((now_ms - last_zoom_ms) >= ZOOM_INTERVAL_MS) {
                last_zoom_ms = now_ms;
                do_zoom_step(m);
            }
        }
    }
}

