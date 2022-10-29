#pragma once

#include <mutex>
#include <map>

#include "data.h"

#include "BlablaCallbacks.h"

    class Bluetooth;
    class Storage;
    class DS1307;
    class CronManager;

    class WaterManager : public BlablaCallbacks
    {
    public:
        WaterManager();
        ~WaterManager();

        void loop();

        // Callbacks
        void onMessageReceived(MessageType messageType, void* message) override;
        void onEventStateChange(const Event& event, bool newState) override;
    private:

        void updateTimeFromRTC();
        void printTimeFromRTC() const;

        void setTimeMessage(const SetTimeMessage& timeMessage) const;

        std::mutex m_mutex;
        void* m_backgroundTaskHandle;
        Bluetooth* m_bluetooth;
        Storage* m_storage;
        DS1307* m_rtc;
        CronManager* m_cronManager;
    };
