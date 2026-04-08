#include "app.hpp"
#include "ui.hpp"

#include <atomic>
#include <iostream>
#include <limits>
#include <ncurses.h>
#include <thread>
#include <vector>

int readPositiveInt(const std::string& label) {
    int value = 0;

    while (true) {
        std::cout << label;
        std::cin >> value;

        if (std::cin.fail() || value <= 0) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Please enter a positive integer.\n";
            continue;
        }

        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return value;
    }
}

int main() {
    AppConfig config;
    config.smokerCount = readPositiveInt("Enter number of smokers: ");
    config.tamperCount = readPositiveInt("Enter number of tampers: ");
    config.matchboxCount = readPositiveInt("Enter number of matches resources: ");
    config.maxCycles = readPositiveInt("Enter number of cycles: ");

    Ui ui(config);
    FairResource tampers("tamper", config.tamperCount, ui, true);
    FairResource matches("matches", config.matchboxCount, ui, false);

    std::atomic<bool> stopFlag{false};
    std::vector<std::thread> threads;

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    initColors();

    for (int i = 1; i <= config.smokerCount; ++i) {
        threads.emplace_back(
            smokerWorker,
            i,
            std::ref(tampers),
            std::ref(matches),
            std::ref(ui),
            std::ref(stopFlag),
            config.maxCycles
        );
    }

    while (true) {
        int ch = getch();

        if (ch == 'q' || ch == 'Q') {
            stopFlag.store(true);
            tampers.notifyAll();
            matches.notifyAll();
            break;
        }

        ui.draw();

        if (ui.getFinishedCount() == config.smokerCount) {
            break;
        }

        std::this_thread::sleep_for(UI_REFRESH_TIME);
    }

    stopFlag.store(true);
    tampers.notifyAll();
    matches.notifyAll();

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    ui.draw();
    mvprintw(LINES - 1, 2, "Program finished. Press any key...");
    nodelay(stdscr, FALSE);
    getch();
    endwin();

    return 0;
}