#include <ncurses.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono_literals;

// =====================
// Konfiguracja symulacji
// =====================

constexpr int SMOKER_COUNT = 15;
constexpr int TAMPER_COUNT = 2;
constexpr int MATCHBOX_COUNT = 2;
constexpr int MAX_CYCLES = 15;

// 1 = normalnie
// 2 = 2x wolniej
// 3 = 3x wolniej
constexpr int SIMULATION_SPEED_MULTIPLIER = 1;

constexpr auto TAMPING_TIME = 700ms;
constexpr auto LIGHTING_TIME = 500ms;
constexpr auto SMOKING_TIME = 1200ms;
constexpr auto STAGE_PAUSE = 400ms;
constexpr auto UI_REFRESH_TIME = 50ms;

template <typename Duration>
auto scaled(Duration d) {
    return d * SIMULATION_SPEED_MULTIPLIER;
}

// =====================
// Stany palacza
// =====================

enum class SmokerState {
    START,
    WAIT_TAMPER,
    TAMPING,
    WAIT_MATCHES,
    LIGHTING,
    SMOKING,
    FINISHED
};

string toString(SmokerState state) {
    switch (state) {
        case SmokerState::START: return "START";
        case SmokerState::WAIT_TAMPER: return "CZEKA_UBIJACZ";
        case SmokerState::TAMPING: return "UBIJA";
        case SmokerState::WAIT_MATCHES: return "CZEKA_ZAPALKI";
        case SmokerState::LIGHTING: return "ZAPALA";
        case SmokerState::SMOKING: return "PALI";
        case SmokerState::FINISHED: return "KONIEC";
    }
    return "NIEZNANY";
}

int colorForState(SmokerState state) {
    switch (state) {
        case SmokerState::START: return 1;
        case SmokerState::WAIT_TAMPER: return 2;
        case SmokerState::TAMPING: return 3;
        case SmokerState::WAIT_MATCHES: return 2;
        case SmokerState::LIGHTING: return 4;
        case SmokerState::SMOKING: return 5;
        case SmokerState::FINISHED: return 6;
    }
    return 1;
}

// =====================
// Dane jednego palacza
// =====================

struct SmokerInfo {
    SmokerState state = SmokerState::START;
    int cyclesCompleted = 0;
};

// =====================
// UI ncurses
// =====================

class Ui {
private:
    mutable mutex mtx;
    vector<SmokerInfo> smokers;
    deque<string> logs;

    int freeTampers = 0;
    int freeMatches = 0;
    int tamperQueueSize = 0;
    int matchesQueueSize = 0;
    int finishedCount = 0;

public:
    Ui(int smokerCount, int tampers, int matches)
        : smokers(smokerCount), freeTampers(tampers), freeMatches(matches) {}

    void setState(int smokerId, SmokerState state) {
        lock_guard<mutex> lock(mtx);
        smokers.at(smokerId - 1).state = state;
    }

    void incrementCycles(int smokerId) {
        lock_guard<mutex> lock(mtx);
        smokers.at(smokerId - 1).cyclesCompleted++;
    }

    void setFreeTampers(int value) {
        lock_guard<mutex> lock(mtx);
        freeTampers = value;
    }

    void setFreeMatches(int value) {
        lock_guard<mutex> lock(mtx);
        freeMatches = value;
    }

    void setTamperQueueSize(int value) {
        lock_guard<mutex> lock(mtx);
        tamperQueueSize = value;
    }

    void setMatchesQueueSize(int value) {
        lock_guard<mutex> lock(mtx);
        matchesQueueSize = value;
    }

    void markFinished() {
        lock_guard<mutex> lock(mtx);
        finishedCount++;
    }

    int getFinishedCount() const {
        lock_guard<mutex> lock(mtx);
        return finishedCount;
    }

    void log(const string& message) {
        lock_guard<mutex> lock(mtx);
        logs.push_back(message);

        while (static_cast<int>(logs.size()) > 300) {
            logs.pop_front();
        }
    }

    void draw() {
        lock_guard<mutex> lock(mtx);

        erase();

        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        mvprintw(1, 2, "q = wyjscie");
        mvprintw(
            2,
            2,
            "Palacze: %d | Ubijacze: %d | Zapalki: %d | Skala czasu: x%d",
            SMOKER_COUNT,
            TAMPER_COUNT,
            MATCHBOX_COUNT,
            SIMULATION_SPEED_MULTIPLIER
        );

        mvprintw(
            3,
            2,
            "Wolne ubijacze: %d | Wolne zapalki: %d | Kolejka ubijaczy: %d | Kolejka zapalek: %d | Zakonczono: %d/%d",
            freeTampers,
            freeMatches,
            tamperQueueSize,
            matchesQueueSize,
            finishedCount,
            SMOKER_COUNT
        );

        mvhline(4, 0, '=', cols);

        attron(A_BOLD);
        mvprintw(5, 2, "%-6s %-18s %-8s", "ID", "STAN", "CYKLE");
        attroff(A_BOLD);

        int line = 6;
        int tableEnd = rows - 12;
        if (tableEnd < 7) {
            tableEnd = 7;
        }

        for (size_t i = 0; i < smokers.size() && line < tableEnd; ++i, ++line) {
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
            mvprintw(logStart + 1, 2, "Log zdarzen:");

            int visibleLogs = rows - (logStart + 3);
            int startIndex = max(0, static_cast<int>(logs.size()) - visibleLogs);

            int logLine = logStart + 2;
            for (int i = startIndex; i < static_cast<int>(logs.size()) && logLine < rows - 1; ++i, ++logLine) {
                string msg = logs[i];
                if (static_cast<int>(msg.size()) > cols - 4) {
                    msg = msg.substr(0, cols - 7) + "...";
                }
                mvprintw(logLine, 2, "%s", msg.c_str());
            }
        }

        refresh();
    }
};

