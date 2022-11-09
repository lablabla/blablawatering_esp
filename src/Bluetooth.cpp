
#include "Bluetooth.h"
#include "BlablaCallbacks.h"
#include "data.h"

#include "esp32-hal-log.h"
#include "cJSON.h"

#define MSG_END_CHAR '\n'

#define SERVICE_UUID "ABCD"

#define SET_STATION_CHR_UUID                    "1000"
#define GET_STATIONS_CHR_UUID                   "1001"
#define GET_EVENTS_CHR_UUID                     "1002"
#define NOTIFY_STATION_STATUS_CHANGED_CHR_UUID  "1003"
#define SET_EVENT_CHR_UUID                      "1004"
#define SET_TIME_CHR_UUID                       "1005"



static std::map<std::string, MessageType> chrToMessage = {
    { std::string("0x") + std::string(SET_TIME_CHR_UUID), MessageType::SET_TIME}
};
static std::map<MessageType, std::vector<char>> characteristicsBuffers;

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
    
	cJSON* stations_array = cJSON_AddArrayToObject(object, "station_ids");
    for (auto id : event.stations_ids)
    {
        cJSON* number = cJSON_CreateNumber(id);
        cJSON_AddItemToArray(stations_array, number);        
    }    
    cJSON_AddStringToObject(object, "name", event.name.c_str());
    cJSON_AddStringToObject(object, "cron_expr", event.cron_expr.c_str());
    cJSON_AddNumberToObject(object, "duration", event.duration);

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
    m_pCallback(nullptr),
    m_characteristicValues()
{
    
    NimBLEDevice::init(name);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
    NimBLEDevice::setMTU(BLE_ATT_MTU_MAX);

    NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND);
    // TODO: Replace hardcoded with random and show on LCD display
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
        auto json = stationsToJson(stationVec);
        auto json_cstr = cJSON_PrintUnformatted(json);
        std::string stationsJsonStr(json_cstr);
        log_i("Stations JSON:\n%s", stationsJsonStr.c_str());

        if (stationsJsonStr[stationsJsonStr.size()-1] != '\n')
        {
            log_i("Adding newline char to value");
            stationsJsonStr += "\n";
        }
        m_characteristicValues[getStationsChr] = std::make_pair(stationsJsonStr, 0);
        cJSON_Delete(json);
        cJSON_free(json_cstr);
    }
}

void Bluetooth::setEvents(const std::map<uint32_t, Event>& events)
{
    auto getEventsChr = getCharacteristicByUUIDs(SERVICE_UUID, GET_EVENTS_CHR_UUID);
    if (getEventsChr != nullptr)
    {   
        std::vector<Event> eventsVec = to_vector(events);
        auto json = eventsToJson(eventsVec);
        auto json_cstr = cJSON_PrintUnformatted(json);
        std::string eventsJsonStr(json_cstr);
        log_i("Events JSON:%s", eventsJsonStr.c_str());
        
        if (eventsJsonStr[eventsJsonStr.size()-1] != '\n')
        {
            log_i("Adding newline char to value");
            eventsJsonStr += "\n";
        }
        m_characteristicValues[getEventsChr] = std::make_pair(eventsJsonStr, 0);
        cJSON_Delete(json);
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
        log_i("Notifying Station JSON:%s", stationJsonStr.c_str());
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
        log_i("%s", errorMsg.c_str());
        return nullptr;
    }
    auto pCharacteristic = pService->getCharacteristic(characteristicUuid);
    if (pCharacteristic == nullptr)
    {
        std::string errorMsg = "No characteristic with UUID " + std::string(characteristicUuid);
        log_i("%s", errorMsg.c_str());
        return nullptr;
    }

    return pCharacteristic;
}

