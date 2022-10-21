#pragma once

#include <mutex>
#include <map>

#include "data.h"

#include "BlablaCallbacks.h"

    class Bluetooth;
    class Storage;
    class DS1307;

    class WaterManager : public BlablaCallbacks
    {
    public:
        WaterManager();
        ~WaterManager();

        void run();

        // Callbacks
        void onMessageReceived(MessageType messageType, void* message) override;
    private:
        static void runTask(void* taskStartParameters);
        void vTaskCode();

        void updateTimeFromRTC();
        void printTimeFromRTC() const;

        void setTimeMessage(const SetTimeMessage& timeMessage) const;

        void checkEvents();

        std::mutex m_mutex;
        void* m_backgroundTaskHandle;
        Bluetooth* m_bluetooth;
        Storage* m_storage;
        DS1307* m_rtc;
    };
