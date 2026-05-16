#pragma once

#include <string>

enum class LedState {
    Off,
    Waiting,    // slow blink: powered on, waiting for the phone
    Connected,  // solid: phone connected, Android Auto running
    Error,      // fast blink: something went wrong
};

/**
 * Drives a Linux sysfs LED (e.g. the Raspberry Pi onboard ACT LED) to give
 * the driver at-a-glance feedback on an otherwise headless dongle.
 *
 * Disabled unless AAWG_STATUS_LED names an LED under /sys/class/leds.
 */
class StatusLed {
public:
    static StatusLed& instance();

    void set(LedState state);

private:
    StatusLed();
    StatusLed(StatusLed const&);
    StatusLed& operator=(StatusLed const&);

    void writeLedFile(const std::string& file, const std::string& value);

    bool m_enabled = false;
    std::string m_ledPath;
    int m_maxBrightness = 1;
};
