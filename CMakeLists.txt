cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

project(timer-test)

add_compile_options(-std=c++11)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}, Platform: ${CMAKE_SYSTEM_NAME}")

add_executable(${PROJECT_NAME} test.cpp)
