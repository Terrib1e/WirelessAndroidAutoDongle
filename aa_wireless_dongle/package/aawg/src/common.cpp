#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <sstream>
#include <fstream>
#include <syslog.h>

#include "common.h"
#include "proto/WifiInfoResponse.pb.h"

#pragma region Config
/*static*/ Config* Config::instance() {
    static Config s_instance;
    return &s_instance;
}

int32_t Config::getenv(std::string name, int32_t defaultValue) {
    char* envValue = std::getenv(name.c_str());
    try {
        return envValue != nullptr ? std::stoi(envValue) : defaultValue;
    }
    catch(...) {
        return defaultValue;
    }
}

std::string Config::getenv(std::string name, std::string defaultValue) {
    char* envValue = std::getenv(name.c_str());
    return envValue != nullptr ? envValue : defaultValue;
}

std::string Config::getMacAddress(std::string interface) {
    std::ifstream addressFile("/sys/class/net/" + interface + "/address");
    if (!addressFile.is_open()) {
        Logger::instance()->info("Config: could not open MAC address file for interface %s\n", interface.c_str());
        return "";
    }

    std::string macAddress;
    getline(addressFile, macAddress);

    return macAddress;
}

std::string Config::getUniqueSuffix() {
    std::string uniqueSuffix = getenv("AAWG_UNIQUE_NAME_SUFFIX", "");
    if (!uniqueSuffix.empty()) {
        return uniqueSuffix;
    }

    std::ifstream serialNumberFile("/sys/firmware/devicetree/base/serial-number");
    if (!serialNumberFile.is_open()) {
        Logger::instance()->info("Config: could not open device serial-number, using fallback suffix\n");
    }

    std::string serialNumber;
    getline(serialNumberFile, serialNumber);

    // Removing trailing null from serialNumber, pad at the beginning
    serialNumber = std::string("00000000") + serialNumber.c_str();

    return serialNumber.substr(serialNumber.size() - 6);
}

WifiInfo Config::getWifiInfo() {
    return {
        getenv("AAWG_WIFI_SSID", "AAWirelessDongle"),
        getenv("AAWG_WIFI_PASSWORD", "ConnectAAWirelessDongle"),
        getenv("AAWG_WIFI_BSSID", getMacAddress("wlan0")),
        SecurityMode::WPA2_PERSONAL,
        AccessPointType::DYNAMIC,
        getenv("AAWG_PROXY_IP_ADDRESS", "10.0.0.1"),
        getenv("AAWG_PROXY_PORT", 5288),
    };
}

ConnectionStrategy Config::getConnectionStrategy() {
    if (!connectionStrategy.has_value()) {
        const int32_t connectionStrategyEnv = getenv("AAWG_CONNECTION_STRATEGY", 1);

        switch (connectionStrategyEnv) {
            case 0:
                connectionStrategy = ConnectionStrategy::DONGLE_MODE;
                break;
            case 1:
                connectionStrategy = ConnectionStrategy::PHONE_FIRST;
                break;
            case 2:
                connectionStrategy = ConnectionStrategy::USB_FIRST;
                break;
            default:
                connectionStrategy = ConnectionStrategy::PHONE_FIRST;
                break;
        }
    }

    return connectionStrategy.value();
}

int32_t Config::getUsbGadgetSwitchDelayMs() {
    int32_t delay = getenv("AAWG_GADGET_SWITCH_DELAY_MS", 100);
    if (delay < 0) {
        delay = 100;
    }
    return delay;
}

std::string Config::getPreferredDevice() {
    return getenv("AAWG_PREFERRED_DEVICE", std::string(""));
}

std::string Config::getStatusLed() {
    return getenv("AAWG_STATUS_LED", std::string(""));
}
#pragma endregion Config

#pragma region Logger
static constexpr const char* PERSIST_LOG_PATH = "/persist/aawgd.log";
static constexpr const char* PERSIST_LOG_PATH_OLD = "/persist/aawgd.log.1";
static constexpr long PERSIST_LOG_MAX_BYTES = 256 * 1024;

/*static*/ Logger* Logger::instance() {
    static Logger s_instance;
    return &s_instance;
}

Logger::Logger() {
    openlog(nullptr, LOG_PERROR | LOG_PID, LOG_USER);

    const char* persistEnv = std::getenv("AAWG_PERSIST_LOG");
    if (persistEnv != nullptr && std::string(persistEnv) != "0" && std::string(persistEnv) != "") {
        m_persistLog = fopen(PERSIST_LOG_PATH, "a");
        if (m_persistLog == nullptr) {
            syslog(LOG_WARNING, "Could not open persistent log %s: %s\n", PERSIST_LOG_PATH, strerror(errno));
        }
    }
}

Logger::~Logger() {
    if (m_persistLog != nullptr) {
        fclose(m_persistLog);
        m_persistLog = nullptr;
    }
    closelog();
}

void Logger::info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsyslog(LOG_INFO, format, args);
    va_end(args);

    if (m_persistLog != nullptr) {
        va_list fileArgs;
        va_start(fileArgs, format);
        writeToPersistLog(format, fileArgs);
        va_end(fileArgs);
    }
}

void Logger::writeToPersistLog(const char *format, va_list args) {
    time_t now = time(nullptr);
    struct tm tmValue;
    char timestamp[32];
    if (localtime_r(&now, &tmValue) != nullptr &&
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tmValue) > 0) {
        fprintf(m_persistLog, "[%s] ", timestamp);
    }

    vfprintf(m_persistLog, format, args);
    fflush(m_persistLog);

    if (ftell(m_persistLog) >= PERSIST_LOG_MAX_BYTES) {
        rotatePersistLog();
    }
}

void Logger::rotatePersistLog() {
    fclose(m_persistLog);
    m_persistLog = nullptr;

    // Keep a single previous generation; cap on-disk usage at 2x the max.
    rename(PERSIST_LOG_PATH, PERSIST_LOG_PATH_OLD);

    m_persistLog = fopen(PERSIST_LOG_PATH, "w");
}
#pragma endregion Logger