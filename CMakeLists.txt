cmake_minimum_required(VERSION 3.16)

project(smoker_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Curses REQUIRED)

add_executable(smoker_sim
    src/main.cpp
    src/app.cpp
    src/ui.cpp
)

target_include_directories(smoker_sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(smoker_sim PRIVATE ${CURSES_LIBRARIES})
target_include_directories(smoker_sim PRIVATE ${CURSES_INCLUDE_DIR})