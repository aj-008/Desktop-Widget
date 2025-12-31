/**************************************************************
 *
 *                          main.c
 *
 *     Author:  AJ Romeo
 *
 *     Main program managing multiple display modes with button
 *     navigation. Coordinates hardware initialization, user
 *     input, and display updates for clock, quote, ball, and
 *     Mandelbrot visualizations.
 *
 **************************************************************/

#include "../lib/src/ST7789/hardware.h"
#include "../lib/src/graphics/util.h"
#include "../lib/src/graphics/text.h"
#include "../lib/src/graphics/shapes.h"
#include "../lib/src/graphics/image.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "mandelbrot.h"
#include "ball.h"
#include "clock.h"
#include "quote.h"

#define BUTTON_A_PIN 12
#define BUTTON_B_PIN 13
#define BUTTON_X_PIN 14
#define BUTTON_Y_PIN 15

#define TZ_OFFSET_HOURS (-5)

#define CLOCK_UPDATE_INTERVAL_US 1000000
#define ANIM_UPDATE_INTERVAL_US  16666

typedef enum {
        PAGE_CLOCK,
        PAGE_QUOTE,
        PAGE_BALL,
        PAGE_MANDELBROT
} DisplayPage;

typedef struct {
        DisplayPage current_page;
        uint16_t text_color;
        uint16_t bg_color;
} Widget;

static Widget widget;
static MandelAnim mandel_state;
static Bouncer ball_state;

static void button_init(void);
static bool button_pressed(uint pin);
static void draw_clock_display(const datetime_t *t, uint16_t txt,
                                uint16_t bg);
static void page_clock_enter(void);
static void page_clock_update(void);
static void page_quote_enter(void);
static void page_ball_enter(void);
static void page_ball_update(void);
static void page_mandelbrot_enter(void);
static void page_mandelbrot_update(void);
static void handle_button_input(void);
static void handle_display_updates(absolute_time_t *last_clock,
                                    absolute_time_t *last_anim);
static void widget_init(uint16_t bg, uint16_t text);
static void widget_run(void);

/********** button_init ********
 *
 * Configure GPIO pins for button inputs
 *
 * Parameters:
 *      none
 *
 * Return: none
 *
 * Expects:
 *      Called once at startup
 *
 * Notes:
 *      Buttons are active-low with pull-up resistors
 ************************/
static void button_init(void)
{
        gpio_init(BUTTON_A_PIN);
        gpio_init(BUTTON_B_PIN);
        gpio_init(BUTTON_X_PIN);
        gpio_init(BUTTON_Y_PIN);

        gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
        gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
        gpio_set_dir(BUTTON_X_PIN, GPIO_IN);
        gpio_set_dir(BUTTON_Y_PIN, GPIO_IN);

        gpio_pull_up(BUTTON_A_PIN);
        gpio_pull_up(BUTTON_B_PIN);
        gpio_pull_up(BUTTON_X_PIN);
        gpio_pull_up(BUTTON_Y_PIN);
}

/********** button_pressed ********
 *
 * Check if button pressed with debouncing
 *
 * Parameters:
 *      uint pin: GPIO pin number to check
 *
 * Return: true if button was pressed, false otherwise
 *
 * Expects:
 *      pin is a valid GPIO pin
 *      button_init has been called
 *
 * Notes:
 *      Uses 200ms debounce interval
 *      Tracks per-pin timestamps for independent debouncing
 ************************/
static bool button_pressed(uint pin)
{
        static uint32_t last_time[32];
        uint32_t now = to_ms_since_boot(get_absolute_time());

        if (gpio_get(pin) == 0 && now - last_time[pin] > 200) {
                last_time[pin] = now;
                return true;
        }
        return false;
}

/********** draw_clock_display ********
 *
 * Render date and time at fixed positions
 *
 * Parameters:
 *      const datetime_t *t: datetime to display
 *      uint16_t txt:        text color (RGB565)
 *      uint16_t bg:         background color (RGB565)
 *
 * Return: none
 *
 * Expects:
 *      t is not NULL
 *
 * Notes:
 *      Date format: "Day MM/DD/YYYY"
 *      Time format: "HH:MM:SS"
 ************************/
