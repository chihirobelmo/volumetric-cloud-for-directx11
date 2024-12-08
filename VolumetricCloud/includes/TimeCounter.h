#pragma once

#include <chrono>

class TimeCounter {
public:
    TimeCounter() : startTime(), endTime(), running(false) {}

    void Start() {
        startTime = std::chrono::high_resolution_clock::now();
        running = true;
    }

    void Stop() {
        endTime = std::chrono::high_resolution_clock::now();
        running = false;
    }

    template <typename T>
    double GetElapsedTime() const {
        std::chrono::time_point<std::chrono::high_resolution_clock> endTimePoint;

        if (running) {
            endTimePoint = std::chrono::high_resolution_clock::now();
        }
        else {
            endTimePoint = endTime;
        }

        return std::chrono::duration<double, T>(endTimePoint - startTime).count();
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> endTime;
    bool running;
};