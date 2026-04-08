#pragma once

#include "app.hpp"

#include <deque>
#include <mutex>
#include <string>
#include <vector>

class Ui {
public:
    Ui(const AppConfig& config);

    void setState(int smokerId, SmokerState state);
    void incrementCycles(int smokerId);
    void setFreeTampers(int value);
    void setFreeMatches(int value);
    void setTamperQueueSize(int value);
    void setMatchesQueueSize(int value);
    void markFinished();
    int getFinishedCount() const;
    void log(const std::string& message);
    void draw() const;

private:
    mutable std::mutex mtx;
    AppConfig config;
    std::vector<SmokerInfo> smokers;
    std::deque<std::string> logs;
    int freeTampers = 0;
    int freeMatches = 0;
    int tamperQueueSize = 0;
    int matchesQueueSize = 0;
    int finishedCount = 0;
};