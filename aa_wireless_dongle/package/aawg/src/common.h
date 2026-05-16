#pragma once

#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <optional>

enum SecurityMode: int;
enum AccessPointType: int;

struct WifiInfo {
    std::string ssid;
    std::string key;
    std::string bssid;
    SecurityMode securityMode;
    AccessPointType accessPointType;
    std::string ipAddress;
    int32_t port;
};

enum class ConnectionStrategy {
    DONGLE_MODE = 0,
    PHONE_FIRST = 1,
    USB_FIRST = 2
};

class Config {
public:
    static Config* instance();

    WifiInfo getWifiInfo();
    ConnectionStrategy getConnectionStrategy();

    std::string getUniqueSuffix();

    // Delay (ms) between disabling the default gadget and enabling the
    // accessory gadget. Some head units (e.g. Mazda Connect) need a longer
    // settle time than the 100ms default to re-enumerate reliably.
    int32_t getUsbGadgetSwitchDelayMs();

    // Preferred bluetooth device MAC address (empty if unset). When set, this
    // device is tried first on connection for a faster cold-start.
    std::string getPreferredDevice();

    // Name of the sysfs LED to use as a status indicator (empty if unset).
    std::string getStatusLed();
private:
    Config() = default;

    int32_t getenv(std::string name, int32_t defaultValue);
    std::string getenv(std::string name, std::string defaultValue);

    std::string getMacAddress(std::string interface);

    std::optional<ConnectionStrategy> connectionStrategy;
};

class Logger {
public:
    static Logger* instance();

    void info(const char *format, ...);
private:
    Logger();
    ~Logger();

    void writeToPersistLog(const char *format, va_list args);
    void rotatePersistLog();

    // Optional log file on the writable /persist partition. syslog is
    // volatile, so this survives a power-cycle and lets intermittent
    // failures be diagnosed after the fact. Enabled via AAWG_PERSIST_LOG.
    FILE *m_persistLog = nullptr;
};