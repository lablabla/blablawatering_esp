
#include "Bluetooth.h"
#include "data.h"

#include "esp_log.h"
#include "cJSON.h"

#define SERVICE_UUID "ABCD"

#define SET_STATION_CHR_UUID                    "1000"
#define GET_STATIONS_CHR_UUID                   "1001"
#define GET_EVENTS_CHR_UUID                     "1002"
#define NOTIFY_STATION_STATUS_CHANGED_CHR_UUID  "1003"
#define SET_EVENT_CHR_UUID                      "1004"
#define SET_TIME_CHR_UUID                       "1005"

static const char* TAG = "Bluetooth";

// JSON Helpers
static cJSON* stationToJson(const Station& station)
{
    cJSON* object = cJSON_CreateObject();
    cJSON_AddNumberToObject(object, "id", station.id);
    cJSON_AddNumberToObject(object, "gpio_pin", station.gpio_pin);
    cJSON_AddStringToObject(object, "name", station.name.c_str());
    cJSON_AddBoolToObject(object, "is_on", station.is_on);

    return object;
}

static cJSON* stationsToJson(const std::vector<Station>& stations)
{
	cJSON* root = cJSON_CreateArray();
    for (const auto& s : stations)
    {
        cJSON* object = stationToJson(s);
        cJSON_AddItemToArray(root, object);
    }
    return root;
}

static cJSON* eventToJson(const Event& event)
{
    cJSON* object = cJSON_CreateObject();
    cJSON_AddNumberToObject(object, "id", event.id);
    cJSON_AddNumberToObject(object, "station_id", event.station_id);
    cJSON_AddStringToObject(object, "name", event.name.c_str());
    cJSON_AddNumberToObject(object, "start_time", event.start_time);
    cJSON_AddNumberToObject(object, "duration", event.duration);
    cJSON_AddBoolToObject(object, "is_on", event.is_on);

    return object;
}

static cJSON* eventsToJson(const std::vector<Event>& events)
{
	cJSON* root = cJSON_CreateArray();
    for (const auto& e : events)
    {
        cJSON* object = eventToJson(e);
        cJSON_AddItemToArray(root, object);
    }
    return root;
}

Bluetooth::Bluetooth(const std::string& name) :
    m_pCallback(nullptr)
{
    
    NimBLEDevice::init(name);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
    NimBLEDevice::setMTU(200);

    NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND);
    NimBLEDevice::setSecurityPasskey(123456);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO);
    m_pServer = NimBLEDevice::createServer();

    setupCharacteristic();
}

void Bluetooth::setBluetoothCallbacks(BlablaCallbacks* callbacks)
{
    m_pCallback = callbacks;
}

void Bluetooth::setStations(const std::map<uint32_t, Station>& stations)
{
    auto getStationsChr = getCharacteristicByUUIDs(SERVICE_UUID, GET_STATIONS_CHR_UUID);
    if (getStationsChr != nullptr)
    {   
        std::vector<Station> stationVec = to_vector(stations);
        auto json_cstr = cJSON_PrintUnformatted(stationsToJson(stationVec));
        std::string stationsJsonStr(json_cstr);
        ESP_LOGI(TAG, "Stations JSON:\n%s\n", stationsJsonStr.c_str());
        getStationsChr->setValue(stationsJsonStr);
        cJSON_free(json_cstr);
    }
}

void Bluetooth::setEvents(const std::map<uint32_t, Event>& events)
{
    auto getStationsChr = getCharacteristicByUUIDs(SERVICE_UUID, GET_EVENTS_CHR_UUID);
    if (getStationsChr != nullptr)
    {   
        std::vector<Event> eventsVec = to_vector(events);
        auto json_cstr = cJSON_PrintUnformatted(eventsToJson(eventsVec));
        std::string eventsJsonStr(json_cstr);
        ESP_LOGI(TAG, "Events JSON:\n%s\n", eventsJsonStr.c_str());
        getStationsChr->setValue(eventsJsonStr);
        cJSON_free(json_cstr);
    }
}

