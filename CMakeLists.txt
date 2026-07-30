cmake_minimum_required(VERSION 3.16)
project(RemoDownload VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REMODOWNLOAD_NAME "RemoDownload")
set(REMODOWNLOAD_DOMAIN "com.remodownload")

option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_CLI "Build CLI tool" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)
option(ENABLE_ASAN_UBSAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
endif()

set(REMODOWNLOAD_SOURCES
    src/main.cpp
    src/core/download_manager.cpp
    src/engine/download_engine.cpp
    src/engine/http_engine.cpp
    src/engine/reconnect_manager.cpp
    src/engine/network_monitor.cpp
    src/queue/queue_manager.cpp
    src/scheduler/schedule_engine.cpp
    src/categories/category_engine.cpp
    src/speed/speed_limiter.cpp
    src/storage/storage_manager.cpp
    src/browser/browser_integration.cpp
    src/browser/native_messaging_host.cpp
    src/browser/clipboard_monitor.cpp
    src/ui/main_window.cpp
    src/ui/add_download_dialog.cpp
    src/ui/properties_dialog.cpp
    src/ui/settings_dialog.cpp
    src/ui/speed_graph.cpp
    src/ui/tray_icon.cpp
    src/checksum/streaming_checksum.cpp
    src/plugins/plugin_interface.cpp
    src/plugins/antivirus_hook.cpp
)

set(REMODOWNLOAD_HEADERS
    include/core/download_manager.h
    include/engine/download_engine.h
    include/engine/http_engine.h
    include/engine/reconnect_manager.h
    include/engine/network_monitor.h
    include/queue/queue_manager.h
    include/scheduler/schedule_engine.h
    include/categories/category_engine.h
    include/speed/speed_limiter.h
    include/storage/storage_manager.h
    include/browser/browser_integration.h
    include/browser/native_messaging_host.h
    include/browser/clipboard_monitor.h
    include/ui/main_window.h
    include/ui/add_download_dialog.h
    include/ui/properties_dialog.h
    include/ui/settings_dialog.h
    include/ui/speed_graph.h
    include/ui/tray_icon.h
    include/checksum/streaming_checksum.h
    include/plugins/plugin_interface.h
    include/plugins/antivirus_hook.h
)

add_executable(${REMODOWNLOAD_NAME} ${REMODOWNLOAD_SOURCES} ${REMODOWNLOAD_HEADERS})

target_include_directories(${REMODOWNLOAD_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CURL_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
    ${OpenSSL_INCLUDE_DIR}
)

target_link_libraries(${REMODOWNLOAD_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    CURL::libcurl
    SQLite::SQLite3
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_compile_definitions(${REMODOWNLOAD_NAME} PRIVATE
    REMODOWNLOAD_VERSION="${PROJECT_VERSION}"
    REMODOWNLOAD_DOMAIN="${REMODOWNLOAD_DOMAIN}"
    REMODOWNLOAD_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(ENABLE_COVERAGE)
    target_compile_options(${REMODOWNLOAD_NAME} PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(${REMODOWNLOAD_NAME} PRIVATE --coverage)
endif()

if(ENABLE_ASAN_UBSAN AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"))
    target_compile_options(${REMODOWNLOAD_NAME} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${REMODOWNLOAD_NAME} PRIVATE -fsanitize=address,undefined)
endif()

set_target_properties(${REMODOWNLOAD_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist"
)

add_custom_command(TARGET ${REMODOWNLOAD_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:${REMODOWNLOAD_NAME}>/resources"
)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS ${REMODOWNLOAD_NAME}
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

install(DIRECTORY resources/ DESTINATION share/${REMODOWNLOAD_NAME})
install(DIRECTORY docs/ DESTINATION share/${REMODOWNLOAD_NAME}/docs)
install(DIRECTORY db/ DESTINATION share/${REMODOWNLOAD_NAME}/db)