static void draw_clock_display(const datetime_t *t, uint16_t txt, uint16_t bg)
{
        static const char *days[] = {
                "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
        };

        char date_str[20];
        snprintf(date_str, sizeof(date_str),
                 "%s %02d/%02d/%04d",
                 days[t->dotw], t->month, t->day, t->year);

        char time_str[9];
        snprintf(time_str, sizeof(time_str),
                 "%02d:%02d:%02d", t->hour, t->min, t->sec);

        draw_text_center_bg(135, 16, txt, bg, date_str);
        draw_text_center_bg(85, 32, txt, bg, time_str);
}

/********** page_clock_enter ********
 *
 * Initialize clock display page
 *
 * Parameters:
 *      none
 *
 * Return: none
 *
 * Expects:
 *      widget colors initialized
 *
 * Notes:
 *      Clears screen and displays initial time if available
 ************************/
static void page_clock_enter(void)
{
        datetime_t t;

        fill_screen(widget.bg_color);
        draw_rounded_rec(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 4,
                         widget.text_color);

        if (clock_get_local_datetime(&t)) {
                draw_clock_display(&t, widget.text_color,
                                   widget.bg_color);
        }
}

/********** page_clock_update ********
 *
 * Update clock display with current time
 *
 * Parameters:
 *      none
 *
 * Return: none
 *
 * Expects:
 *      page_clock_enter has been called
 *
 * Notes:
 *      Called periodically to refresh display
 ************************/
static void page_clock_update(void)
{
        datetime_t t;
        if (clock_get_local_datetime(&t)) {
                draw_clock_display(&t, widget.text_color,
                                   widget.bg_color);
        }
}

/********** page_quote_enter ********
 *
 * Initialize quote display page
 *
 * Parameters:
 *      none
 *
 * Return: none
 *
 * Expects:
 *      widget colors initialized
 *
 * Notes:
 *      Displays randomly selected quote from collection
 ************************/
static void page_quote_enter(void)
{
        fill_screen(widget.bg_color);
        draw_rounded_rec(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 4,
                         widget.text_color);

        uint32_t index = get_rand_32() % QUOTE_COUNT;
        draw_quote_centered(quotes[index], widget.text_color);
}

/********** page_ball_enter ********
 *
 * Initialize bouncing ball animation page
 *
 * Parameters:
 *      none
 *
 * Return: none
 *
 * Expects:
 *      widget colors initialized
 *
 * Notes:
 *      Ball starts at center with radius 12, velocity 2px/tick
 ************************/
static void page_ball_enter(void)
{
        uint16_t cyan = color565(0, 255, 255);

        bouncer_init(&ball_state, 12, 2, 2, widget.bg_color,
                     widget.text_color, cyan);
}

/********** page_ball_update ********
 *
 * Update ball animation for one frame
 *
 * Parameters:
 *      none
 *
 * Return: none
 *
 * Expects:
 *      page_ball_enter has been called
 *
 * Notes:
 *      Called at ANIM_UPDATE_INTERVAL_US (~60 FPS)
 ************************/
static void page_ball_update(void)
{
        bouncer_tick(&ball_state);
}

/********** page_mandelbrot_enter ********
 *
 * Initialize Mandelbrot fractal animation page
 *
 * Parameters:
 *      none
 *
 * Return: none
 *
 * Expects:
 *      widget colors initialized
 *
 * Notes:
 *      Starts at interesting zoom location with progressive
 *      rendering
 ************************/
static void page_mandelbrot_enter(void)
{
        fill_screen(widget.bg_color);
        draw_rounded_rec(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 4,
                         widget.text_color);

        mandelbrot_init(&mandel_state);
}

/********** page_mandelbrot_update ********
 *
 * Update Mandelbrot animation by rendering scanlines
 *
 * Parameters:
 *      none
 *
 * Return: none
 *
 * Expects:
 *      page_mandelbrot_enter has been called
 *
 * Notes:
 *      Renders 32 lines per update for smooth animation
 ************************/
static void page_mandelbrot_update(void)
{
        mandelbrot_tick(&mandel_state, 32);
}