void Bluetooth::setCharacteristicValue(NimBLECharacteristic* pCharacteristic, const std::string& value)
{
    log_i("Setting value of %s to %s", pCharacteristic->toString().c_str(), value.c_str());
    auto max_size = std::min((std::size_t)NimBLEDevice::getMTU(), value.length());
    log_i("Max size is %d, value length is %d", max_size, value.length());

    std::string substr = value.substr(0, max_size);
    int& bytes_sent = m_characteristicValues[pCharacteristic].second;
    bytes_sent += substr.length();
    log_i("Setting value to substring %s", substr.c_str());
    pCharacteristic->setValue(substr);

    const std::string& full_value = m_characteristicValues[pCharacteristic].first;
    if (bytes_sent == full_value.length())
    {
        log_i("Full value sent. Resetting to 0");
        bytes_sent = 0;
    }
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
    
    for (const auto& characteristics : chrToMessage)
    {
        characteristicsBuffers[characteristics.second] = std::vector<char>();
    }

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
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
}

void Bluetooth::onRead(NimBLECharacteristic* pCharacteristic) {
    auto value = pCharacteristic->getValue();
    auto total_length = m_characteristicValues[pCharacteristic].first.length();
    auto last_read_pos = m_characteristicValues[pCharacteristic].second;
    log_i("Total length: %d, last read pos: %d", total_length, last_read_pos);
    int remaining_length = total_length - last_read_pos;
    if (remaining_length > 0)
    {
        std::string new_value = m_characteristicValues[pCharacteristic].first.substr(last_read_pos);
        setCharacteristicValue(pCharacteristic, new_value);
    }
    log_i("Sending value %s", pCharacteristic->getValue().c_str());
}

void Bluetooth::onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string characteristicStr = pCharacteristic->getUUID().toString();
    std::string value_str(pCharacteristic->getValue().c_str());
    log_i("%s: value: %s (%s)", characteristicStr.c_str(), pCharacteristic->getValue().c_str(), value_str.c_str());


    if (chrToMessage.find(characteristicStr) == chrToMessage.end())
    {
        log_i("Unknown characteristic %s", characteristicStr.c_str());
        return;
    }
    MessageType messageType = chrToMessage.at(characteristicStr);
    auto& buffer = characteristicsBuffers[messageType];
    if (value_str.at(0) == '$')
    {
        value_str = value_str.substr(1);
        log_i("Found start of message. Resetting buffer. Using incoming message %s", value_str.c_str());
        buffer = std::vector<char>();
    }

    log_i("Buffer length is %d", buffer.size());
    buffer.insert(buffer.end(), value_str.begin(), value_str.end());
    std::string buffer_string(buffer.begin(), buffer.end());
    log_i("Added %d chars to buffer. New buffer length is %d (%s)", value_str.length(), buffer.size(), buffer_string.c_str());
    if (buffer[buffer.size()-1] == MSG_END_CHAR)
    {
        log_i("characteristic %s, message is %d", characteristicStr.c_str(), messageType);

        parseCharacteristicWrite(buffer, messageType);
    }
}

void Bluetooth::onNotify(NimBLECharacteristic* pCharacteristic) {
    log_i("%s", pCharacteristic->getUUID().toString().c_str());
    log_i(": onNotify(), value: %s", pCharacteristic->getValue().c_str());
}

void Bluetooth::parseCharacteristicWrite(const std::vector<char>& buffer, MessageType messageType) const
{
    switch(messageType)
    {
        case SET_TIME:
            parseSetTime(buffer);
        break;

        default:
        break;
    }
}

void Bluetooth::parseSetTime(const std::vector<char>& buffer) const
{    
    if (m_pCallback)
    {
        cJSON *json = cJSON_Parse(&buffer[0]);
        if (json == NULL)
        {
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL)
            {
                log_e("Error before: %s", error_ptr);
            }
        }
        SetTimeMessage message;        
        auto tv_sec = cJSON_GetObjectItemCaseSensitive(json, "tv_sec");
        auto tv_usec = cJSON_GetObjectItemCaseSensitive(json, "tv_usec");
        auto tz_str = cJSON_GetObjectItemCaseSensitive(json, "tz_str");
        message.timeval.tv_sec = cJSON_IsNumber(tv_sec) ? tv_sec->valueint : 0;
        message.timeval.tv_usec = cJSON_IsNumber(tv_usec) ? tv_usec->valueint : 0;
        message.tz = cJSON_IsString(tz_str) ? tz_str->valuestring : "";
        cJSON_free(json);
        m_pCallback->onMessageReceived(MessageType::SET_TIME, (void*)&message);
    }
}
