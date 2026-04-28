# ESP32-S3 Ultra-Fast RGB Color Tracker

## Overview
This project provides an ultra-fast, raw color blob tracking algorithm designed specifically for the ESP32-S3 microprocessor equipped with a camera module. By processing raw `RGB565` image data directly from the camera's frame buffer (bypassing slow JPEG compression), the algorithm achieves processing speeds limited only by the camera's framerate. 

It tracks a user-defined target color (inputted as a standard web Hex code) and calculates the center of mass and size of the detected color blob in real-time. 

## Features
* **Blazing Fast Execution:** Uses direct memory access and bitwise math on raw `RGB565` pixels, avoiding costly format conversions.
* **Web-Friendly Color Input:** Converts standard Hex color codes (e.g., `#067EC8`) into camera-native 5-6-5 bit RGB thresholds automatically.
* **Center of Mass Calculation:** Returns the `X, Y` coordinates of the tracked object, making it ideal for robotic tracking and motor control.
* **Proximity Estimation:** Returns the total pixel count of the blob, which can be used to estimate how close the object is to the lens.
* **Low Resolution Target:** Locked to `QQVGA` (160x120) for maximum framerate and stability.

## Hardware Requirements
* **Microcontroller:** ESP32-S3 (WROOM or similar)
* **Camera:** Standard ESP32 compatible camera module (e.g., OV2640, configured for Freenove or generic S3 pinouts).

## Usage

### 1. Configuration
The camera pinout is pre-configured for generic ESP32-S3 WROOM / Freenove boards. If you are using an AI-Thinker board or a different custom board, you will need to update the `GPIO_NUM` definitions at the top of the header file.

### 2. Setting the Target Color
Inside the `initVision()` function, the target color is set using the `setThresholdsFromHex` function:

```cpp
// Look for Blue (#067EC8) with a tolerance of 1
setThresholdsFromHex("#067EC8", 1);
```

* **Parameter 1: The target Hex color.

* **Parameter 2: The tolerance level. Because RGB565 scales values down (Red/Blue max 31, Green max 63), a tolerance of 1 or 2 provides a strict color match.

### 3. Implementation in main.cpp or Sketch
Include the header file and call the initialization and loop functions within your main Arduino structure:

```cpp
#include "STRICT_RGB_H"

void setup() {
  // Initializes Serial, configures the camera, and sets the color thresholds
  initVision();
}

void loop() {
  // Grabs a frame, finds the blob, and prints the X/Y coordinates
  runVision();
}
```

### Function Breakdown
* **setThresholdsFromHex(const char* hexString, int tolerance): Parses a web Hex string, downscales standard 8-bit RGB (0-255) into RGB565 (0-31, 0-63, 0-31), and calculates the acceptable search boundaries based on the tolerance window.

* **findColorBlob(camera_fb_t *fb): Iterates through the entire 160x120 frame buffer. If a pixel matches the calculated thresholds, its X/Y coordinates are added to a sum. Finally, it averages all valid coordinates to find the center of the blob. Returns a TrackingResult struct.

* **initVision(): Boots the camera with PIXFORMAT_RGB565 and FRAMESIZE_QQVGA configuration.

* **runVision(): Fetches the frame buffer, passes it to the math function, logs the coordinates via Serial, and crucially returns the frame buffer to prevent memory leaks.

### Important Limitations
* **Lighting Dependent: Because this algorithm uses strict RGB thresholds instead of HSV (Hue, Saturation, Value), it is highly sensitive to changes in lighting. It works best in fully lit environments with consistent, uniform lighting.

* **No Multiple Object Tracking: This algorithm calculates a single center of mass. If two objects of the exact same color are in the frame, the reported X, Y coordinates will be the midpoint between the two objects.
