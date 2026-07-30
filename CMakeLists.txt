cmake_minimum_required(VERSION 3.16)
project(RemooDownload VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REMOODOWNLOAD_NAME "RemooDownload")
set(REMOODOWNLOAD_DOMAIN "com.remoodownload")
set(REMOODOWNLOAD_CORE_TARGET "remo_core")
set(REMOODOWNLOAD_NATIVE_HOST_TARGET "remo_native_host")

option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_GUI "Build Qt Widgets GUI" ON)
option(BUILD_CLI "Build CLI tool" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)
option(ENABLE_ASAN_UBSAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

if(BUILD_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
    find_package(Qt6 OPTIONAL_COMPONENTS LinguistTools)
endif()

if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
endif()

set(REMOODOWNLOAD_CORE_SOURCES
    src/common/logging.cpp
    src/core/download_manager.cpp
    src/engine/download_engine.cpp
    src/engine/http_engine.cpp
    src/engine/network_client.cpp
    src/engine/reconnect_manager.cpp
    src/engine/network_monitor.cpp
    src/engine/segment_planner.cpp
    src/queue/queue_manager.cpp
    src/scheduler/schedule_engine.cpp
    src/categories/category_engine.cpp
    src/speed/speed_limiter.cpp
    src/storage/storage_manager.cpp
    src/browser/browser_integration.cpp
    src/browser/native_messaging_host.cpp
    src/browser/clipboard_monitor.cpp
    src/checksum/streaming_checksum.cpp
    src/plugins/plugin_interface.cpp
    src/plugins/antivirus_hook.cpp
)

set(REMOODOWNLOAD_CORE_HEADERS
    include/common/logging.h
    include/common/result.h
    include/common/types.h
    include/core/download_manager.h
    include/engine/download_engine.h
    include/engine/http_engine.h
    include/engine/network_client.h
    include/engine/reconnect_manager.h
    include/engine/network_monitor.h
    include/engine/segment_planner.h
    include/queue/queue_manager.h
    include/scheduler/schedule_engine.h
    include/categories/category_engine.h
    include/speed/speed_limiter.h
    include/storage/storage_manager.h
    include/browser/browser_integration.h
    include/browser/native_messaging_host.h
    include/browser/clipboard_monitor.h
    include/checksum/streaming_checksum.h
    include/plugins/plugin_interface.h
    include/plugins/antivirus_hook.h
)

set(REMOODOWNLOAD_GUI_SOURCES
    src/main.cpp
    src/ui/main_window.cpp
    src/ui/add_download_dialog.cpp
    src/ui/properties_dialog.cpp
    src/ui/settings_dialog.cpp
    src/ui/speed_graph.cpp
    src/ui/tray_icon.cpp
)

set(REMOODOWNLOAD_GUI_HEADERS
    include/ui/main_window.h
    include/ui/add_download_dialog.h
    include/ui/properties_dialog.h
    include/ui/settings_dialog.h
    include/ui/speed_graph.h
    include/ui/tray_icon.h
)

set(REMOODOWNLOAD_TRANSLATIONS
    resources/translations/remoodownload_ar.ts
    resources/translations/remoodownload_en.ts
)

if(BUILD_GUI AND TARGET Qt6::lrelease)
    qt6_add_translation(REMOODOWNLOAD_QM_FILES ${REMOODOWNLOAD_TRANSLATIONS})
endif()

add_library(${REMOODOWNLOAD_CORE_TARGET} STATIC
    ${REMOODOWNLOAD_CORE_SOURCES}
    ${REMOODOWNLOAD_CORE_HEADERS}
)

target_include_directories(${REMOODOWNLOAD_CORE_TARGET} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CURL_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
    ${OpenSSL_INCLUDE_DIR}
)

target_link_libraries(${REMOODOWNLOAD_CORE_TARGET} PUBLIC
    CURL::libcurl
    SQLite::SQLite3
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_compile_definitions(${REMOODOWNLOAD_CORE_TARGET} PUBLIC
    REMOODOWNLOAD_VERSION="${PROJECT_VERSION}"
    REMOODOWNLOAD_DOMAIN="${REMOODOWNLOAD_DOMAIN}"
    REMOODOWNLOAD_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(ENABLE_COVERAGE)
    target_compile_options(${REMOODOWNLOAD_CORE_TARGET} PUBLIC --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(${REMOODOWNLOAD_CORE_TARGET} PUBLIC --coverage)
endif()

if(ENABLE_ASAN_UBSAN AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"))
    target_compile_options(${REMOODOWNLOAD_CORE_TARGET} PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${REMOODOWNLOAD_CORE_TARGET} PUBLIC -fsanitize=address,undefined)
endif()

if(BUILD_CLI)
    add_executable(remo_cli
        src/cli/main.cpp
    )

    target_link_libraries(remo_cli PRIVATE
        ${REMOODOWNLOAD_CORE_TARGET}
    )

    set_target_properties(remo_cli PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

if(BUILD_GUI)
    add_executable(${REMOODOWNLOAD_NAME}
        ${REMOODOWNLOAD_GUI_SOURCES}
        ${REMOODOWNLOAD_GUI_HEADERS}
        ${REMOODOWNLOAD_QM_FILES}
    )

    target_link_libraries(${REMOODOWNLOAD_NAME} PRIVATE
        ${REMOODOWNLOAD_CORE_TARGET}
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
    )

    add_executable(${REMOODOWNLOAD_NATIVE_HOST_TARGET}
        src/browser/native_host_main.cpp
    )

    target_link_libraries(${REMOODOWNLOAD_NATIVE_HOST_TARGET} PRIVATE
        ${REMOODOWNLOAD_CORE_TARGET}
    )

    set_target_properties(${REMOODOWNLOAD_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        WIN32_EXECUTABLE ON
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist"
    )

    add_custom_command(TARGET ${REMOODOWNLOAD_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources"
            "$<TARGET_FILE_DIR:${REMOODOWNLOAD_NAME}>/resources"
    )

    add_custom_command(TARGET ${REMOODOWNLOAD_NATIVE_HOST_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/native-messaging"
            "$<TARGET_FILE_DIR:${REMOODOWNLOAD_NATIVE_HOST_TARGET}>/native-messaging"
    )
endif()
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_GUI)
    install(TARGETS ${REMOODOWNLOAD_NAME} ${REMOODOWNLOAD_NATIVE_HOST_TARGET}
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .
    )
endif()

if(BUILD_CLI)
    install(TARGETS remo_cli
        RUNTIME DESTINATION bin
    )
endif()
install(DIRECTORY resources/ DESTINATION share/${REMOODOWNLOAD_NAME})
install(DIRECTORY extensions/ DESTINATION share/${REMOODOWNLOAD_NAME}/extensions)
install(DIRECTORY docs/ DESTINATION share/${REMOODOWNLOAD_NAME}/docs)
install(DIRECTORY db/ DESTINATION share/${REMOODOWNLOAD_NAME}/db)
