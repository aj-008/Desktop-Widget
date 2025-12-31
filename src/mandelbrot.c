/**************************************************************
 *
 *                       mandelbrot.c
 *
 *     Mandelbrot set renderer
 *
 **************************************************************/

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
#define DISP_W (SCREEN_WIDTH  - 2*BORDER)
#define DISP_H (SCREEN_HEIGHT - 2*BORDER)
#define SAMPLE_H (DISP_H / 2)

#define ZOOM_INTERVAL_MS 10u
#define ZOOM_FACTOR      0.985

static uint16_t pal[256];
static uint32_t last_zoom_ms = 0;

static inline fx fx_mul(fx a, fx b);
static inline fx fx_add(fx a, fx b);
static inline fx fx_sub(fx a, fx b);
static inline fx fx_from_double(double d);
static void palette_init(void);
static inline void pixel_to_complex(const MandelAnim *m, int x, int y,
                                    fx *cr, fx *ci);
static inline bool in_cardioid_or_bulb(fx cr, fx ci);
static inline uint16_t mandel_color(const MandelAnim *m, int x, int y);
static void render_scanline(const MandelAnim *m, int y,
                             uint16_t *out_swapped);
static void do_zoom_step(MandelAnim *m);

/********** fx_mul ********
 *
 * Multiply two Q4.28 fixed-point numbers
 *
 * Parameters:
 *      fx a, b: fixed-point operands
 *
 * Return: product in Q4.28 format
 *
 * Expects:
 *      none
 *
 * Notes:
 *      Uses 64-bit intermediate to avoid overflow
 ************************/
static inline fx fx_mul(fx a, fx b)
{
        return (fx)((int64_t)a * (int64_t)b >> FX_SHIFT);
}

/********** fx_add ********
 *
 * Add two Q4.28 fixed-point numbers
 *
 * Parameters:
 *      fx a, b: fixed-point operands
 *
 * Return: sum in Q4.28 format
 *
 * Expects:
 *      none
 *
 * Notes:
 *      Simple addition in fixed-point
 ************************/
static inline fx fx_add(fx a, fx b)
{
        return a + b;
}

/********** fx_sub ********
 *
 * Subtract two Q4.28 fixed-point numbers
 *
 * Parameters:
 *      fx a, b: fixed-point operands
 *
 * Return: difference in Q4.28 format
 *
 * Expects:
 *      none
 *
 * Notes:
 *      Simple subtraction in fixed-point
 ************************/
static inline fx fx_sub(fx a, fx b)
{
        return a - b;
}

/********** fx_from_double ********
 *
 * Convert double to Q4.28 fixed-point
 *
 * Parameters:
 *      double d: floating-point value to convert
 *
 * Return: Q4.28 fixed-point representation
 *
 * Expects:
 *      d fits within Q4.28 range
 *
 * Notes:
 *      Used for initialization constants
 ************************/
static inline fx fx_from_double(double d)
{
        return (fx)(d * (double)(1u << FX_SHIFT));
}

/********** palette_init ********
 *
 * Initialize color palette for iteration visualization
 *
 * Parameters:
 *      none
 *
 * Return: none
 *
 * Expects:
 *      Called once at initialization
 *
 * Notes:
 *      Creates gradient based on iteration count
 *      Index 0 reserved for black (points in set)
 ************************/
static void palette_init(void)
{
        for (int i = 0; i < 256; i++) {
                uint8_t r = (uint8_t)i;
                uint8_t g = (uint8_t)((i * 5) ^ (i << 1));
                uint8_t b = (uint8_t)(255 - i);
                pal[i] = color565(r, g, b);
        }
        pal[0] = color565(0, 0, 0);
}

/********** pixel_to_complex ********
 *
 * Convert screen coordinates to complex plane coordinates
 *
 * Parameters:
 *      const MandelAnim *m: animation state
 *      int x, y:            screen coordinates
 *      fx *cr, *ci:         output real and imaginary parts
 *
 * Return: none (outputs via cr, ci)
 *
 * Expects:
 *      cr and ci are not NULL
 *
 * Notes:
 *      Uses current zoom level and center point
 ************************/
static inline void pixel_to_complex(const MandelAnim *m, int x, int y, fx *cr, fx *ci)
{
        int32_t dx = x - (SCREEN_WIDTH / 2);
        int32_t dy = y - (SCREEN_HEIGHT / 2);
        *cr = m->cx + (fx)((int64_t)dx * (int64_t)m->scale);
        *ci = m->cy + (fx)((int64_t)dy * (int64_t)m->scale);
}

/********** in_cardioid_or_bulb ********
 *
 * Test if point is in main cardioid or period-2 bulb
 *
 * Parameters:
 *      fx cr, ci: real and imaginary parts of complex number
 *
 * Return: true if in cardioid or bulb, false otherwise
 *
 * Expects:
 *      none
 *
 * Notes:
 *      Optimization: points in these regions are in the set
 *      Avoids expensive iteration for interior points
 *      Period-2 bulb: (x+1)^2 + y^2 <= 1/16
 *      Cardioid: q*(q + (x-1/4)) <= y^2/4 where q=(x-1/4)^2+y^2
 ************************/
