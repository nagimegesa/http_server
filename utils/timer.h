#pragma once

#include <ctime>

class Timer {
    std::clock_t alarm_time;
public:
    bool operator<(const Timer& rhs) const {
        return alarm_time < rhs.alarm_time;
    }

    bool operator>(const Timer& rhs) const {
        return alarm_time > rhs.alarm_time;
    }

    Timer() : alarm_time(0) {
    }

    Timer(std::clock_t t) : alarm_time(t) {
    }

    std::clock_t get_time() const {
        return alarm_time;
    }

    void set_timer(std::clock_t t) {
        alarm_time = t;
    }
};