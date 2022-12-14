#pragma once

#include <string>
#include <map>

#include <NimBLEDevice.h>

#include "data.h"

class BlablaCallbacks;
class Storage;
class cJSON;

class Bluetooth : public NimBLECharacteristicCallbacks, public NimBLEServerCallbacks
{
public:
    Bluetooth(const std::string& name, Storage* storage);

    void start();
    void setBluetoothCallbacks(BlablaCallbacks* callbacks);
    void setStations();
    void setEvents();
    void notifyStationStates() const;

private:

    // Server callbacks
    void onConnect(NimBLEServer* pServer) override;
    void onDisconnect(NimBLEServer* pServer) override;

    // Characteristics callbacks
    void onRead(NimBLECharacteristic* pCharacteristic) override;
    void onWrite(NimBLECharacteristic* pCharacteristic) override;
    void onNotify(NimBLECharacteristic* pCharacteristic) override;

    void setCharacteristicValue(NimBLECharacteristic* pCharacteristic, const std::string& value);

    void setupCharacteristic();

    NimBLECharacteristic* getCharacteristicByUUIDs(const char* serviceUuid, const char* characteristicUuid) const;

    void parseCharacteristicWrite(const std::vector<char>& buffer) const;
    void parseSetTime(const cJSON* json) const;
    void parseSetStationState(const cJSON* json) const;

    NimBLEServer* m_pServer;
    Storage* m_pStorage;

    BlablaCallbacks* m_pCallback;
    std::map<NimBLECharacteristic*, std::pair<std::string, int>> m_characteristicValues;
    std::map<std::string, std::vector<char>> m_characteristicsBuffers;
};
