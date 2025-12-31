# Desktop Widget

A modular desktop widget system for Raspberry Pi Pico W with ST7789 display, featuring multiple interactive displays including clock, quotes, animations, and visualizations.

## Features

- **Real-time Clock**: Displays current date and time with USB time synchronization
- **Quote Display**: Randomly selected motivational quotes from a curated collection
- **Bouncing Ball Animation**: ball animation with color-changing on corner collisions
- **Mandelbrot Fractal**: Real-time fractal zoom animation

## Hardware Requirements

- Raspberry Pi Pico W
- ST7789 LCD Display (320x240)
- 4 Push Buttons (A, B, X, Y)

## Button Controls

- **Button A**: Switch to Clock display
- **Button B**: Switch to Quote display
- **Button X**: Switch to Ball animation
- **Button Y**: Switch to Mandelbrot animation

## Dependencies

This project requires my display driver library which provides:
- ST7789 hardware initialization
- Graphics primitives (shapes, text, images)
- DMA-accelerated rendering utilities

## Build Instructions

1. Install the Pico SDK and set the `PICO_SDK_PATH` environment variable
2. Clone this repository and the display driver library
3. Create a build directory:
   ```bash
   mkdir build && cd build
   ```
4. Configure and build:
   ```bash
   cmake ..
   make
   ```
5. Flash the resulting `widget.uf2` file to your Pico W

## Time Synchronization

The clock module supports USB serial time synchronization. Send the current Unix epoch timestamp via USB serial in the format:
```
T <epoch_timestamp>
```

The system will respond with `OK` on success or an error message on failure.


## Configuration

### Clock Timezone
Edit `TZ_OFFSET_HOURS` in `src/clock.c` to set your timezone offset from UTC.

### Display Settings
The system is configured for a 320x240 ST7789 display. Modify `SCREEN_WIDTH` and `SCREEN_HEIGHT` in the display driver library if using a different resolution.

## Technical Details

### Mandelbrot Renderer
- Uses 32-bit fixed-point arithmetic (Q4.28 format)
- Implements cardioid and period-2 bulb optimizations
- Progressive rendering with automatic zoom
- Configurable iteration count with dynamic adjustment

### Ball Animation
- Pre-computed circle rendering for efficiency
- DMA-accelerated span drawing
- Velocity-based collision detection
- Color cycling on corner impacts

### Clock System
- Hardware RTC integration
- Simple timezone offset support
- USB serial synchronization protocol
- Automatic validity checking

## License

MIT (see `LICENSE`).

