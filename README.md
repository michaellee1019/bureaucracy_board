# TimmonsLee BureaucracyBoard

This project is a custom PCB designed for playing board games that have an elaborate bureaucracy phase. It helps keep track of which step in the phase you have completed and reminds you with colored icons which phase is next.

A button-controlled LED sequence display system for ESP32-S3 with NeoPixels arranged in a 2D grid.

## Hardware Requirements

- Seeed XIAO ESP32-S3
- 21 NeoPixels connected to pin D1 (GPIO2)
- Button connected to pin D0 (GPIO1) with internal pull-up

## LED Grid Layout

The 21 LEDs are arranged in a 7×3 grid (columns A-G, rows 1-3):

```
Row 1:  A1(0)   B1(1)   C1(2)   D1(3)   E1(4)   F1(5)   G1(6)
Row 2:  A2(7)   B2(8)   C2(9)   D2(10)  E2(11)  F2(12)  G2(13)
Row 3:  A3(14)  B3(15)  C3(16)  D3(17)  E3(18)  F3(19)  G3(20)
```

## Features

- **RTOS-based button handling**: Interrupt-driven, reliable button detection using FreeRTOS
- **Hardware debouncing**: 200ms debounce with ISR-level filtering
- **Event-driven architecture**: Queue-based communication between interrupt and main task
- **Grid-based addressing**: Use easy grid coordinates (A1, B2, etc.) instead of LED indices
- **Hex color codes**: Use familiar "#RRGGBB" format for colors
- **Separate configuration file**: Edit `src/sequences.h` to change LED patterns
- **Optional debug output**: Production build runs fast with no serial delays

## How to Use

### 1. Build and Upload

**Production Build** (no debug output, fastest):
```bash
pio run -e controller -t upload
```

**Debug Build** (with serial output for troubleshooting):
```bash
pio run -e debug -t upload
pio run -e debug -t monitor
```

**Note:** The `-e` flag selects a specific environment. Without it, PlatformIO builds ALL environments (both controller and debug), which causes duplicate uploads.

**Quick commands:**
```bash
# Build only (no upload)
pio run -e controller

# Upload and immediately monitor (debug)
pio run -e debug -t upload -t monitor

# Clean build files
pio run -e controller -t clean
```

### 2. Configure LED Sequences

Edit `src/sequences.h` to define your LED patterns:

```cpp
const char* SEQUENCES_JSON = R"(
[
  {
    "A1": "#FFFFFF",
    "B1": "#FF0000",
    "C1": "#00FF00"
  },
  {
    "D2": "#0000FF",
    "E2": "#FFFF00"
  }
]
)";
```

**Common hex colors:**
- `#FFFFFF` - White
- `#FF0000` - Red
- `#00FF00` - Green
- `#0000FF` - Blue
- `#FFFF00` - Yellow
- `#FF00FF` - Magenta
- `#00FFFF` - Cyan
- `#FFA500` - Orange
- `#800080` - Purple

### 3. Operation

- **Power on**: Displays first sequence
- **Press button**: Cycles to next sequence
- **After last sequence**: Wraps back to first

## Technical Details

### Button Handling (RTOS)

The button uses a reliable interrupt-driven approach:

1. **Hardware interrupt** on GPIO1 (FALLING edge) triggers immediately on button press
2. **ISR debouncing** filters rapid repeated triggers (200ms window)
3. **FreeRTOS queue** passes events from ISR to main loop safely
4. **Event-driven loop** blocks on queue instead of polling, reducing CPU usage

This approach is much more reliable than polling `digitalRead()` and provides consistent response.

### Debug vs Production Builds

**Production Build** (`controller` environment):
- No serial output or debugging
- Faster startup (no 2-second delay)
- No delays from serial printing
- Ideal for final deployment

**Debug Build** (`debug` environment):
- Full serial debugging output at 115200 baud
- Shows button presses, LED mappings, and colors
- Useful for development and troubleshooting
- Adds `DEBUG=1` compile flag

The debug system uses preprocessor macros to completely remove debug code in production builds, ensuring zero performance overhead.

### Configuration

You can adjust these values in `src/main.cpp`:

- `strip.setBrightness(50)` - LED brightness (0-255)
- `debounceDelay` - Button debounce time in milliseconds (default: 200ms)

## Example Sequences

### Example 1: Simple pattern
```json
[
  {
    "A1": "#FF0000",
    "G1": "#FF0000"
  },
  {
    "A3": "#0000FF",
    "G3": "#0000FF"
  }
]
```

### Example 2: Full row
```json
[
  {
    "A1": "#FFFFFF",
    "B1": "#FFFFFF",
    "C1": "#FFFFFF",
    "D1": "#FFFFFF",
    "E1": "#FFFFFF",
    "F1": "#FFFFFF",
    "G1": "#FFFFFF"
  }
]
```

## Backward Compatibility

The system also supports RGB array format for colors:

```json
{
  "A1": [255, 0, 0]
}
```

But hex codes are recommended for easier color selection.

## Development

Built with PlatformIO using:
- Adafruit NeoPixel library
- ArduinoJson library
- Arduino framework for ESP32
- FreeRTOS (built into ESP32 Arduino Core)

## Memory Usage

- **Production build**: ~260KB flash, ~20KB RAM
- **Debug build**: ~269KB flash, ~20KB RAM
- Plenty of room for expansion on 8MB ESP32-S3!