// =====================
// Zasób z FIFO
// =====================

class FairResource {
private:
    string name;
    int available;
    queue<int> waitingQueue;
    mutex mtx;
    condition_variable cv;
    Ui& ui;
    bool isTamper;

public:
    FairResource(string resourceName, int count, Ui& uiRef, bool tamperResource)
        : name(std::move(resourceName)),
          available(count),
          ui(uiRef),
          isTamper(tamperResource) {}

    void acquire(int smokerId, SmokerState waitingState, SmokerState workingState) {
        ui.setState(smokerId, waitingState);
        ui.log("Palacz " + to_string(smokerId) + " czeka na " + name);

        unique_lock<mutex> lock(mtx);
        waitingQueue.push(smokerId);
        updateUiCountersUnsafe();

        cv.wait(lock, [&]() {
            return !waitingQueue.empty()
                && waitingQueue.front() == smokerId
                && available > 0;
        });

        waitingQueue.pop();
        --available;
        updateUiCountersUnsafe();

        lock.unlock();

        ui.setState(smokerId, workingState);
        ui.log("Palacz " + to_string(smokerId) + " dostal " + name);
    }

    void release(int smokerId) {
        unique_lock<mutex> lock(mtx);
        ++available;
        updateUiCountersUnsafe();
        lock.unlock();

        ui.log("Palacz " + to_string(smokerId) + " oddal " + name);
        cv.notify_all();
    }

    void notifyAll() {
        cv.notify_all();
    }

private:
    void updateUiCountersUnsafe() {
        if (isTamper) {
            ui.setFreeTampers(available);
            ui.setTamperQueueSize(static_cast<int>(waitingQueue.size()));
        } else {
            ui.setFreeMatches(available);
            ui.setMatchesQueueSize(static_cast<int>(waitingQueue.size()));
        }
    }
};

// =====================
// Wątek palacza
// =====================

void smokerWorker(
    int smokerId,
    FairResource& tampers,
    FairResource& matches,
    Ui& ui,
    atomic<bool>& stopFlag,
    int maxCycles
) {
    for (int cycle = 0; cycle < maxCycles && !stopFlag.load(); ++cycle) {
        ui.log("Palacz " + to_string(smokerId) + " rozpoczyna cykl " + to_string(cycle + 1));

        tampers.acquire(smokerId, SmokerState::WAIT_TAMPER, SmokerState::TAMPING);
        this_thread::sleep_for(scaled(STAGE_PAUSE));
        this_thread::sleep_for(scaled(TAMPING_TIME));
        tampers.release(smokerId);

        if (stopFlag.load()) {
            break;
        }

        this_thread::sleep_for(scaled(STAGE_PAUSE));

        matches.acquire(smokerId, SmokerState::WAIT_MATCHES, SmokerState::LIGHTING);
        this_thread::sleep_for(scaled(STAGE_PAUSE));
        this_thread::sleep_for(scaled(LIGHTING_TIME));
        matches.release(smokerId);

        if (stopFlag.load()) {
            break;
        }

        this_thread::sleep_for(scaled(STAGE_PAUSE));

        ui.setState(smokerId, SmokerState::SMOKING);
        ui.log("Palacz " + to_string(smokerId) + " pali fajke");
        this_thread::sleep_for(scaled(STAGE_PAUSE));
        this_thread::sleep_for(scaled(SMOKING_TIME));

        ui.incrementCycles(smokerId);
    }

    ui.setState(smokerId, SmokerState::FINISHED);
    ui.markFinished();
    ui.log("Palacz " + to_string(smokerId) + " zakonczyl prace");
}

// =====================
// Kolory ncurses
// =====================

void initColors() {
    if (!has_colors()) {
        return;
    }

    start_color();
    use_default_colors();

    init_pair(1, COLOR_WHITE,   -1); // START
    init_pair(2, COLOR_YELLOW,  -1); // CZEKA
    init_pair(3, COLOR_CYAN,    -1); // UBIJA
    init_pair(4, COLOR_MAGENTA, -1); // ZAPALA
    init_pair(5, COLOR_GREEN,   -1); // PALI
    init_pair(6, COLOR_BLUE,    -1); // KONIEC
}

// =====================
// main
// =====================

int main() {
    Ui ui(SMOKER_COUNT, TAMPER_COUNT, MATCHBOX_COUNT);
    FairResource tampers("ubijacz", TAMPER_COUNT, ui, true);
    FairResource matches("zapalki", MATCHBOX_COUNT, ui, false);

    atomic<bool> stopFlag{false};
    vector<thread> threads;

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
            ref(tampers),
            ref(matches),
            ref(ui),
            ref(stopFlag),
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

        if (ch == KEY_RESIZE) {
            
        }

        ui.draw();

        if (ui.getFinishedCount() == SMOKER_COUNT) {
            break;
        }

        this_thread::sleep_for(UI_REFRESH_TIME);
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
    mvprintw(LINES - 1, 2, "Koniec programu. Nacisnij dowolny klawisz...");
    nodelay(stdscr, FALSE);
    getch();
    endwin();

    return 0;
}