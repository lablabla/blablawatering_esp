#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <cstdint>
#include <string>
#include <sstream>
#include <string.h>
#include <vector>
#include <map>
#include <sys/time.h>

enum MessageType
{
    SET_TIME,
    MAX,
};

struct SetTimeMessage
{
    struct timeval timeval;
    std::string tz;
};

struct Station
{
    Station() = default;
    ~Station() = default;
    Station(uint8_t id_, uint8_t gpio_pin_, const std::string& name_, bool is_on_) :
        id(id_), gpio_pin(gpio_pin_), name(name_), is_on(is_on_)
    {}

    uint8_t id;
    uint8_t gpio_pin;
    std::string name;
    bool is_on;
};

struct Event
{
    Event() = default;
    ~Event() = default;
    Event(uint8_t id_, const std::vector<uint8_t>& stations_ids_, const std::string& name_, const std::string& cron_expr_, int32_t duration_) :
        id(id_), stations_ids(stations_ids_), name(name_), cron_expr(cron_expr_), duration(duration_)
    {}

    uint8_t id;
    std::vector<uint8_t> stations_ids;
    std::string name;
    std::string cron_expr;
    int32_t duration;
};

struct StationData
{
    StationData(const Station& station) :
        id(station.id), gpio_pin(station.gpio_pin), name_length(station.name.length())
    {
        name = new char[name_length+1];
        memset(name, 0, name_length+1);
        memcpy(name, station.name.c_str(), name_length);
    }
    
    ~StationData()
    {
        delete[] name;
    }

    uint8_t id;
    uint8_t gpio_pin;
    uint8_t name_length;
    char* name;    
};

struct EventData
{
    EventData(const Event& event) :
        id(event.id), num_stations(event.stations_ids.size()), name_length(event.name.length()), days(0)
    {       
        name = new char[name_length+1];
        memset(name, 0, name_length+1);
        memcpy(name, event.name.c_str(), name_length);

        stations = new uint8_t[num_stations];
        memset(stations, 0, num_stations);
        memcpy(stations, &event.stations_ids[0], num_stations);

        int minute, hour;
        char days_cstr[16] = { 0 };
        sscanf(event.cron_expr.c_str(), "* %d %d * * %s", &minute, &hour, days_cstr);
        std::string days_str(days_cstr);
        std::stringstream ss(days_str);
        while( ss.good() )
        {
            std::string substr;
            getline( ss, substr, ',' );
            size_t pos;
            int day = atoi(substr.c_str());
            days |= (1 << (day-1));
        }
    }

    ~EventData()
    {
        delete[] stations;
        delete[] name;
    }

    uint16_t length() const
    {
        return  sizeof(id) + \
                sizeof(num_stations) + \
                num_stations * sizeof(uint8_t) + \
                sizeof(name_length) + \
                name_length * sizeof(char) + \
                sizeof(start_hour) + \
                sizeof(start_minute) + \
                sizeof(end_hour) + \
                sizeof(end_minute) + \
                sizeof(days);
    }


    uint8_t id;
    uint8_t num_stations;
    uint8_t name_length;
    uint8_t start_hour;
    uint8_t start_minute;
    uint8_t end_hour;
    uint8_t end_minute;
    uint8_t days;
    uint8_t* stations;
    char* name;
};

template <class T>
static std::map<uint32_t, T> to_map(const std::vector<T>& vec)
{
    std::map<uint32_t, T> m;
    for (const auto& e : vec)
    {
        m[e.id] = e;
    }
    return m;
}

template <class T>
static std::vector<T> to_vector(const std::map<uint32_t, T>& map)
{
    std::vector<T> vec;
    for (const auto& e : map)
    {
        vec.push_back(e.second);
    }
    return vec;
}


