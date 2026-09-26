#include "HalDisplay.h"
#include "EPD_Painter_Adafruit.h"
#include "pins_h716.h"
#include <esp_heap_caps.h>
#include <Wire.h>

// LilyGo EPD47 S3 uses a 960x540 display.
#define H716_WIDTH 960
#define H716_HEIGHT 540

EPD_PainterAdafruit painter(EPD_PAINTER_PRESET);

static uint64_t lutBW[256];
static bool lutInitialized = false;

static void initLut() {
    if (lutInitialized) return;
    for (int i = 0; i < 256; i++) {
        uint8_t bytes[8];
        for (int bit = 0; bit < 8; bit++) {
            // inx-pro: 0 is Ink (Black), 1 is Paper (White)
            bool ink = (i & (0x80 >> bit)) == 0;
            bytes[bit] = ink ? 3 : 0; // 3 is Black, 0 is White in EPD_Painter
        }
        memcpy(&lutBW[i], bytes, 8);
    }
    lutInitialized = true;
}

HalDisplay::HalDisplay() {}

HalDisplay::~HalDisplay() {
    if (_bwBuffer) heap_caps_free(_bwBuffer);
    if (_lsbBuffer) heap_caps_free(_lsbBuffer);
    if (_msbBuffer) heap_caps_free(_msbBuffer);
}

