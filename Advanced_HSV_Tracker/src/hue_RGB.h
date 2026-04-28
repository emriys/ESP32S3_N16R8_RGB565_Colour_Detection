// 

#ifndef HUE_RGB_H
#define HUE_RGB_H

#include <Arduino.h>
#include "esp_camera.h"
#include <Adafruit_NeoPixel.h>

// ===========================
// Camera Pinout (Freenove / Generic S3 WROOM)
// ===========================
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5

#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

// --- ONBOARD RGB LED CONFIG ---
#define LED_PIN 48
#define NUMPIXELS 1
Adafruit_NeoPixel statusLED(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- Global HSV Threshold Variables ---
// We are looking for Blue (#067EC8)
float min_h = 0.0, max_h = 0.0;

// We want vibrant colors only. Ignore anything washed out or greyish.
float min_s = 0.3; // 30% to 100% saturation (allows for glares/washout)
float max_s = 1.0;

// We want somewhat lit objects. Ignore pitch black shadows.
float min_v = 0.2; // 20% to 100% brightness (survives deep shadows)
float max_v = 1.0;

// NOISE FILTER: Ignore blobs smaller than this many pixels
const int MIN_BLOB_SIZE = 50; 

struct TrackingResult {
  int x;
  int y;
  int pixelCount; 
};

// --- Hex to Dynamic HSV Bounds Converter ---
void setTargetColorHex(const char* hexString, float hueTolerance) {
  // 1. Convert Hex to standard 8-bit RGB
  long number = strtol(hexString + 1, NULL, 16);
  float r = ((number >> 16) & 0xFF) / 255.0f;
  float g = ((number >> 8)  & 0xFF) / 255.0f;
  float b = (number & 0xFF)         / 255.0f;

  // 2. Convert to a single Target Hue
  float max_c = max(r, max(g, b));
  float min_c = min(r, min(g, b));
  float delta = max_c - min_c;
  float target_h = 0.0f;

  if (delta != 0.0f) {
    if (max_c == r) target_h = 60.0f * fmod(((g - b) / delta), 6.0f);
    else if (max_c == g) target_h = 60.0f * (((b - r) / delta) + 2.0f);
    else if (max_c == b) target_h = 60.0f * (((r - g) / delta) + 4.0f);
    if (target_h < 0.0f) target_h += 360.0f;
  }

  // 3. Build the Tolerance Window and handle 360-degree wrap-around
  min_h = target_h - hueTolerance;
  max_h = target_h + hueTolerance;

  if (min_h < 0.0f) min_h += 360.0f;
  if (max_h > 360.0f) max_h -= 360.0f;

  Serial.printf("Target Hex %s -> Perfect Hue: %.1f deg\n", hexString, target_h);
  Serial.printf("Tracking Hue Window: %.1f to %.1f\n", min_h, max_h);
}

// --- Fast RGB565 to HSV Converter ---
void rgb565_to_hsv(uint16_t rgb565, float &h, float &s, float &v) {
  // 1. Extract and normalize RGB to 0.0 - 1.0
  float r = ((rgb565 >> 11) & 0x1F) / 31.0f;
  float g = ((rgb565 >> 5)  & 0x3F) / 63.0f;
  float b = (rgb565 & 0x1F)         / 31.0f;

  float max_c = max(r, max(g, b));
  float min_c = min(r, min(g, b));
  float delta = max_c - min_c;

  // Value is just the maximum color component
  v = max_c;

  // Saturation is the difference divided by the max
  s = (max_c == 0.0f) ? 0.0f : (delta / max_c);

  // Hue calculation
  if (delta == 0.0f) {
    h = 0.0f; // It's a shade of grey
  } else {
    if (max_c == r) {
      h = 60.0f * fmod(((g - b) / delta), 6.0f);
    } else if (max_c == g) {
      h = 60.0f * (((b - r) / delta) + 2.0f);
    } else if (max_c == b) {
      h = 60.0f * (((r - g) / delta) + 4.0f);
    }
    if (h < 0.0f) h += 360.0f;
  }
}

// --- The New HSV Tracking Algorithm ---
TrackingResult findColorBlob(camera_fb_t *fb) {
  TrackingResult result = {0, 0, 0};
  uint16_t *pixels = (uint16_t *)fb->buf;
  int total_pixels = fb->width * fb->height;
  
  long sum_x = 0;
  long sum_y = 0;

  for (int i = 0; i < total_pixels; i++) {
    // 1. Read the raw pixel
    uint16_t p = pixels[i];
    
    // 2. THE FIX: Swap the high and low bytes to correct the hardware endianness mismatch
    p = __builtin_bswap16(p); 
    
    float h, s, v;
    rgb565_to_hsv(p, h, s, v);

    // 1. Check if Hue matches (with wrap-around support)
    bool hue_match = false;
    if (min_h <= max_h) {
      hue_match = (h >= min_h && h <= max_h); // Standard window
    } else {
      hue_match = (h >= min_h || h <= max_h); // Wrapped window (e.g., crossing 360 to 0)
    }

    // 2. The Final Threshold Test
    if (hue_match && s >= min_s && s <= max_s && v >= min_v && v <= max_v) {
      int current_x = i % fb->width;
      int current_y = i / fb->width;

      sum_x += current_x;
      sum_y += current_y;
      result.pixelCount++;
    }
  }

  // Apply the Noise Filter!
  if (result.pixelCount > MIN_BLOB_SIZE) {
    result.x = sum_x / result.pixelCount;
    result.y = sum_y / result.pixelCount;
  } else {
    result.x = -1;
    result.y = -1;
    result.pixelCount = 0; // Reset it so the main loop knows it was just noise
  }

  return result;
}

// void setup() {
void initHueVision() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n--- Booting Robot Vision ---");

  // --- Initialize the RGB LED ---
  statusLED.begin();           // Start the data stream
  statusLED.setBrightness(50); // Set brightness (0-255). 50 is plenty bright!
  statusLED.clear();           // Turn it off initially
  statusLED.show();            // Push the command to the LED

  // 1. Initialize Camera hardware
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 10000000; // 10MHz for stability

  // CRITICAL CHANGES FOR VISION PROCESSING
  config.pixel_format = PIXFORMAT_RGB565; // Raw color math, no JPEG compression!
  config.frame_size = FRAMESIZE_QQVGA;    // 160x120 resolution for maximum speed
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  // --- THE FIX: Lock the Sensor Settings ---
  sensor_t * s = esp_camera_sensor_get();
  s->set_whitebal(s, 0);       // Disable Automatic White Balance
  s->set_awb_gain(s, 0);       // Disable AWB Gain
  s->set_exposure_ctrl(s, 1);  // Keep Auto-Exposure ON so it handles shadows
  // -----------------------------------------

  // Disable the floating white LED just in case
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  // Set target to your specific colour, with a generous 5-15 degree hue tolerance
  setTargetColorHex("#067EC8", 5.0);
  // setTargetColorHex("#FFD700",10.0);
  
  Serial.println("Robot Vision Initialized. Ready to hunt.");
}

// void loop() {
void runHueVision() {
  // 1. Grab a frame from the camera
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(100);
    return;
  }

  // 2. Process the frame through our math function
  TrackingResult target = findColorBlob(fb);

  // 3. Output the results (We'll add motor logic here eventually)
  if (target.pixelCount > 0) {
    Serial.printf("TARGET LOCKED | X: %d, Y: %d | Size: %d pixels\n", target.x, target.y, target.pixelCount);
    // Set LED to Green (R, G, B)
    statusLED.setPixelColor(0, statusLED.Color(0, 255, 0));
    statusLED.show();
  } else {
    Serial.println("Scanning...");
    // Turn LED off (or set it to Red while searching!)
    statusLED.setPixelColor(0, statusLED.Color(150, 0, 0));
    statusLED.show();
  }

  // 4. CRITICAL: Return the frame buffer to the camera to free up memory
  esp_camera_fb_return(fb);

  // 5. Yield to FreeRTOS watchdog
  delay(10);
}

#endif