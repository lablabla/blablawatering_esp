#pragma once

#include <mutex>
#include <map>

#include "data.h"

#include "BlablaCallbacks.h"

    class Bluetooth;
    class Storage;

    class WaterManager : public BlablaCallbacks
    {
    public:
        WaterManager();
        ~WaterManager();

        void run();

        // Callbacks
        void onMessageReceived(void* message) override;
    private:
        static void runTask(void* taskStartParameters);
        void vTaskCode();

        std::mutex m_mutex;
        void* m_backgroundTaskHandle;
        Bluetooth* m_bluetooth;
        Storage* m_storage;
    };
