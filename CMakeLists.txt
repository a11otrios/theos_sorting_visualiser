cmake_minimum_required(VERSION 3.20)

project(sorting_visualizer LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The C++ application uses the Win32 API and must be built on Windows.")
endif()

add_executable(sorting_visualizer WIN32 sorting4.cpp)
target_compile_features(sorting_visualizer PRIVATE cxx_std_17)
target_link_libraries(sorting_visualizer PRIVATE winmm)

if(MSVC)
    target_compile_options(sorting_visualizer PRIVATE /W4 /permissive-)
else()
    target_compile_options(sorting_visualizer PRIVATE -Wall -Wextra -Wpedantic)
endif()
