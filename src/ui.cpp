#include "ui.hpp"

#include <algorithm>
#include <ncurses.h>

Ui::Ui(const AppConfig& config)
    : config(config),
      smokers(config.smokerCount),
      freeTampers(config.tamperCount),
      freeMatches(config.matchboxCount) {}

void Ui::setState(int smokerId, SmokerState state) {
    std::lock_guard<std::mutex> lock(mtx);
    smokers.at(smokerId - 1).state = state;
}

void Ui::incrementCycles(int smokerId) {
    std::lock_guard<std::mutex> lock(mtx);
    smokers.at(smokerId - 1).cyclesCompleted++;
}

void Ui::setFreeTampers(int value) {
    std::lock_guard<std::mutex> lock(mtx);
    freeTampers = value;
}

void Ui::setFreeMatches(int value) {
    std::lock_guard<std::mutex> lock(mtx);
    freeMatches = value;
}

void Ui::setTamperQueueSize(int value) {
    std::lock_guard<std::mutex> lock(mtx);
    tamperQueueSize = value;
}

void Ui::setMatchesQueueSize(int value) {
    std::lock_guard<std::mutex> lock(mtx);
    matchesQueueSize = value;
}

void Ui::markFinished() {
    std::lock_guard<std::mutex> lock(mtx);
    finishedCount++;
}

int Ui::getFinishedCount() const {
    std::lock_guard<std::mutex> lock(mtx);
    return finishedCount;
}

void Ui::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(mtx);
    logs.push_back(message);

    while (static_cast<int>(logs.size()) > 300) {
        logs.pop_front();
    }
}

void Ui::draw() const {
    std::lock_guard<std::mutex> lock(mtx);

    erase();

    int rows = 0;
    int cols = 0;
    getmaxyx(stdscr, rows, cols);

    mvprintw(1, 2, "q = quit");
    mvprintw(
        2,
        2,
        "Smokers: %d | Tampers: %d | Matches: %d | Time scale: x%d",
        config.smokerCount,
        config.tamperCount,
        config.matchboxCount,
        SIMULATION_SPEED_MULTIPLIER
    );

    mvprintw(
        3,
        2,
        "Free tampers: %d | Free matches: %d | Tamper queue: %d | Match queue: %d | Finished: %d/%d",
        freeTampers,
        freeMatches,
        tamperQueueSize,
        matchesQueueSize,
        finishedCount,
        config.smokerCount
    );

    mvhline(4, 0, '=', cols);

    attron(A_BOLD);
    mvprintw(5, 2, "%-6s %-18s %-8s", "ID", "STATE", "CYCLES");
    attroff(A_BOLD);

    int line = 6;
    int tableEnd = rows - 12;
    if (tableEnd < 7) {
        tableEnd = 7;
    }

    for (std::size_t i = 0; i < smokers.size() && line < tableEnd; ++i, ++line) {
        int color = colorForState(smokers[i].state);
        attron(COLOR_PAIR(color));
        mvprintw(
            line,
            2,
            "%-6zu %-18s %-8d",
            i + 1,
            toString(smokers[i].state).c_str(),
            smokers[i].cyclesCompleted
        );
        attroff(COLOR_PAIR(color));
    }

    int logStart = line + 1;
    if (logStart < rows - 2) {
        mvhline(logStart, 0, '=', cols);
        mvprintw(logStart + 1, 2, "Event log:");

        int visibleLogs = rows - (logStart + 3);
        int startIndex = std::max(0, static_cast<int>(logs.size()) - visibleLogs);

        int logLine = logStart + 2;
        for (int i = startIndex; i < static_cast<int>(logs.size()) && logLine < rows - 1; ++i, ++logLine) {
            std::string msg = logs[i];
            if (static_cast<int>(msg.size()) > cols - 4) {
                msg = msg.substr(0, cols - 7) + "...";
            }
            mvprintw(logLine, 2, "%s", msg.c_str());
        }
    }

    refresh();
}