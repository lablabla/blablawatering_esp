#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>


constexpr int8_t NO_DURATION = -1;
constexpr int8_t START_IMMEDIATELY = -1;

struct Station
{
    uint8_t id;
    uint8_t gpio_pin;
    std::string name;
    bool is_on;
};

struct Event
{
    uint8_t id;
    uint8_t station_id;
    std::string name;
    int64_t start_time;
    int64_t duration;
    bool is_on;
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


