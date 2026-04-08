#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

enum class SmokerState {
    Start,
    WaitTamper,
    Tamping,
    WaitMatches,
    Lighting,
    Smoking,
    Finished
};

struct SmokerInfo {
    SmokerState state = SmokerState::Start;
    int cyclesCompleted = 0;
};

constexpr int SMOKER_COUNT = 15;
constexpr int TAMPER_COUNT = 2;
constexpr int MATCHBOX_COUNT = 2;
constexpr int MAX_CYCLES = 15;
constexpr int SIMULATION_SPEED_MULTIPLIER = 1;

constexpr auto TAMPING_TIME = std::chrono::milliseconds(700);
constexpr auto LIGHTING_TIME = std::chrono::milliseconds(500);
constexpr auto SMOKING_TIME = std::chrono::milliseconds(1200);
constexpr auto STAGE_PAUSE = std::chrono::milliseconds(400);
constexpr auto UI_REFRESH_TIME = std::chrono::milliseconds(50);

template <typename Duration>
constexpr auto scaled(Duration d) {
    return d * SIMULATION_SPEED_MULTIPLIER;
}

class Ui;

std::string toString(SmokerState state);
int colorForState(SmokerState state);
void initColors();

class FairResource {
public:
    FairResource(std::string resourceName, int count, Ui& uiRef, bool tamperResource);

    void acquire(int smokerId, SmokerState waitingState, SmokerState workingState);
    void release(int smokerId);
    void notifyAll();

private:
    void updateUiCountersUnsafe();

    std::string name;
    int available;
    std::queue<int> waitingQueue;
    std::mutex mtx;
    std::condition_variable cv;
    Ui& ui;
    bool isTamper;
};

void smokerWorker(
    int smokerId,
    FairResource& tampers,
    FairResource& matches,
    Ui& ui,
    std::atomic<bool>& stopFlag,
    int maxCycles
);