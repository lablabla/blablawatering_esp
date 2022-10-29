#pragma once

#include <string>
#include <map>

#include <NimBLEDevice.h>

#include "data.h"

class BlablaCallbacks;

class Bluetooth : public NimBLECharacteristicCallbacks
{
public:
    Bluetooth(const std::string& name);

    void start();
    void setBluetoothCallbacks(BlablaCallbacks* callbacks);
    void setStations(const std::map<uint32_t, Station>& stations);
    void setEvents(const std::map<uint32_t, Event>& events);
    void notifyStationStateChanged(const Station& station) const;

private:

    void onRead(NimBLECharacteristic* pCharacteristic) override;
    void onWrite(NimBLECharacteristic* pCharacteristic) override;
    void onNotify(NimBLECharacteristic* pCharacteristic) override;

    void setupCharacteristic();

    NimBLECharacteristic* getCharacteristicByUUIDs(const char* serviceUuid, const char* characteristicUuid) const;

    void parseCharacteristicWrite(NimBLECharacteristic* pCharacteristic, MessageType messageType) const;
    void parseSetTime(NimBLECharacteristic* pCharacteristic) const;

    NimBLEServer* m_pServer;
    BlablaCallbacks* m_pCallback;
};
