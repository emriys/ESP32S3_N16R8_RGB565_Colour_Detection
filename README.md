# ESP32-S3 Ultra-Fast Color Tracking Vision

## Overview
This repository contains two ultra-fast, raw color blob tracking algorithms designed specifically for the ESP32-S3 microcontroller equipped with an OV5640 camera module. 

By processing raw `RGB565` image data directly from the camera's frame buffer, and bypassing slow JPEG compression entirely, these algorithms achieve real-time processing speeds limited only by the camera's framerate. Both versions track a user-defined target color (using standard web Hex codes) and calculate the center of mass (`X, Y`) and the pixel size of the detected object.

## Which Version Should I Choose?

| Feature | Basic RGB Tracker | Advanced HSV Tracker |
| :--- | :--- | :--- |
| **Best For** | Highly controlled, consistent lighting environments. | Real-world environments with shadows, glares, or changing light. |
| **Color Math** | Strict RGB matching. | Hue tracking (filters out low saturation/brightness). |
| **Speed** | Absolute maximum framerate. | Slightly heavier math, but still incredibly fast. |
| **Status Feedback** | Serial Monitor only. | Visual feedback via onboard NeoPixel (WS2812). |
| **Noise Filtering** | None (tracks every matching pixel). | Includes a Minimum Blob Size filter to ignore rogue artifacts. |
| **Sensor Override** | Default camera auto-settings. | Overrides Auto-White Balance (AWB) to prevent color shifting. |

## Repository Structure
```text
ESP32S3_N16R8_RGB565_Colour_Detection/
│
├── Basic_RGB_Tracker/
│   ├── include
│   ├── lib
│   ├── src
│   │ ├── main.cpp
│   │ └── strict_RGB.h
│   ├── test
│   ├── .gitignore
│   └── platform.io
│
├── Advanced_HSV_Tracker/
│   ├── include
│   ├── lib
│   ├── src
│   │ ├── main.cpp
│   │ └── hue_RGB.h
│   ├── test
│   ├── .gitignore
│   └── platform.io
└── README.md
```

## Hardware Requirements
* **Microcontroller:** ESP32-S3 (WROOM or similar generic boards). Mine is configured for the N16R8 with `psram`.

* **Camera:** Standard ESP32 compatible camera module (I used the OV5640). Pinouts are pre-configured for Freenove boards.

* **Status LED:** (HSV Version Only) Onboard WS2812 / NeoPixel on GPIO 48.

## Installation & Setup
### For Arduino IDE Users
Download or clone this repository.

1. Open either `Basic_RGB_Tracker/main.cpp` or `Advanced_HSV_Tracker/main.cpp` in the Arduino IDE (you may need to rename `.cpp` to `.ino` depending on your IDE version).

2. Ensure you have the ESP32 board manager installed and your specific board selected.

3. **HSV Version Only:** Install the `Adafruit NeoPixel` library via the Library Manager.

4. Compile and upload.

### For PlatformIO Users
1. Clone this repository into your PlatformIO workspace.

2. Open either the `Basic_RGB_Tracker` or `Advanced_HSV_Tracker` folder as your active project.

3. Initialize your `platformio.ini` file for your specific ESP32-S3 board (e.g., `board = esp32-s3-devkitc-1` or `4d_systems_esp32s3_gen4_r8n16` if you have my exact configuration).

4. **HSV Version Only:** Add the NeoPixel library to your platformio.ini dependencies:

```Ini, TOML
lib_deps = adafruit/Adafruit NeoPixel @ ^1.11.0
```

5. Build and upload.

### Quick Configuration
1. **Pinouts:**
If you are using an AI-Thinker board or a custom PCB, ensure you update the Camera GPIO_NUM definitions at the top of the header files to match your specific hardware.

2. **Setting Your Target Color:**
Both algorithms use standard Web Hex codes to find colors, making it incredibly easy to configure.

**For the Basic RGB Version:**
Inside `initVision()`, define the Hex code and a tight tolerance (usually 1 to 5).

```C++
// Look for Blue (#067EC8) with an RGB565 tolerance of 1
setThresholdsFromHex("#067EC8", 1);
```

**For the Advanced HSV Version:**
Inside `initHueVision()`, define the Hex code and a generous Hue degree window (usually 5.0 to 15.0).

```C++
// Look for Blue (#067EC8) with a 5-degree Hue tolerance window
setTargetColorHex("#067EC8", 5.0);
```

3. **Tuning the HSV Tracker:**
The HSV tracker includes several global variables at the top of the file to fine-tune the vision:

* `min_s`: Increase this (e.g., 0.3) to ignore grayish, washed-out colors.

* `min_v`: Increase this (e.g., 0.2) to ignore pitch-black shadows.

* `MIN_BLOB_SIZE`: Increase this (e.g., 50) if the tracker is getting distracted by tiny specs of color in the background.

### Important Limitations
* **Resolution:** Both trackers lock the camera to QQVGA (160x120). This low resolution is intentional; it keeps the memory footprint small and ensures the highest possible framerate for real-time robotic reactions.

* **Single Object Tracking:** These algorithms calculate a single Center of Mass. If two objects of the exact same target color are in the frame, the reported X, Y coordinates will be the midpoint between the two objects.
