cmake_minimum_required(VERSION 3.16)
project(jeson_package_smoke LANGUAGES CXX)

find_package(jeson 0.1 CONFIG REQUIRED)
add_executable(jeson_package_smoke ../smoke.cpp)
target_link_libraries(jeson_package_smoke PRIVATE jeson::jeson)

