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

enum page {
    CLOCK,
    QUOTE,
    FILL,
    ANIM
};
static MandelAnim mandel;
static Bouncer ball;

struct widget {
    uint8_t display;
    uint16_t color;
} typedef Widget; 

Widget widget;

void button_init() {
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

bool button_pressed(uint pin) {
    static uint32_t last_time[32];
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (!gpio_get(pin) && now - last_time[pin] > 200) {
        last_time[pin] = now;
        return true;
    }
    return false;
}


void widget_init(uint16_t bg_color, uint16_t text_color) {
    widget.color = text_color;

    fill_screen(bg_color);

    draw_rounded_rec(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 4, text_color);
}





void clock_enter() {
    datetime_t t;

    if (!clock_get_local_datetime(&t)) {
        return;
    } else {
        static const char *days[] = {
            "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
        };

        char date_str[20];  
        snprintf(date_str, sizeof(date_str),
                 "%s %02d/%02d/%04d",
                 days[t.dotw], t.month, t.day, t.year);

        char time_str[9];   
        snprintf(time_str, sizeof(time_str),
                 "%02d:%02d:%02d", t.hour, t.min, t.sec);

        uint16_t red = color565(255, 0, 0);
        uint16_t black = color565(0, 0, 0);
        
        fill_screen(black);
        draw_rounded_rec(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 4, red);
        draw_text_center(135, 32, red, date_str);
        draw_text_center_bg(85, 32, red, black, time_str);
    }
}

void quote_enter() {
    uint16_t red = color565(255, 0, 0);
    uint16_t black = color565(0, 0, 0);
    
    fill_screen(black);
    draw_rounded_rec(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 4, red);

    uint32_t index = get_rand_32() % 250;
    char *quote = quotes[index];

    draw_quote_centered(quotes[index], red);
}



void fill_enter() {
    uint16_t black = color565(0, 0, 0);
    uint16_t red   = color565(255, 0, 0);
    uint16_t cyan  = color565(0, 255, 255);

    // radius 12, velocity 2px/tick
    bouncer_init(&ball, 12, 2, 2, black, red, cyan);
}

void anim_enter() {
    uint16_t red   = color565(255, 0, 0);
    uint16_t black = color565(0, 0, 0);

    fill_screen(black);
    draw_rounded_rec(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 4, red);

    mandelbrot_init(&mandel);
}







void update_clock(void) {
    datetime_t t;

    if (!clock_get_local_datetime(&t)) {
        return;
    } else {
        static const char *days[] = {
            "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
        };

        char date_str[20];  
        snprintf(date_str, sizeof(date_str),
                 "%s %02d/%02d/%04d",
                 days[t.dotw], t.month, t.day, t.year);

        char time_str[9];   
        snprintf(time_str, sizeof(time_str),
                 "%02d:%02d:%02d", t.hour, t.min, t.sec);

        uint16_t red = color565(255, 0, 0);
        uint16_t black = color565(0, 0, 0);
        
        draw_text_center_bg(135, 16, red, black, date_str);
        draw_text_center_bg(85, 32, red, black, time_str);
    }
}


void update_quote() {
}



void update_fill() {
    bouncer_tick(&ball);
}



void update_anim() {
    mandelbrot_tick(&mandel, 32);
}



void widget_run() {
    absolute_time_t last_clock_tick = get_absolute_time();
    absolute_time_t last_anim_tick  = get_absolute_time();

    while (1) {
        usb_time_sync_poll();

        if (button_pressed(BUTTON_A_PIN)) {
            widget.display = CLOCK;
            clock_enter();
        }
        if (button_pressed(BUTTON_B_PIN)) {
            widget.display = QUOTE;
            quote_enter();
        }
        if (button_pressed(BUTTON_X_PIN)) {
            widget.display = FILL;
            fill_enter();
        }
        if (button_pressed(BUTTON_Y_PIN)) {
            widget.display = ANIM;
            anim_enter();
        }

        if (widget.display == CLOCK) {
            if (absolute_time_diff_us(last_clock_tick, get_absolute_time()) > 1000000) {
                last_clock_tick = get_absolute_time();
                update_clock();
            }
        }

        if (widget.display == FILL || widget.display == ANIM) {
            if (absolute_time_diff_us(last_anim_tick, get_absolute_time()) > 16666) {
                last_anim_tick = get_absolute_time();

                if (widget.display == FILL) update_fill();
                else update_anim();
            }
        }
        else if (widget.display == ANIM) {
            update_anim();      
        }
        else if (widget.display == QUOTE) {
        }

        sleep_ms(1);
    }
}



int main() { 
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
}
