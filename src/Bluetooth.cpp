
#include <string>

#include "Bluetooth.h"
#include "BlablaCallbacks.h"
#include "Storage.h"

#include "data.h"

#include "esp32-hal-log.h"
#include "cJSON.h"

#define MSG_START_CHAR '$'
#define MSG_END_CHAR '\n'

#define SERVICE_UUID "0000abcd-6e32-4f94-adf6-b96ebda4c6ce"

#define SET_DATA_CHR_UUID                       "1000b0ea-6e32-4f94-adf6-b96ebda4c6ce"
#define GET_STATIONS_CHR_UUID                   "1001b0ea-6e32-4f94-adf6-b96ebda4c6ce"
#define GET_EVENTS_CHR_UUID                     "1002b0ea-6e32-4f94-adf6-b96ebda4c6ce"
#define NOTIFY_STATION_STATUS_CHR_UUID          "1003b0ea-6e32-4f94-adf6-b96ebda4c6ce"

static std::vector<std::string> s_characteristics = {
    SET_DATA_CHR_UUID,
    GET_STATIONS_CHR_UUID,
    GET_EVENTS_CHR_UUID,
    NOTIFY_STATION_STATUS_CHR_UUID
};

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

static void advertisingComplete(NimBLEAdvertising *pAdv)
{
    auto numConnectedDevices = NimBLEDevice::getServer()->getConnectedCount();
    log_i("Advertising complete, connected to %d devices", numConnectedDevices);
}

Bluetooth::Bluetooth(const std::string& name, Storage* storage) :
    m_pStorage(storage),
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
    m_pServer->setCallbacks(this);
    setupCharacteristic();

    log_i("Updating stations and events");
    setStations();
    setEvents();
}

void Bluetooth::setBluetoothCallbacks(BlablaCallbacks* callbacks)
{
    m_pCallback = callbacks;
}

