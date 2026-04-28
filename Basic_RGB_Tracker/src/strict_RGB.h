// This uses strict RGB bounding boxes to strictly find the colour
// in full lighing conditions.
// It provides the fastest computation for the ESP32-S3
// allowing for ultra-fast reaction times.
// This will run so fast on the ESP32-S3 that the limiting 
// factor will be the camera's framerate, not the processor.

#ifndef STRICT_RGB_H
#define STRICT_RGB_H

#include <Arduino.h>
#include "esp_camera.h"

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

// --- Global Threshold Variables ---
uint8_t min_r = 0, max_r = 31; // Red ranges from 0-31
uint8_t min_g = 0,  max_g = 63; // Green ranges from 0-63
uint8_t min_b = 0,  max_b = 31; // Blue ranges from 0-31

// Struct to hold our tracking results
struct TrackingResult {
  int x;
  int y;
  int pixelCount; // Tells us how big the object is (closer = bigger)
};

// Helper function to translate Web Hex into Robot Vision Thresholds
void setThresholdsFromHex(const char* hexString, int tolerance) {
  
  // 1. Convert the Hex string (e.g., "#067EC8") into a standard number
  // We start at index 1 to skip the '#' character
  long number = strtol(hexString + 1, NULL, 16);
  
  // 2. Extract standard 8-bit RGB values (0-255)
  uint8_t r_8bit = (number >> 16) & 0xFF;
  uint8_t g_8bit = (number >> 8) & 0xFF;
  uint8_t b_8bit = number & 0xFF;
  
  // 3. Downscale to RGB565 by bit-shifting (Fastest math possible)
  // Shift right by 3 divides by 8 (255 -> 31)
  // Shift right by 2 divides by 4 (255 -> 63)
  uint8_t target_r = r_8bit >> 3; 
  uint8_t target_g = g_8bit >> 2; 
  uint8_t target_b = b_8bit >> 3; 

  Serial.printf("Target Scaled Color -> R:%d, G:%d, B:%d\n", target_r, target_g, target_b);

  // 4. Apply the Tolerance Window (and prevent math wrapping below 0 or above max)
  // For Red (Max 31)
  min_r = (target_r > tolerance) ? (target_r - tolerance) : 0;
  max_r = (target_r + tolerance < 31) ? (target_r + tolerance) : 31;

  // For Green (Max 63 - Note we give green double the tolerance because the scale is twice as large)
  int green_tolerance = tolerance * 2;
  min_g = (target_g > green_tolerance) ? (target_g - green_tolerance) : 0;
  max_g = (target_g + green_tolerance < 63) ? (target_g + green_tolerance) : 63;

  // For Blue (Max 31)
  min_b = (target_b > tolerance) ? (target_b - tolerance) : 0;
  max_b = (target_b + tolerance < 31) ? (target_b + tolerance) : 31;

  Serial.printf("New Search Bounds: R[%d-%d], G[%d-%d], B[%d-%d]\n", min_r, max_r, min_g, max_g, min_b, max_b);
}

TrackingResult findColorBlob(camera_fb_t *fb) {
  TrackingResult result = {0, 0, 0};
  
  // The camera buffer is an array of 8-bit bytes, but RGB565 pixels are 16-bit.
  // We cast the buffer pointer so we process two bytes (one full pixel) at a time.
  uint16_t *pixels = (uint16_t *)fb->buf;
  
  // Calculate total pixels (e.g., 160 * 120 = 19,200)
  int total_pixels = fb->width * fb->height;
  
  long sum_x = 0;
  long sum_y = 0;

  // Scan every pixel in the frame
  for (int i = 0; i < total_pixels; i++) {
    
    // 1. Extract the raw pixel
    uint16_t p = pixels[i];

    // 2. Bitwise extraction of R, G, B
    uint8_t r = (p >> 11) & 0x1F; 
    uint8_t g = (p >> 5)  & 0x3F; 
    uint8_t b = p & 0x1F;         

    // 3. The Threshold Test
    if (r >= min_r && r <= max_r &&
        g >= min_g && g <= max_g &&
        b >= min_b && b <= max_b) {
      
      // TARGET FOUND! 
      // Calculate the 2D (X,Y) position from the 1D array index
      int current_x = i % fb->width;
      int current_y = i / fb->width;

      // Add to the Center of Mass calculation
      sum_x += current_x;
      sum_y += current_y;
      result.pixelCount++;
    }
  }

  // 4. Average the coordinates to find the center
  if (result.pixelCount > 0) {
    result.x = sum_x / result.pixelCount;
    result.y = sum_y / result.pixelCount;
  } else {
    // If we didn't see the color, output an error coordinate
    result.x = -1;
    result.y = -1;
  }

  return result;
}

// void setup() {
void initVision() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n--- Booting Robot Vision ---");

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

  // Disable the floating LED just in case
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  // Set target to your desired Hex Code.
  // The '1' is our tolerance. It means look for +/- 1 shades of Red and Blue, 
  // and +/- 2 shades of Green around our perfect target.
  setThresholdsFromHex("#067EC8", 1);
  
  Serial.println("Robot Vision Initialized. Ready to hunt.");
}

// void loop() {
void runVision() {
  // 1. Grab a frame from the camera
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(100);
    return;
  }

  // 2. Process the frame through the math function
  TrackingResult target = findColorBlob(fb);

  // 3. Output the results
  if (target.pixelCount > 0) {
    Serial.printf("TARGET LOCKED | X: %d, Y: %d | Size: %d pixels\n", target.x, target.y, target.pixelCount);
  } else {
    Serial.println("Scanning...");
  }

  // 4. CRITICAL: Return the frame buffer to the camera to free up memory
  esp_camera_fb_return(fb);

  // 5. Yield to FreeRTOS watchdog
  delay(10);
}

#endif