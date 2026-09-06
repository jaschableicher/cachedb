cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are disabled. Use: cmake --preset linux-debug"
    )
endif()

project(cache_db LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

function(enable_warnings target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
        >
    )
endfunction()

add_library(cache_db_core
    src/commands/command.cpp
    src/commands/executor.cpp
    src/commands/parser.cpp
    src/commands/registry.cpp
    src/commands/utils.cpp
    src/database/database.cpp
    src/server/server.cpp
    src/server/client.cpp
    src/database/expiration.cpp
)
target_include_directories(cache_db_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/commands
    ${CMAKE_CURRENT_SOURCE_DIR}/src/server
)
enable_warnings(cache_db_core)

add_executable(cache_db src/main.cpp)
target_link_libraries(cache_db PRIVATE cache_db_core)
enable_warnings(cache_db)

option(BUILD_BENCHMARKS "Build standalone benchmark executables" OFF)
#cmake --preset linux-debug -DBUILD_BENCHMARKS=ON
if(BUILD_BENCHMARKS)
    find_package(Threads REQUIRED)

    add_executable(server_connection_load benchmarks/server_connection_load.cpp)
    target_link_libraries(server_connection_load PRIVATE Threads::Threads)
    enable_warnings(server_connection_load)
endif()

if(BUILD_TESTING)
    # Prefer an installed package, but make a clean checkout self-contained.
    find_package(GTest CONFIG QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.16.0
            GIT_SHALLOW TRUE
        )
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    file(GLOB UNIT_TEST_SOURCES CONFIGURE_DEPENDS tests/unit/*.cpp)
    add_executable(unit_tests ${UNIT_TEST_SOURCES})
    target_link_libraries(unit_tests PRIVATE cache_db_core GTest::gtest_main)
    
    enable_warnings(unit_tests)
    add_test(NAME unit_tests COMMAND unit_tests)

    file(GLOB INTEGRATION_TEST_SOURCES CONFIGURE_DEPENDS tests/integration/*.cpp)
    add_executable(integration_tests ${INTEGRATION_TEST_SOURCES})
    target_link_libraries(integration_tests PRIVATE cache_db_core GTest::gtest_main)
    enable_warnings(integration_tests)
    add_test(NAME integration_tests COMMAND integration_tests)
    
endif()