static inline bool in_cardioid_or_bulb(fx cr, fx ci)
{
        fx y2 = fx_mul(ci, ci);

        fx x1 = fx_add(cr, FX_ONE);
        fx x1_2 = fx_mul(x1, x1);
        fx one_over_16 = (fx)(FX_ONE >> 4);
        if (fx_add(x1_2, y2) <= one_over_16) {
                return true;
        }

        fx quarter = (fx)(FX_ONE >> 2);
        fx xm = fx_sub(cr, quarter);
        fx q = fx_add(fx_mul(xm, xm), y2);

        fx left = fx_mul(q, fx_add(q, xm));
        fx right = (fx)(y2 >> 2);
        return left <= right;
}

/********** mandel_color ********
 *
 * Compute color for pixel using Mandelbrot escape-time algorithm
 *
 * Parameters:
 *      const MandelAnim *m: animation state
 *      int x, y:            screen coordinates
 *
 * Return: RGB565 color value
 *
 * Expects:
 *      m is not NULL
 *
 * Notes:
 *      Returns black for points in set (max iterations)
 *      Uses cardioid/bulb test for quick rejection
 *      Iterates z = z^2 + c until |z|^2 > 4 or max_iter
 ************************/
static inline uint16_t mandel_color(const MandelAnim *m, int x, int y)
{
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

                if (fx_add(zr2, zi2) > FX_FOUR) {
                        break;
                }

                fx two_zr_zi = (fx_mul(zr, zi) << 1);
                zr = fx_add(fx_sub(zr2, zi2), cr);
                zi = fx_add(two_zr_zi, ci);
                it++;
        }

        if (it == m->max_iter) {
                return color565(0, 0, 0);
        }
        uint8_t idx = (uint8_t)((it * 255u) / (uint32_t)m->max_iter);
        return pal[idx];
}

/********** render_scanline ********
 *
 * Render complete scanline with byte-swapped pixels
 *
 * Parameters:
 *      const MandelAnim *m: animation state
 *      int y:               screen y-coordinate
 *      uint16_t *out_swapped: output buffer (DISP_W pixels)
 *
 * Return: none
 *
 * Expects:
 *      out_swapped has space for DISP_W pixels
 *
 * Notes:
 *      Skips 1-pixel border on each side
 *      Byte-swaps RGB565 for DMA transfer
 ************************/
static void render_scanline(const MandelAnim *m, int y, uint16_t *out_swapped)
{
        for (int x = 1; x <= SCREEN_WIDTH - 2; x++) {
                uint16_t c = mandel_color(m, x, y);
                out_swapped[x - 1] = (uint16_t)((c << 8) | (c >> 8));
        }
}

/********** do_zoom_step ********
 *
 * Apply one zoom step to animation
 *
 * Parameters:
 *      MandelAnim *m: animation state to update
 *
 * Return: none
 *
 * Expects:
 *      m is not NULL
 *
 * Notes:
 *      Multiplies scale by ZOOM_FACTOR
 *      Increases iteration count for more detail
 *      Caps iterations at 140
 ************************/
static void do_zoom_step(MandelAnim *m)
{
        const fx zoom = fx_from_double(ZOOM_FACTOR);
        m->scale = fx_mul(m->scale, zoom);

        if (m->max_iter < 140) {
                m->max_iter++;
        }
}

/********** mandelbrot_init ********
 *
 * Initialize Mandelbrot animation state
 *
 * Parameters:
 *      MandelAnim *m: animation structure to initialize
 *
 * Return: none
 *
 * Expects:
 *      m is not NULL
 *
 * Notes:
 *      Sets interesting starting location (spiral tendril)
 *      Initializes color palette
 *      Begins at lower iteration count for speed
 ************************/
void mandelbrot_init(MandelAnim *m)
{
        palette_init();

        m->cx = fx_from_double(-0.743643887037151);
        m->cy = fx_from_double( 0.131825904205330);
        m->scale = fx_from_double(0.010);

        m->max_iter = 64;
        m->y_next = 0;

        last_zoom_ms = to_ms_since_boot(get_absolute_time());
}

/********** mandelbrot_tick ********
 *
 * Render batch of scanlines and handle zoom timing
 *
 * Parameters:
 *      MandelAnim *m:          animation state
 *      uint16_t lines_per_tick: sample rows to render (0 = 1)
 *
 * Return: none
 *
 * Expects:
 *      m is not NULL
 *      mandelbrot_init has been called on m
 *
 * Notes:
 *      Uses 1x2 upscaling (each row drawn twice)
 *      Automatically zooms after each complete frame
 *      Progressive rendering maintains interactivity
 ************************/
void mandelbrot_tick(MandelAnim *m, uint16_t lines_per_tick)
{
        static uint16_t line_swapped[DISP_W];

        if (lines_per_tick == 0) {
                lines_per_tick = 1;
        }

        for (uint16_t i = 0; i < lines_per_tick; i++) {
                int sy = (int)m->y_next;
                int y0 = BORDER + sy * 2;
                int y1 = y0 + 1;

                render_scanline(m, y0, line_swapped);

                push_scanline_swapped_xy(BORDER, (uint16_t)y0,
                                         line_swapped, DISP_W);
                push_scanline_swapped_xy(BORDER, (uint16_t)y1,
                                         line_swapped, DISP_W);

                m->y_next++;

                if (m->y_next >= SAMPLE_H) {
                        m->y_next = 0;

                        uint32_t now_ms =
                                to_ms_since_boot(get_absolute_time());
                        if ((now_ms - last_zoom_ms) >=
                            ZOOM_INTERVAL_MS) {
                                last_zoom_ms = now_ms;
                                do_zoom_step(m);
                        }
                }
        }
}