void Bluetooth::notifyStationStateChanged(const Station& station) const
{
    auto stationChangedChr = getCharacteristicByUUIDs(SERVICE_UUID, NOTIFY_STATION_STATUS_CHANGED_CHR_UUID);
    if (stationChangedChr != nullptr)
    {
        auto json_cstr = cJSON_PrintUnformatted(stationToJson(station));
        std::string stationJsonStr(json_cstr);
        ESP_LOGI(TAG, "Notifying Station JSON:\n%s\n", stationJsonStr.c_str());
        stationChangedChr->notify(stationJsonStr);
        cJSON_free(json_cstr);
    }
}

NimBLECharacteristic* Bluetooth::getCharacteristicByUUIDs(const char* serviceUuid, const char* characteristicUuid) const
{
    auto pService = m_pServer->getServiceByUUID(serviceUuid);
    if (pService == nullptr)
    {
        std::string errorMsg = "No service with UUID " + std::string(serviceUuid);
        ESP_LOGI(TAG, "%s\n", errorMsg.c_str());
        return nullptr;
    }
    auto pCharacteristic = pService->getCharacteristic(characteristicUuid);
    if (pCharacteristic == nullptr)
    {
        std::string errorMsg = "No characteristic with UUID " + std::string(characteristicUuid);
        ESP_LOGI(TAG, "%s\n", errorMsg.c_str());
        return nullptr;
    }

    return pCharacteristic;
}

void Bluetooth::setupCharacteristic()
{        
    NimBLEService *pService = m_pServer->createService(SERVICE_UUID);
    NimBLECharacteristic *getStationsChr = pService->createCharacteristic(GET_STATIONS_CHR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN);
    NimBLECharacteristic *setStationChr = pService->createCharacteristic(SET_STATION_CHR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);

    NimBLECharacteristic *notifyStationChangedChr = pService->createCharacteristic(NOTIFY_STATION_STATUS_CHANGED_CHR_UUID, NIMBLE_PROPERTY::NOTIFY);
    
    NimBLECharacteristic *getEventsChr = pService->createCharacteristic(GET_EVENTS_CHR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN);        
    NimBLECharacteristic *setEventChr = pService->createCharacteristic(SET_EVENT_CHR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);

    NimBLECharacteristic *setTimeChr = pService->createCharacteristic(SET_TIME_CHR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);
    

    getStationsChr->setCallbacks(this);
    setStationChr->setCallbacks(this);
    notifyStationChangedChr->setCallbacks(this);
    setEventChr->setCallbacks(this);
    getEventsChr->setCallbacks(this);
    setTimeChr->setCallbacks(this);
    pService->start();
}

void Bluetooth::start()
{
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID("ABCD");
    pAdvertising->start();
}

void Bluetooth::onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo){
    ESP_LOGI(TAG, "%s", pCharacteristic->getUUID().toString().c_str());
    ESP_LOGI(TAG, ": onRead(), value: \n%s\n",pCharacteristic->getValue().c_str());

    if (m_pCallback)
    {
        m_pCallback->onMessageReceived(&pCharacteristic->getValue());
    }
};

void Bluetooth::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    ESP_LOGI(TAG, "%s", pCharacteristic->getUUID().toString().c_str());
    ESP_LOGI(TAG, ": onWrite(), value: \n%s\n", pCharacteristic->getValue().c_str());

    if (m_pCallback)
    {
        m_pCallback->onMessageReceived(&pCharacteristic->getValue());
    }
};

void Bluetooth::onNotify(NimBLECharacteristic* pCharacteristic) {
    ESP_LOGI(TAG, "%s", pCharacteristic->getUUID().toString().c_str());
    ESP_LOGI(TAG, ": onNotify(), value: \n%s\n", pCharacteristic->getValue().c_str());
}
