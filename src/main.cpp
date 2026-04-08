#include "app.hpp"
#include "ui.hpp"

#include <ncurses.h>
#include <atomic>
#include <thread>
#include <vector>

int main() {
    Ui ui(SMOKER_COUNT, TAMPER_COUNT, MATCHBOX_COUNT);
    FairResource tampers("tamper", TAMPER_COUNT, ui, true);
    FairResource matches("matches", MATCHBOX_COUNT, ui, false);

    std::atomic<bool> stopFlag{false};
    std::vector<std::thread> threads;

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    initColors();

    for (int i = 1; i <= SMOKER_COUNT; ++i) {
        threads.emplace_back(
            smokerWorker,
            i,
            std::ref(tampers),
            std::ref(matches),
            std::ref(ui),
            std::ref(stopFlag),
            MAX_CYCLES
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

        if (ui.getFinishedCount() == SMOKER_COUNT) {
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