void Bluetooth::setStations()
{
    auto getStationsChr = getCharacteristicByUUIDs(SERVICE_UUID, GET_STATIONS_CHR_UUID);
    if (getStationsChr != nullptr)
    {   
        std::vector<Station> stationVec = to_vector(m_pStorage->getStations());
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

void Bluetooth::setEvents()
{
    auto getEventsChr = getCharacteristicByUUIDs(SERVICE_UUID, GET_EVENTS_CHR_UUID);
    if (getEventsChr != nullptr)
    {   
        std::vector<Event> eventsVec = to_vector(m_pStorage->getEvents());
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

void Bluetooth::notifyStationStates() const
{
    auto stationStatesChr = getCharacteristicByUUIDs(SERVICE_UUID, NOTIFY_STATION_STATUS_CHR_UUID);
    if (stationStatesChr != nullptr)
    {
        cJSON* array = cJSON_CreateArray();
        for (const auto& station : m_pStorage->getStations())
        {            
            cJSON* object = cJSON_CreateObject();
            cJSON_AddNumberToObject(object, "id", station.second.id);
            cJSON_AddBoolToObject(object, "is_on", station.second.is_on);
            cJSON_AddItemToArray(array, object);
        }
        auto json_cstr = cJSON_PrintUnformatted(array);
        std::string statesJsonStr(json_cstr);
        log_i("Notifying Stations states JSON:%s", statesJsonStr.c_str());
        stationStatesChr->notify(statesJsonStr);
        cJSON_Delete(array);
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
    NimBLECharacteristic *setDataChr = pService->createCharacteristic(SET_DATA_CHR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);

    NimBLECharacteristic *notifyStationChangedChr = pService->createCharacteristic(NOTIFY_STATION_STATUS_CHR_UUID, NIMBLE_PROPERTY::NOTIFY);
    
    NimBLECharacteristic *getEventsChr = pService->createCharacteristic(GET_EVENTS_CHR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN);        


    for (auto& chr : s_characteristics)
    {
        m_characteristicsBuffers[chr] = std::vector<char>();    
    }

    getStationsChr->setCallbacks(this);
    setDataChr->setCallbacks(this);
    notifyStationChangedChr->setCallbacks(this);
    getEventsChr->setCallbacks(this);
    pService->start();
}

void Bluetooth::start()
{
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start(0, &advertisingComplete);
}

void Bluetooth::onConnect(NimBLEServer* pServer)
{
    log_d("Device connected. stopping advertising");
    NimBLEDevice::getAdvertising()->stop();

    log_d("Resetting write buffers");
    for (auto& chr : s_characteristics)
    {
        m_characteristicsBuffers[chr] = std::vector<char>();    
    }
    log_d("Resetting read buffers");
    for (auto& value : m_characteristicValues)
    {
        value.second.second = 0;
    }
}

void Bluetooth::onDisconnect(NimBLEServer* pServer)
{    
    log_d("Device disconnected.");
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
    if (value_str.empty())
    {
        value_str = "$\n";
    }
    log_i("%s: value: %s (%s)", characteristicStr.c_str(), pCharacteristic->getValue().c_str(), value_str.c_str());

    auto& buffer = m_characteristicsBuffers[characteristicStr];
    if (value_str.at(0) == MSG_START_CHAR)
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
        log_i("Found end of message. Parsing");

        parseCharacteristicWrite(buffer);
    }
}

void Bluetooth::onNotify(NimBLECharacteristic* pCharacteristic) {
    log_i("%s", pCharacteristic->getUUID().toString().c_str());
    log_i(": onNotify(), value: %s", pCharacteristic->getValue().c_str());
}

void Bluetooth::parseCharacteristicWrite(const std::vector<char>& buffer) const
{
    cJSON *json = cJSON_Parse(&buffer[0]);
    if (json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            log_e("cJSON Error: %s", error_ptr);
        }
        else
        {
            log_e("cJSON - Unknown parsing error");
        }
        return;
    }
    auto messageTypeInt = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (!cJSON_IsNumber(messageTypeInt) || messageTypeInt->valueint >= MessageType::MAX)
    {
        log_e("Unknown message type received");
        return;
    }
    if (!cJSON_HasObjectItem(json, "data"))
    {
        log_e("Message doesn't have data object");
        return;
    }
    auto dataJsonStr = cJSON_GetObjectItem(json, "data");
    auto dataJson = cJSON_Parse(dataJsonStr->valuestring);
    if (dataJson == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            log_e("data cJSON Error: %s", error_ptr);
        }
        else
        {
            log_e("data cJSON - Unknown parsing error");
        }
        return;
    }
    auto messageType = static_cast<MessageType>(messageTypeInt->valueint);
    switch(messageType)
    {
        case SET_TIME:
            parseSetTime(dataJson);
        break;
        case SET_STATION_STATE:
            parseSetStationState(dataJson);
        break;
        case REQUEST_NOTIFY:
            notifyStationStates();
        break;

        default:
        break;
    }
    cJSON_free(json);
}

void Bluetooth::parseSetTime(const cJSON* json) const
{    
    if (m_pCallback)
    {
        SetTimeMessage message;        
        auto tv_sec = cJSON_GetObjectItemCaseSensitive(json, "tv_sec");
        auto tv_usec = cJSON_GetObjectItemCaseSensitive(json, "tv_usec");
        auto tz_str = cJSON_GetObjectItemCaseSensitive(json, "tz_str");
        message.timeval.tv_sec = cJSON_IsNumber(tv_sec) ? tv_sec->valueint : 0;
        message.timeval.tv_usec = cJSON_IsNumber(tv_usec) ? tv_usec->valueint : 0;
        message.tz = cJSON_IsString(tz_str) ? tz_str->valuestring : "";
        m_pCallback->onMessageReceived(MessageType::SET_TIME, (void*)&message);
    }
}

void Bluetooth::parseSetStationState(const cJSON* json) const
{
    if (m_pCallback)
    {
        SetStationStateMessage message;
        auto id = cJSON_GetObjectItemCaseSensitive(json, "station_id");
        auto state = cJSON_GetObjectItemCaseSensitive(json, "is_on");
        message.station_id = cJSON_IsNumber(id) ? id->valueint : -1;
        message.is_on = cJSON_IsBool(state) ? cJSON_IsTrue(state) : false;
        m_pCallback->onMessageReceived(MessageType::SET_STATION_STATE, (void*)&message);
    }
}
