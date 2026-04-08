#include "app.hpp"
#include "ui.hpp"

#include <ncurses.h>
#include <thread>
#include <utility>

std::string toString(SmokerState state) {
    switch (state) {
        case SmokerState::Start: return "START";
        case SmokerState::WaitTamper: return "WAIT_TAMPER";
        case SmokerState::Tamping: return "TAMPING";
        case SmokerState::WaitMatches: return "WAIT_MATCHES";
        case SmokerState::Lighting: return "LIGHTING";
        case SmokerState::Smoking: return "SMOKING";
        case SmokerState::Finished: return "FINISHED";
    }
    return "UNKNOWN";
}

int colorForState(SmokerState state) {
    switch (state) {
        case SmokerState::Start: return 1;
        case SmokerState::WaitTamper: return 2;
        case SmokerState::Tamping: return 3;
        case SmokerState::WaitMatches: return 2;
        case SmokerState::Lighting: return 4;
        case SmokerState::Smoking: return 5;
        case SmokerState::Finished: return 6;
    }
    return 1;
}

void initColors() {
    if (!has_colors()) {
        return;
    }

    start_color();
    use_default_colors();

    init_pair(1, COLOR_WHITE, -1);
    init_pair(2, COLOR_YELLOW, -1);
    init_pair(3, COLOR_CYAN, -1);
    init_pair(4, COLOR_MAGENTA, -1);
    init_pair(5, COLOR_GREEN, -1);
    init_pair(6, COLOR_BLUE, -1);
}

FairResource::FairResource(std::string resourceName, int count, Ui& uiRef, bool tamperResource)
    : name(std::move(resourceName)),
      available(count),
      ui(uiRef),
      isTamper(tamperResource) {}

void FairResource::acquire(int smokerId, SmokerState waitingState, SmokerState workingState) {
    ui.setState(smokerId, waitingState);
    ui.log("Smoker " + std::to_string(smokerId) + " is waiting for " + name);

    std::unique_lock<std::mutex> lock(mtx);
    waitingQueue.push(smokerId);
    updateUiCountersUnsafe();

    cv.wait(lock, [&]() {
        return !waitingQueue.empty() && waitingQueue.front() == smokerId && available > 0;
    });

    waitingQueue.pop();
    --available;
    updateUiCountersUnsafe();

    lock.unlock();

    ui.setState(smokerId, workingState);
    ui.log("Smoker " + std::to_string(smokerId) + " acquired " + name);
}

void FairResource::release(int smokerId) {
    std::unique_lock<std::mutex> lock(mtx);
    ++available;
    updateUiCountersUnsafe();
    lock.unlock();

    ui.log("Smoker " + std::to_string(smokerId) + " released " + name);
    cv.notify_all();
}

void FairResource::notifyAll() {
    cv.notify_all();
}

void FairResource::updateUiCountersUnsafe() {
    if (isTamper) {
        ui.setFreeTampers(available);
        ui.setTamperQueueSize(static_cast<int>(waitingQueue.size()));
    } else {
        ui.setFreeMatches(available);
        ui.setMatchesQueueSize(static_cast<int>(waitingQueue.size()));
    }
}

void smokerWorker(
    int smokerId,
    FairResource& tampers,
    FairResource& matches,
    Ui& ui,
    std::atomic<bool>& stopFlag,
    int maxCycles
) {
    for (int cycle = 0; cycle < maxCycles && !stopFlag.load(); ++cycle) {
        ui.log("Smoker " + std::to_string(smokerId) + " starts cycle " + std::to_string(cycle + 1));

        tampers.acquire(smokerId, SmokerState::WaitTamper, SmokerState::Tamping);
        std::this_thread::sleep_for(scaled(STAGE_PAUSE));
        std::this_thread::sleep_for(scaled(TAMPING_TIME));
        tampers.release(smokerId);

        if (stopFlag.load()) {
            break;
        }

        ui.setState(smokerId, SmokerState::WaitMatches);
        std::this_thread::sleep_for(scaled(STAGE_PAUSE));

        matches.acquire(smokerId, SmokerState::WaitMatches, SmokerState::Lighting);
        std::this_thread::sleep_for(scaled(STAGE_PAUSE));
        std::this_thread::sleep_for(scaled(LIGHTING_TIME));
        matches.release(smokerId);

        if (stopFlag.load()) {
            break;
        }

        ui.setState(smokerId, SmokerState::Smoking);
        ui.log("Smoker " + std::to_string(smokerId) + " is smoking");
        std::this_thread::sleep_for(scaled(STAGE_PAUSE));
        std::this_thread::sleep_for(scaled(SMOKING_TIME));

        ui.incrementCycles(smokerId);
    }

    ui.setState(smokerId, SmokerState::Finished);
    ui.markFinished();
    ui.log("Smoker " + std::to_string(smokerId) + " finished work");
}