void HalDisplay::begin() {
    printf("[H716] HalDisplay::begin()\n");

    painter.setAutoShutdown(false);

    if (!painter.begin()) {
        printf("[H716] !! EPD_Painter::begin() FAILED\n");
    } else {
        printf("[H716] EPD_Painter initialized successfully\n");
    }

    painter.setQuality(EPD_Painter::Quality::QUALITY_NORMAL);
    initLut();

    // Allocate 1bpp buffers in PSRAM, with fallback to internal RAM
    _bwBuffer = (uint8_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_bwBuffer) {
        printf("[H716] PSRAM allocation failed for _bwBuffer, trying internal RAM\n");
        _bwBuffer = (uint8_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    _lsbBuffer = (uint8_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_lsbBuffer) {
        _lsbBuffer = (uint8_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    _msbBuffer = (uint8_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_msbBuffer) {
        _msbBuffer = (uint8_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    if (!_bwBuffer) {
        printf("[H716] CRITICAL ERROR: _bwBuffer allocation failed completely!\n");
        return;
    }

    memset(_bwBuffer, 0xFF, BUFFER_SIZE); // White baseline
    if (_lsbBuffer) memset(_lsbBuffer, 0xFF, BUFFER_SIZE);
    if (_msbBuffer) memset(_msbBuffer, 0xFF, BUFFER_SIZE);

    // Initial hardware screen clear to reset physical glass to solid white
    painter.clear();

    printf("[H716] HalDisplay initialized. Buffer size: %u bytes\n", BUFFER_SIZE);
}

void HalDisplay::clearScreen(uint8_t color) const {
    if (_bwBuffer) memset(_bwBuffer, color, BUFFER_SIZE);
}

void HalDisplay::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool fromProgmem) const {
}

void HalDisplay::displayBuffer(RefreshMode mode) {
    if (!_bwBuffer) {
        printf("[H716] displayBuffer error: _bwBuffer is NULL\n");
        return;
    }

    uint8_t* canvas = painter.getBuffer();
    if (!canvas) {
        printf("[H716] displayBuffer error: painter buffer is NULL\n");
        return;
    }

    static uint16_t consecutiveFastRefreshes = 0;

    bool doHardwareClear = (mode == HalDisplay::MANUAL_REFRESH ||
                            mode == HalDisplay::FULL_REFRESH ||
                            mode == HalDisplay::HALF_REFRESH ||
                            mode == HalDisplay::STRONG_FAST_REFRESH);

    if (mode == HalDisplay::FAST_REFRESH) {
        consecutiveFastRefreshes++;
        if (consecutiveFastRefreshes >= 6) {
            doHardwareClear = true;
            consecutiveFastRefreshes = 0;
        }
    } else {
        consecutiveFastRefreshes = 0;
    }

    if (doHardwareClear) {
        painter.clear(); // Erase residual charge to keep background pure white & UI text pitch-black
        painter.setQuality(EPD_Painter::Quality::QUALITY_HIGH);
    } else {
        painter.setQuality(EPD_Painter::Quality::QUALITY_NORMAL);
    }

    uint64_t* canvas64 = (uint64_t*)canvas;
    const uint8_t* src = _bwBuffer;
    uint32_t i = 0;
    for (; i + 3 < BUFFER_SIZE; i += 4) {
        canvas64[i]     = lutBW[src[i]];
        canvas64[i + 1] = lutBW[src[i + 1]];
        canvas64[i + 2] = lutBW[src[i + 2]];
        canvas64[i + 3] = lutBW[src[i + 3]];
    }
    for (; i < BUFFER_SIZE; i++) {
        canvas64[i] = lutBW[src[i]];
    }

    painter.paint();
}

void HalDisplay::displayBufferAsync(RefreshMode mode) {
    displayBuffer(mode);
}

void HalDisplay::refreshDisplay(RefreshMode mode, bool turnOffScreen) {
    displayBuffer(mode);
}

void HalDisplay::syncWriteBufferFromActive() const {
}

void HalDisplay::deepSleep() {
    painter.end();
}

uint8_t* HalDisplay::getFrameBuffer() const {
    return _bwBuffer;
}

void HalDisplay::copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) {
    if (_lsbBuffer && lsbBuffer) memcpy(_lsbBuffer, lsbBuffer, BUFFER_SIZE);
    if (_msbBuffer && msbBuffer) memcpy(_msbBuffer, msbBuffer, BUFFER_SIZE);
}

void HalDisplay::copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) {
    if (_lsbBuffer && lsbBuffer) memcpy(_lsbBuffer, lsbBuffer, BUFFER_SIZE);
}

void HalDisplay::copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) {
    if (_msbBuffer && msbBuffer) memcpy(_msbBuffer, msbBuffer, BUFFER_SIZE);
}

void HalDisplay::cleanupGrayscaleBuffers(const uint8_t* bwBuffer) {
    if (_bwBuffer && bwBuffer) memcpy(_bwBuffer, bwBuffer, BUFFER_SIZE);
}

void HalDisplay::displayGrayBuffer(bool quality, bool trackForRevert) {
    if (!_lsbBuffer || !_msbBuffer) return;

    uint8_t* canvas = painter.getBuffer();
    if (!canvas) return;

    for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
        uint8_t lsb = _lsbBuffer[i];
        uint8_t msb = _msbBuffer[i];
        for (int bit = 0; bit < 8; bit++) {
            bool lbit = (lsb & (0x80 >> bit)) == 0;
            bool mbit = (msb & (0x80 >> bit)) == 0;

            uint8_t val = 0;
            if (mbit && lbit) val = 3;       // Black
            else if (mbit) val = 2;          // Dark Gray
            else if (lbit) val = 1;          // Light Gray
                                             // else val = 0 (White)

            canvas[i * 8 + bit] = val;
        }
    }

    painter.setQuality(EPD_Painter::Quality::QUALITY_NORMAL);
    painter.paint();
}

void HalDisplay::displayGrayBufferFastQuality() {
    painter.setQuality(EPD_Painter::Quality::QUALITY_FAST);
    displayGrayBuffer(false, false);
}

void HalDisplay::displayGrayscaleBase(RefreshMode fallback, bool turnOffScreen) {
    (void)fallback;
    (void)turnOffScreen;
    displayGrayBuffer(true, true);
}

bool HalDisplay::refreshBusy() const {
    return false;
}

void HalDisplay::prepareQualityGrayscale() {
    painter.setQuality(EPD_Painter::Quality::QUALITY_HIGH);
}

uint16_t HalDisplay::getDisplayWidth() const { return H716_WIDTH; }
uint16_t HalDisplay::getDisplayHeight() const { return H716_HEIGHT; }
uint16_t HalDisplay::getDisplayWidthBytes() const { return H716_WIDTH / 8; }
uint32_t HalDisplay::getBufferSize() const { return (H716_WIDTH / 8) * H716_HEIGHT; }
