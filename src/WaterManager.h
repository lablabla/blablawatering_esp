#pragma once

#include <mutex>
#include <map>

#include "data.h"

#include "BlablaCallbacks.h"

    class Bluetooth;
    class Storage;
    class DS1307;
    class CronManager;
    class LiquidCrystal_I2C;

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

        void setStationState(Station& station, bool newState);

        void updateTimeFromRTC();
        void printTimeFromRTC() const;

        void setTimeMessage(const SetTimeMessage& timeMessage) const;
        void setStationStateMessage(const SetStationStateMessage& stationStateMessage);

        std::mutex m_mutex;
        void* m_backgroundTaskHandle;
        Bluetooth* m_bluetooth;
        Storage* m_storage;
        DS1307* m_rtc;
        CronManager* m_cronManager;
        LiquidCrystal_I2C* m_lcd;
    };
