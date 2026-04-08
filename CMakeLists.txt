cmake_minimum_required(VERSION 3.20)
project(SmokersNcurses LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

add_executable(smokers
    src/main.cpp
)

target_include_directories(smokers PRIVATE ${CURSES_INCLUDE_DIRS})
target_link_libraries(smokers PRIVATE ${CURSES_LIBRARIES} Threads::Threads)

target_compile_options(smokers PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)