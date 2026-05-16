#include <fstream>

#include "common.h"
#include "statusled.h"

StatusLed& StatusLed::instance() {
    static StatusLed instance;
    return instance;
}

StatusLed::StatusLed() {
    std::string ledName = Config::instance()->getStatusLed();
    if (ledName.empty()) {
        Logger::instance()->info("Status LED: disabled (AAWG_STATUS_LED not set)\n");
        return;
    }

    m_ledPath = "/sys/class/leds/" + ledName;

    std::ifstream maxFile(m_ledPath + "/max_brightness");
    if (!maxFile.is_open()) {
        Logger::instance()->info("Status LED: %s not found, disabling\n", m_ledPath.c_str());
        return;
    }
    maxFile >> m_maxBrightness;
    if (m_maxBrightness <= 0) {
        m_maxBrightness = 1;
    }

    m_enabled = true;
    Logger::instance()->info("Status LED: using %s\n", m_ledPath.c_str());
}

void StatusLed::writeLedFile(const std::string& file, const std::string& value) {
    std::ofstream ledFile(m_ledPath + "/" + file);
    if (ledFile.is_open()) {
        ledFile << value;
    }
}

void StatusLed::set(LedState state) {
    if (!m_enabled) {
        return;
    }

    switch (state) {
        case LedState::Off:
            writeLedFile("trigger", "none");
            writeLedFile("brightness", "0");
            break;
        case LedState::Connected:
            writeLedFile("trigger", "none");
            writeLedFile("brightness", std::to_string(m_maxBrightness));
            break;
        case LedState::Waiting:
            // Slow blink via the kernel timer trigger.
            writeLedFile("trigger", "timer");
            writeLedFile("delay_on", "900");
            writeLedFile("delay_off", "900");
            break;
        case LedState::Error:
            // Fast blink via the kernel timer trigger.
            writeLedFile("trigger", "timer");
            writeLedFile("delay_on", "150");
            writeLedFile("delay_off", "150");
            break;
    }
}
