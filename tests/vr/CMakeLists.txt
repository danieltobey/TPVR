# Host unit tests for the pure VR maths (spec 10 item 6). A standalone
# project, independent of the game and Android builds:
#   cmake -S tests/vr -B build/host-tests -G Ninja && cmake --build build/host-tests
#   build/host-tests/vr_math_tests
# (tools/test.sh in the dev environment does this.)
cmake_minimum_required(VERSION 3.25)
project(tpvr_vr_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_executable(vr_math_tests vr_math_tests.cpp)
target_include_directories(vr_math_tests PRIVATE ${REPO_ROOT}/src ${REPO_ROOT}/extern)
target_compile_options(vr_math_tests PRIVATE -Wall -Wextra -Werror)
