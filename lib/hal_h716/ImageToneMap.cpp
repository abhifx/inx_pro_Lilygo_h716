/**
 * @file ImageToneMap.cpp
 * @brief LilyGo EPD47 H716 image tone mapping.
 */

#include <cstdint>

// On-panel 2-bit plane codes for LilyGo H716 (MEDIUM path):
// level 0  white      -> 0b00 (msb=0, lsb=0)
// level 1  dark gray  -> 0b10 (msb=1, lsb=0)
// level 2  light gray -> 0b01 (msb=0, lsb=1)
// level 3  black      -> 0b11 (msb=1, lsb=1)
const uint8_t* grayscaleCodeTable() {
  static constexpr uint8_t kTable[4] = {
      0b00,  // level 0  white
      0b10,  // level 1  dark gray
      0b01,  // level 2  light gray
      0b11,  // level 3  black
  };
  return kTable;
}

// Quality (GRAY2) path for H716:
// level 0  white      -> 0b00
// level 1  dark gray  -> 0b10
// level 2  light gray -> 0b01
// level 3  black      -> 0b11
uint8_t mapQualityGray2Level(const uint8_t level) {
  const uint8_t l = level & 3u;
  if (l == 0u) return 0u;  // White      -> 0b00
  if (l == 1u) return 2u;  // Dark Gray  -> 0b10
  if (l == 2u) return 1u;  // Light Gray -> 0b01
  return 3u;               // Black      -> 0b11
}

// Text AA code table shares the image table.
const uint8_t* mediumTextCodeTable() { return grayscaleCodeTable(); }

// Identity tone curve
int applyDeviceToneCurve(const int gray) { return gray; }
