
#include <vector>
#include <iostream>

#include "Storage.h"

#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"


// Mount path for the partition
const char *BASE_PATH = "/spiflash";
const char* STATIONS_FILE = "/spiflash/stations.bin";
const char* EVENTS_FILE = "/spiflash/events.bin";
static const char* TAG = "Storage";


static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

Storage::Storage()
{
    ESP_LOGI(TAG, "Mounting FAT filesystem");

    esp_vfs_fat_mount_config_t mount_config;
    mount_config.max_files = 4;
    mount_config.format_if_mount_failed = true;
    mount_config.allocation_unit_size = CONFIG_WL_SECTOR_SIZE;
    esp_err_t err = esp_vfs_fat_spiflash_mount(BASE_PATH, "storage", &mount_config, &s_wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }

    loadStations();
    if (m_stations.empty())
    {
        Station s1 = {0, 13, "Test station 1", false};
        Station s2 = {1, 19, "station 2", false};
        addStation(s1);
        addStation(s2);
    }

    ESP_LOGI(TAG, "Calling loadEvents()\n");
    loadEvents();
    if (m_events.empty())
    {
        Event e1 = {0, 0, "Meow1", START_IMMEDIATELY, NO_DURATION, true};
        addEvent(e1);
    }
}

Storage::~Storage()
{    
    // Unmount FATFS
    ESP_LOGI(TAG, "Unmounting FAT filesystem");
    ESP_ERROR_CHECK( esp_vfs_fat_spiflash_unmount(BASE_PATH, s_wl_handle));
}

bool Storage::loadStations()
{
    FILE* f = fopen(STATIONS_FILE, "rb");
    if (f == nullptr)
    {
        ESP_LOGI(TAG, "Failed opening stations db");
        return false;
    }
    uint32_t numOfStations;
    if (fread(&numOfStations, sizeof(uint32_t), 1, f) == 0)
    {
        ESP_LOGI(TAG, "Failed reading number of stations");
        return false;
    }

    std::vector<Station> vec(numOfStations);
    for (int i = 0; i < numOfStations; i++)
    {
        auto& s = vec[i];
        size_t size = 0;
        fread(&s.id, sizeof(uint8_t), 1, f);
        fread(&s.gpio_pin, sizeof(uint8_t), 1, f);
        fread(&size, sizeof(size_t), 1, f);
        char* name = new char[size + 1];
        memset(name, 0, size + 1);
        fread(name, sizeof(char), size, f);
        fread(&s.is_on, sizeof(bool), 1, f);
        s.name = std::string(name);
        delete[] name;
    }
    for(int i = 0; i < numOfStations; i++)
    {
        ESP_LOGI(TAG, "Station #%d id: %d, name: %s", (i+1), vec[i].id, vec[i].name.c_str());
    }
    fclose(f);
    m_stations = to_map(vec);
    return true;
}

bool Storage::loadEvents()
{
    FILE* f = fopen(EVENTS_FILE, "rb");
    if (f == nullptr)
    {
        ESP_LOGI(TAG, "Failed opening events db");
        return false;
    }
    uint32_t numOfEvents;
    if (fread(&numOfEvents, sizeof(uint32_t), 1, f) == 0)
    {
        ESP_LOGI(TAG, "Failed reading number of events");
        return false;
    }

    std::vector<Event> vec(numOfEvents);
    for (int i = 0; i < numOfEvents; i++)
    {
        auto& e = vec[i];
        size_t size = 0;
        fread(&e.id, sizeof(uint8_t), 1, f);
        fread(&e.station_id, sizeof(uint8_t), 1, f);
        fread(&size, sizeof(size_t), 1, f);
        char* name = new char[size + 1];
        memset(name, 0, size + 1);
        fread(name, sizeof(char), size, f);
        fread(&e.start_time, sizeof(int64_t), 1, f);      
        fread(&e.duration, sizeof(int64_t), 1, f);      
        fread(&e.is_on, sizeof(bool), 1, f);
        e.name = std::string(name);
        delete[] name;
    }
    for(int i = 0; i < numOfEvents; i++)
    {
        ESP_LOGI(TAG, "Event #%d id: %d, name: %s", (i+1), vec[i].id, vec[i].name.c_str());
    }
    fclose(f);
    m_events = to_map(vec);
    return true;
}

bool Storage::clear()
{
    return true;
}

