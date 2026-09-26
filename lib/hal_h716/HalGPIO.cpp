#include "HalGPIO.h"
#include "pins_h716.h"
#include "TouchOrientation.h"
#include <BatteryMonitor.h>
#include <BoardConfig.h>
#include <Rtc.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <driver/rtc_io.h>
#include <cmath>
#include <ctime>

namespace {
uint8_t weekdayFromCalendarDate(const uint16_t year, const uint8_t month, const uint8_t day) {
  if (year < 1900 || month < 1 || month > 12 || day < 1 || day > 31) {
    return 0;
  }
  static constexpr uint8_t monthOffsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  uint16_t adjustedYear = year;
  if (month < 3) --adjustedYear;
  const uint8_t sundayZero = static_cast<uint8_t>(
      (adjustedYear + adjustedYear / 4 - adjustedYear / 100 + adjustedYear / 400 + monthOffsets[month - 1] + day) % 7);
  return static_cast<uint8_t>(sundayZero == 0 ? 7 : sundayZero);
}
constexpr unsigned long BATTERY_POLL_MS = 10000;
static unsigned long batteryLastPollMs = 0;
static int batteryCachedPercent = 100;
}

void HalGPIO::begin() {
    inputMgr.begin();
}

void HalGPIO::serviceTouchGestures() {
    touchPressedEvents = 0;
    touchReleasedEvents = 0;
    touchSwipeDirection = TouchSwipe::None;
    touchSwipeStartNx = 0.0f;
    touchSwipeStartNy = 0.0f;

    if (!inputMgr.hasTouch()) {
        return;
    }

    float sx = 0.0f, sy = 0.0f, ex = 0.0f, ey = 0.0f;
    if (inputMgr.wasSwipe(sx, sy, ex, ey)) {
        touchSwipeStartNx = sx;
        touchSwipeStartNy = sy;

        const float dx = ex - sx;
        const float dy = ey - sy;

        constexpr float kMinSwipeDistance = 0.05f;

        if (std::fabs(dx) >= kMinSwipeDistance || std::fabs(dy) >= kMinSwipeDistance) {
            TouchSwipe nativeDirection = TouchSwipe::None;
            if (std::fabs(dy) > std::fabs(dx)) {
                nativeDirection = (dy < 0.0f) ? TouchSwipe::Up : TouchSwipe::Down;
            } else {
                nativeDirection = (dx < 0.0f) ? TouchSwipe::Left : TouchSwipe::Right;
            }
            touchSwipeDirection = inx::touch::toDefaultOrientation(nativeDirection);
        }
    }
}

void HalGPIO::update() {
    inputMgr.update();
    serviceTouchGestures();
}

void HalGPIO::injectOneShotPress(uint8_t buttonIndex) {}

bool HalGPIO::isPressed(uint8_t buttonIndex) const {
    return inputMgr.isPressed(buttonIndex) || ((touchPressedEvents & (1 << buttonIndex)) != 0);
}

bool HalGPIO::wasPressed(uint8_t buttonIndex) const {
    return inputMgr.wasPressed(buttonIndex) || ((touchPressedEvents & (1 << buttonIndex)) != 0);
}

bool HalGPIO::wasAnyPressed() const {
    return inputMgr.wasAnyPressed() || (touchPressedEvents != 0);
}

bool HalGPIO::wasReleased(uint8_t buttonIndex) const {
    return inputMgr.wasReleased(buttonIndex) || ((touchReleasedEvents & (1 << buttonIndex)) != 0);
}

bool HalGPIO::wasAnyReleased() const {
    return inputMgr.wasAnyReleased() || (touchReleasedEvents != 0);
}

unsigned long HalGPIO::getHeldTime() const {
    return inputMgr.getPowerButtonHeldTime();
}

bool HalGPIO::hasTouch() const { return inputMgr.hasTouch(); }
bool HalGPIO::isTouchPressed() const { return inputMgr.isTouchPressed(); }
bool HalGPIO::wasTouchPressedAt(float& nx, float& ny) const { return inputMgr.wasTouchPressedAt(nx, ny); }
bool HalGPIO::isTouchHeldAt(float& nx, float& ny) const { return inputMgr.isTouchHeldAt(nx, ny); }
bool HalGPIO::wasTouchActivity() const { return inputMgr.wasTouchActivity(); }
bool HalGPIO::wasTouchTap(float& nx, float& ny) const { return inputMgr.wasTouchTap(nx, ny); }
unsigned long HalGPIO::lastTouchHeldMs() const { return inputMgr.lastTouchHeldMs(); }

bool HalGPIO::touchSwipeStart(float& nx, float& ny) const {
    nx = touchSwipeStartNx;
    ny = touchSwipeStartNy;
    return touchSwipeDirection != TouchSwipe::None;
}

HalGPIO::MotionGesture HalGPIO::readMotionGesture(uint8_t orientation, uint8_t mode, uint8_t sensitivity) {
    return MotionGesture::None;
}

