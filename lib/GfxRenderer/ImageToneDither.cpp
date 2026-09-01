#include "ImageToneDither.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

// Hot per-pixel dithering math for every image render; opted into -O2 (see JpegRender.cpp for why - the
// firmware otherwise builds with -Os). Placed after includes so it doesn't affect inlined header code.
#pragma GCC optimize("O2")

namespace {
int clamp255(const int v) { return std::max(0, std::min(255, v)); }

constexpr int kCleanPaperMin = 252;

// Amplitude of the ordered lattice that breaks Floyd-Steinberg "worm" lines on flat gray areas.
// Larger = more texture breakup (but more visible dither); 0 = pure FS (worms return).
constexpr int kGrayscaleMicroDither = 20;

int perceptualTone(const int gray) {
  return clamp255(gray);
}

}  // namespace

FourToneImageDitherer::FourToneImageDitherer(const int width) : width_(width) {
  if (width_ <= 0) {
    width_ = 0;
    return;
  }
  const size_t rowElems = static_cast<size_t>(width_) + 4u;
  const size_t totalElems = 9u * rowElems;
  buffer_ = static_cast<int16_t*>(std::calloc(totalElems, sizeof(int16_t)));
  if (!buffer_) {
    width_ = 0;
    return;
  }
  for (int plane = 0; plane < 3; plane++) {
    for (int row = 0; row < 3; row++) {
      errorRows_[plane][row] = buffer_ + (plane * 3 + row) * rowElems;
    }
  }
}

FourToneImageDitherer::~FourToneImageDitherer() {
  std::free(buffer_);
}

bool FourToneImageDitherer::ok() const {
  return width_ > 0 && buffer_ != nullptr;
}

ImageToneSample FourToneImageDitherer::quantize(const int gray) {
  const int g = clamp255(gray);
  ImageToneSample sample;
  if (g < 42) {
    sample.level = 3;
    sample.value = 0;
  } else if (g < 128) {
    sample.level = 1;
    sample.value = 85;
  } else if (g < kCleanPaperMin) {
    sample.level = 2;
    sample.value = 170;
  } else {
    sample.level = 0;
    sample.value = 255;
  }
  return sample;
}

uint8_t FourToneImageDitherer::levelFromValue(const int value) {
  if (value <= 42) return 3;
  if (value <= 128) return 1;
  if (value <= 251) return 2;
  return 0;
}

bool FourToneImageDitherer::bwInkForLevel(const uint8_t level, const int x, const int y) {
  if (level == 3) return true;
  if (level == 1) return ((x + y) & 1) == 0;
  if (level == 2) return ((x & 1) == 0) && ((y & 1) == 0);
  return false;
}

bool FourToneImageDitherer::bwPreviewInkForLevel(const uint8_t level, const int x, const int y) {
  (void)x;
  (void)y;
  return level > 0;
}

ImageToneSample FourToneImageDitherer::process(const int gray, const int x) {
  if (!ok() || x < 0 || x >= width_) {
    return quantize(perceptualTone(gray));
  }

  const int base = perceptualTone(gray);
  if (base >= kCleanPaperMin) {
    return quantize(255);
  }

  const int adjusted = clamp255(base + errorRows_[0][0][x + 2]);
  if (adjusted >= kCleanPaperMin) {
    return quantize(255);
  }

  const ImageToneSample out = quantize(adjusted);
  const int spread = (adjusted - static_cast<int>(out.value)) >> 3;

  if (x + 1 < width_) errorRows_[0][0][x + 3] += static_cast<int16_t>(spread);
  if (x + 2 < width_) errorRows_[0][0][x + 4] += static_cast<int16_t>(spread);
  if (x > 0) errorRows_[0][1][x + 1] += static_cast<int16_t>(spread);
  errorRows_[0][1][x + 2] += static_cast<int16_t>(spread);
  if (x + 1 < width_) errorRows_[0][1][x + 3] += static_cast<int16_t>(spread);
  errorRows_[0][2][x + 2] += static_cast<int16_t>(spread);

  return out;
}