/********** handle_button_input ********
 *
 * Poll buttons and switch pages when pressed
 *
 * Parameters:
 *      none
 *
 * Return: none
 *
 * Expects:
 *      button_init has been called
 *
 * Notes:
 *      A = Clock, B = Quote, X = Ball, Y = Mandelbrot
 ************************/
static void handle_button_input(void)
{
        if (button_pressed(BUTTON_A_PIN)) {
                widget.current_page = PAGE_CLOCK;
                page_clock_enter();
        }
        if (button_pressed(BUTTON_B_PIN)) {
                widget.current_page = PAGE_QUOTE;
                page_quote_enter();
        }
        if (button_pressed(BUTTON_X_PIN)) {
                widget.current_page = PAGE_BALL;
                page_ball_enter();
        }
        if (button_pressed(BUTTON_Y_PIN)) {
                widget.current_page = PAGE_MANDELBROT;
                page_mandelbrot_enter();
        }
}

/********** handle_display_updates ********
 *
 * Manage periodic display updates based on current page
 *
 * Parameters:
 *      absolute_time_t *last_clock: ptr to last clock update time
 *      absolute_time_t *last_anim:  ptr to last anim update time
 *
 * Return: none
 *
 * Expects:
 *      last_clock and last_anim are not NULL
 *
 * Notes:
 *      Clock updates every 1 second
 *      Animations update at ~60 FPS
 ************************/
static void handle_display_updates(absolute_time_t *last_clock,
                                   absolute_time_t *last_anim)
{
        absolute_time_t now = get_absolute_time();

        if (widget.current_page == PAGE_CLOCK) {
                if (absolute_time_diff_us(*last_clock, now) >
                    CLOCK_UPDATE_INTERVAL_US) {
                        *last_clock = now;
                        page_clock_update();
                }
        }

        if (widget.current_page == PAGE_BALL ||
            widget.current_page == PAGE_MANDELBROT) {
                if (absolute_time_diff_us(*last_anim, now) >
                    ANIM_UPDATE_INTERVAL_US) {
                        *last_anim = now;

                        if (widget.current_page == PAGE_BALL) {
                                page_ball_update();
                        } else {
                                page_mandelbrot_update();
                        }
                }
        }
}

/********** widget_init ********
 *
 * Initialize widget system with colors
 *
 * Parameters:
 *      uint16_t bg:   background color (RGB565)
 *      uint16_t text: text/border color (RGB565)
 *
 * Return: none
 *
 * Expects:
 *      Display hardware initialized
 *
 * Notes:
 *      Sets default page to clock
 ************************/
static void widget_init(uint16_t bg, uint16_t text)
{
        widget.bg_color = bg;
        widget.text_color = text;
        widget.current_page = PAGE_CLOCK;

        fill_screen(bg);
        draw_rounded_rec(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 4, text);
}

/********** widget_run ********
 *
 * Main event loop for widget system
 *
 * Parameters:
 *      none
 *
 * Return: none
 *
 * Expects:
 *      All systems initialized
 *
 * Notes:
 *      Polls USB time sync, handles input, updates display
 *      Runs indefinitely until system reset
 ************************/
static void widget_run(void)
{
        absolute_time_t last_clock_update = get_absolute_time();
        absolute_time_t last_anim_update = get_absolute_time();

        while (1) {
                usb_time_sync_poll();
                handle_button_input();
                handle_display_updates(&last_clock_update,
                                       &last_anim_update);
                sleep_ms(1);
        }
}

/********** main ********
 *
 * Program entry point
 *
 * Parameters:
 *      none
 *
 * Return: 0 (never reached)
 *
 * Expects:
 *      none
 *
 * Notes:
 *      Initializes all hardware and starts event loop
 ************************/
int main(void)
{
        stdio_init_all();
        display_spi_init();
        display_dma_init();
        gpio_pin_init();
        st7789_init();

        button_init();
        clock_init();

        uint16_t black = color565(0, 0, 0);
        uint16_t red = color565(255, 0, 0);

        widget_init(black, red);
        widget_run();

        return 0;
}