void HalGPIO::startDeepSleep() {
    // 1. Wait for power button release so we don't instantly wake from the press that put us to sleep
    const auto pwrPin = static_cast<gpio_num_t>(H716_BUTTON_PIN);
    pinMode(H716_BUTTON_PIN, INPUT_PULLUP);
    while (digitalRead(H716_BUTTON_PIN) == LOW) {
        delay(50);
    }

    // 2. Shut down radios
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    btStop();

    // 3. Ensure RTC GPIO pullup is enabled during deep sleep so GPIO 21 does not float low
    if (rtc_gpio_is_valid_gpio(pwrPin)) {
        rtc_gpio_init(pwrPin);
        rtc_gpio_set_direction(pwrPin, RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pullup_en(pwrPin);
        rtc_gpio_pulldown_dis(pwrPin);
    }

    // 4. Configure power button ext1 deep sleep wakeup
    const uint64_t wakeMask = 1ULL << H716_BUTTON_PIN;
    esp_sleep_enable_ext1_wakeup(wakeMask, ESP_EXT1_WAKEUP_ANY_LOW);

    // 5. Enter deep sleep
    esp_deep_sleep_start();
}

int HalGPIO::getBatteryPercentage() const {
    const unsigned long now = millis();
    if (batteryLastPollMs != 0 && (now - batteryLastPollMs) < BATTERY_POLL_MS) {
        return batteryCachedPercent;
    }

    static const BatteryMonitor battery;
    uint16_t percent = 0;
    if (battery.readPercentageChecked(percent) && percent > 0) {
        batteryCachedPercent = percent;
    } else {
        uint32_t mvSum = 0;
        for (int i = 0; i < 4; ++i) {
            mvSum += analogReadMilliVolts(H716_BATTERY_ADC);
        }
        const uint16_t pinMv = static_cast<uint16_t>(mvSum / 4);
        const uint16_t batMv = pinMv * 2;
        if (batMv > 2800) {
            batteryCachedPercent = BatteryMonitor::percentageFromMillivolts(batMv);
        } else {
            batteryCachedPercent = 100;
        }
    }
    batteryLastPollMs = now;
    return batteryCachedPercent;
}

bool HalGPIO::isCharging() const {
    return isUsbConnected();
}

bool HalGPIO::isUsbConnected() const {
    if (BoardConfig::ACTIVE.usbDetect < 0) {
        return false;
    }
    return digitalRead(BoardConfig::ACTIVE.usbDetect) == HIGH;
}

bool HalGPIO::readDateTime(DateTime& outDateTime) const {
    freeink::Rtc rtc;
    if (!rtc.begin()) {
        return false;
    }
    freeink::Rtc::DateTime dt;
    if (!rtc.now(dt)) {
        return false;
    }
    outDateTime.year = dt.year;
    outDateTime.month = dt.month;
    outDateTime.day = dt.day;
    outDateTime.hour = dt.hour;
    outDateTime.minute = dt.minute;
    outDateTime.second = dt.second;
    const uint8_t calendarWeekday = weekdayFromCalendarDate(dt.year, dt.month, dt.day);
    outDateTime.weekday = calendarWeekday != 0 ? calendarWeekday : static_cast<uint8_t>(dt.weekday == 0 ? 7 : dt.weekday);
    return true;
}

bool HalGPIO::writeDateTime(const DateTime& dateTime) const {
    freeink::Rtc rtc;
    if (!rtc.begin()) {
        return false;
    }
    freeink::Rtc::DateTime dt;
    dt.year = dateTime.year;
    dt.month = dateTime.month;
    dt.day = dateTime.day;
    dt.hour = dateTime.hour;
    dt.minute = dateTime.minute;
    dt.second = dateTime.second;
    dt.weekday = static_cast<uint8_t>(dateTime.weekday == 7 ? 0 : dateTime.weekday);
    return rtc.set(dt);
}

bool HalGPIO::syncRtcFromSystemTime() const {
    const time_t now = time(nullptr);
    if (now < 1704067200) {
        return false;
    }
    struct tm localTime {};
    if (localtime_r(&now, &localTime) == nullptr) {
        return false;
    }
    DateTime dt;
    dt.year = static_cast<uint16_t>(localTime.tm_year + 1900);
    dt.month = static_cast<uint8_t>(localTime.tm_mon + 1);
    dt.day = static_cast<uint8_t>(localTime.tm_mday);
    dt.hour = static_cast<uint8_t>(localTime.tm_hour);
    dt.minute = static_cast<uint8_t>(localTime.tm_min);
    dt.second = static_cast<uint8_t>(localTime.tm_sec);
    dt.weekday = static_cast<uint8_t>(localTime.tm_wday == 0 ? 7 : localTime.tm_wday);
    return writeDateTime(dt);
}

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const {
    return WakeupReason::PowerButton;
}

bool HalGPIO::getTemperatureAndHumidity(float& outTempC, float& outHumidityPct) const { return false; }
