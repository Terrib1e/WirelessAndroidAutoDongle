#include <stdio.h>
#include <unistd.h>

#include "common.h"
#include "bluetoothHandler.h"
#include "proxyHandler.h"
#include "uevent.h"
#include "usb.h"
#include "statusled.h"

int main(void) {
    Logger::instance()->info("AA Wireless Dongle\n");

    // Global init
    std::optional<std::thread> ueventThread = UeventMonitor::instance().start();
    if (!ueventThread) {
        // Accessory detection depends on uevents; without them the daemon
        // would block forever. Fail fast and let the supervisor restart us.
        Logger::instance()->info("Failed to start uevent monitoring, aborting\n");
        return 1;
    }
    UsbManager::instance().init();
    BluetoothHandler::instance().init();

    ConnectionStrategy connectionStrategy = Config::instance()->getConnectionStrategy();
    if (connectionStrategy == ConnectionStrategy::DONGLE_MODE) {
        BluetoothHandler::instance().powerOn();
    }

    while (true) {
        Logger::instance()->info("Connection Strategy: %d\n", connectionStrategy);
        StatusLed::instance().set(LedState::Waiting);

        // Per connection setup and processing
        if (connectionStrategy == ConnectionStrategy::USB_FIRST) {
            Logger::instance()->info("Waiting for the accessory to connect first\n");
            UsbManager::instance().enableDefaultAndWaitForAccessory();
        }

        AAWProxy proxy;
        std::optional<std::thread> proxyThread = proxy.startServer(Config::instance()->getWifiInfo().port);

        if (!proxyThread) {
            StatusLed::instance().set(LedState::Error);
            return 1;
        }

        if (connectionStrategy != ConnectionStrategy::DONGLE_MODE) {
            BluetoothHandler::instance().powerOn();
        }

        std::optional<std::thread> btConnectionThread = BluetoothHandler::instance().connectWithRetry();

        proxyThread->join();

        if (btConnectionThread) {
            BluetoothHandler::instance().stopConnectWithRetry();
            btConnectionThread->join();
        }

        UsbManager::instance().disableGadget();

        if (connectionStrategy != ConnectionStrategy::DONGLE_MODE) {
            // sleep for a couple of seconds before retrying
            sleep(2);
        }
    }

    return 0;
}