bool Storage::addStation(const Station& station)
{
    if (m_stations.find(station.id) != m_stations.end())
    {
        ESP_LOGI(TAG, "Station already exists. Not overriding\n");
        return false;
    }
    ESP_LOGI(TAG, "Adding station %d", station.id);
    m_stations.insert({station.id, station});

    return setStations(m_stations);
}

bool Storage::removeStation(uint32_t id)
{
    if (m_stations.find(id) == m_stations.end())
    {
        ESP_LOGI(TAG, "Station not found\n");
        return false;
    }
    m_stations.erase(id);

    return setStations(m_stations);
}

bool Storage::clearStations()
{
    FILE* f = fopen(STATIONS_FILE, "wb");
    if (f == nullptr)
    {
        ESP_LOGI(TAG, "Failed opening stations db");
        return false;
    }
    fclose(f);
    return true;
}

bool Storage::setStations(const std::map<uint32_t, Station>& stations)
{
    FILE* f = fopen(STATIONS_FILE, "wb");
    if (f == nullptr)
    {
        ESP_LOGI(TAG, "Failed opening stations db");
        return false;
    }
    uint32_t numOfStations = stations.size();
    if (fwrite(&numOfStations, sizeof(uint32_t), 1, f) == 0)
    {
        ESP_LOGI(TAG, "Failed writing number of stations");
        return false;
    }
    for (const auto& station : stations)
    {
        const auto& s = station.second;
        const auto name = s.name.c_str();
        size_t size = s.name.length();

        fwrite(&s.id, sizeof(uint8_t), 1, f);
        fwrite(&s.gpio_pin, sizeof(uint8_t), 1, f);
        fwrite(&size, sizeof(size_t), 1, f);
        fwrite(name, sizeof(char), size, f);
        fwrite(&s.is_on, sizeof(bool), 1, f);        
    }
    fclose(f);
    ESP_LOGI(TAG, "Wrote %d stations to db", numOfStations);
    m_stations = stations;
    return true;
}

bool Storage::getStation(uint32_t id, Station& station) const
{
    if (m_stations.find(id) == m_stations.end())
    {
        ESP_LOGI(TAG, "Station not found\n");
        return false;
    }
    station = m_stations.at(id);
    return true;
}

bool Storage::addEvent(const Event& event)
{
    if (m_events.find(event.id) != m_events.end())
    {
        ESP_LOGI(TAG, "Event already exists. Not overriding\n");
        return false;
    }
    m_events.insert({event.id, event});

    ESP_LOGI(TAG, "Adding event %d", event.id);
    return setEvents(m_events);
}

bool Storage::removeEvent(uint32_t id)
{
    if (m_events.find(id) == m_events.end())
    {
        ESP_LOGI(TAG, "Event not found\n");
        return false;
    }
    m_events.erase(id);

    return setStations(m_stations);
}

bool Storage::clearEvents()
{
    FILE* f = fopen(EVENTS_FILE, "wb");
    if (f == nullptr)
    {
        ESP_LOGI(TAG, "Failed opening events db");
        return false;
    }
    fclose(f);
    return true;
}

bool Storage::setEvents(const std::map<uint32_t, Event>& events)
{
    FILE* f = fopen(EVENTS_FILE, "wb");
    if (f == nullptr)
    {
        ESP_LOGI(TAG, "Failed opening events db");
        return false;
    }
    uint32_t numOfEvents = events.size();
    if (fwrite(&numOfEvents, sizeof(uint32_t), 1, f) == 0)
    {
        ESP_LOGI(TAG, "Failed writing number of events");
        return false;
    }
    for (const auto& event : events)
    {
        const auto& e = event.second;
        const auto name = e.name.c_str();
        size_t size = e.name.length();
        
        fwrite(&e.id, sizeof(uint8_t), 1, f);
        fwrite(&e.station_id, sizeof(uint8_t), 1, f);
        fwrite(&size, sizeof(size_t), 1, f);
        fwrite(name, sizeof(char), e.name.length(), f);
        fwrite(&e.start_time, sizeof(int64_t), 1, f);      
        fwrite(&e.duration, sizeof(int64_t), 1, f);      
        fwrite(&e.is_on, sizeof(bool), 1, f);
    }
    fclose(f);
    ESP_LOGI(TAG, "Wrote %d events to db", numOfEvents);
    m_events = events;
    return true;
}

bool Storage::getEvent(uint32_t id, Event& event) const
{
    if (m_events.find(id) == m_events.end())
    {
        ESP_LOGI(TAG, "Event not found\n");
        return false;
    }
    event = m_events.at(id);
    return true;
}