ImageToneSample FourToneImageDitherer::processGrayscaleFS(const int gray, const int x) {
  if (!ok() || x < 0 || x >= width_) {
    return quantize(perceptualTone(gray));
  }

  const int base = perceptualTone(gray);
  if (base >= kCleanPaperMin) {
    return quantize(255);
  }

  const int adjusted = clamp255(base + errorRows_[0][0][x + 2]);
  if (adjusted >= kCleanPaperMin) {
    return quantize(255);
  }

  // Ordered lattice bias breaks the Floyd-Steinberg "worm" lines on flat gray areas. It is a pure
  // function of (x, row_), so both grayscale passes (LSB then MSB) still produce identical levels.
  const int lattice = ((x * 13 + row_ * 7 + ((x ^ row_) * 3)) & 15) - 8;
  const int biased = clamp255(adjusted + (lattice * kGrayscaleMicroDither) / 12);

  const ImageToneSample out = quantize(biased);
  // Diffuse the true tone error (from adjusted, not the lattice-biased value) so the lattice only
  // perturbs the quantization decision and does not propagate as new worms.
  const int error = adjusted - static_cast<int>(out.value);
  if (error == 0) {
    return out;
  }

  // Floyd-Steinberg (7/3/5/1 over 16): smooth, conserves all error - no Atkinson speckle.
  if (x + 1 < width_) errorRows_[0][0][x + 3] += static_cast<int16_t>((error * 7) / 16);
  if (x > 0) errorRows_[0][1][x + 1] += static_cast<int16_t>((error * 3) / 16);
  errorRows_[0][1][x + 2] += static_cast<int16_t>((error * 5) / 16);
  if (x + 1 < width_) errorRows_[0][1][x + 3] += static_cast<int16_t>(error / 16);

  return out;
}

ImageToneSample FourToneImageDitherer::processAtkinson(const int gray, const int x) {
  if (!ok() || x < 0 || x >= width_) {
    return quantize(perceptualTone(gray));
  }

  const int base = perceptualTone(gray);
  if (base >= kCleanPaperMin) {
    return quantize(255);
  }

  const int adjusted = clamp255(base + errorRows_[0][0][x + 2]);
  if (adjusted >= kCleanPaperMin) {
    return quantize(255);
  }

  const ImageToneSample out = quantize(adjusted);
  const int spread = (adjusted - static_cast<int>(out.value)) / 8;
  if (spread == 0) {
    return out;
  }

  if (x + 1 < width_) errorRows_[0][0][x + 3] += static_cast<int16_t>(spread);
  if (x + 2 < width_) errorRows_[0][0][x + 4] += static_cast<int16_t>(spread);
  if (x > 0) errorRows_[0][1][x + 1] += static_cast<int16_t>(spread);
  errorRows_[0][1][x + 2] += static_cast<int16_t>(spread);
  if (x + 1 < width_) errorRows_[0][1][x + 3] += static_cast<int16_t>(spread);
  errorRows_[0][2][x + 2] += static_cast<int16_t>(spread);

  return out;
}

ImageToneSample FourToneImageDitherer::processQuality(const int gray, const int x) {
  if (!ok() || x < 0 || x >= width_) {
    return quantize(gray);
  }

  const int adjusted = clamp255(gray + errorRows_[0][0][x + 2]);
  const ImageToneSample out = quantize(adjusted);
  const int error = adjusted - static_cast<int>(out.value);

  if (error == 0) {
    return out;
  }

  if (x + 1 < width_) errorRows_[0][0][x + 3] += static_cast<int16_t>((error * 7) / 16);
  if (x > 0) errorRows_[0][1][x + 1] += static_cast<int16_t>((error * 3) / 16);
  errorRows_[0][1][x + 2] += static_cast<int16_t>((error * 5) / 16);
  if (x + 1 < width_) errorRows_[0][1][x + 3] += static_cast<int16_t>(error / 16);

  return out;
}

void FourToneImageDitherer::nextRow() {
  for (int plane = 0; plane < 3; plane++) {
    int16_t* temp = errorRows_[plane][0];
    errorRows_[plane][0] = errorRows_[plane][1];
    errorRows_[plane][1] = errorRows_[plane][2];
    errorRows_[plane][2] = temp;
    if (errorRows_[plane][2]) {
      std::memset(errorRows_[plane][2], 0, (static_cast<size_t>(width_) + 4u) * sizeof(int16_t));
    }
  }
  row_++;
}

void FourToneImageDitherer::reset() {
  for (int plane = 0; plane < 3; plane++) {
    for (int row = 0; row < 3; row++) {
      if (errorRows_[plane][row]) {
        std::memset(errorRows_[plane][row], 0, (static_cast<size_t>(width_) + 4u) * sizeof(int16_t));
      }
    }
  }
  row_ = 0;
}
