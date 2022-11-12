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
    GET_STATIONS,
    SET_STATION_STATE,
    REQUEST_NOTIFY,
    MAX,
};

struct SetTimeMessage
{
    struct timeval timeval;
    std::string tz;
};

struct SetStationStateMessage
{
    int station_id;
    bool is_on;
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


