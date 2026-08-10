#include <Arduino.h>
#include <EPD_Painter.h>
#include <EPD_Painter_presets.h>
#include "benchmark_image.h"

EPD_Painter epd(EPD_LILYGO_EPD47_H716_PRESET);
uint8_t *fb;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n>>> H716 Success Replicator <<<");

  if (!epd.begin()) {
    Serial.println("EPD fail");
    while (1) delay(1000);
  }

  // Use 16 levels, Quality High
  epd.setGreyLevels(16);
  epd.setQuality(EPD_Painter::Quality::QUALITY_HIGH);

  fb = (uint8_t *)heap_caps_malloc(518400, MALLOC_CAP_SPIRAM);
  if (!fb) {
    Serial.println("Memory Error");
    while (1) delay(1000);
  }

  Serial.println("Copying raw bytes...");
  memcpy(fb, benchmark_image_data, 518400);

  Serial.println("Hardware Clear...");
  epd.clear();

  Serial.println("Painting...");
  epd.paint(fb);
  epd.paint(fb); // Double paint for better density

  Serial.println("Done. This should match your FIRST successful result but be upright.");
}

void loop() {
  delay(1000);
}
