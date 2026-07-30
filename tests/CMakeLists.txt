cmake_minimum_required(VERSION 3.16)

if(NOT BUILD_TESTS)
    return()
endif()

find_package(GTest REQUIRED)

enable_testing()

set(TEST_SOURCES
    unit/test_speed_limiter.cpp
    unit/test_queue_manager.cpp
    unit/test_category_engine.cpp
    unit/test_scheduler.cpp
    unit/test_database.cpp
    unit/test_checksum.cpp
    unit/test_segment_planner.cpp
    unit/test_download_engine.cpp
)

add_executable(remo_tests ${TEST_SOURCES})

target_include_directories(remo_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(remo_tests PRIVATE
    remo_core
    GTest::GTest
    GTest::Main
)

if(ENABLE_COVERAGE)
    target_compile_options(remo_tests PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(remo_tests PRIVATE --coverage)
endif()

if(ENABLE_ASAN_UBSAN AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"))
    target_compile_options(remo_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(remo_tests PRIVATE -fsanitize=address,undefined)
endif()

add_test(NAME remo_tests COMMAND remo_tests)
