cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are disabled. Use: cmake --preset mingw-debug"
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
    src/commands/executor.cpp
    src/commands/parser.cpp
    src/database/database.cpp
)
target_include_directories(cache_db_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/commands
)
enable_warnings(cache_db_core)

add_executable(cache_db src/main.cpp)
target_link_libraries(cache_db PRIVATE cache_db_core)
enable_warnings(cache_db)

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

    add_executable(db_tests tests/command_tests.cpp)
    target_link_libraries(db_tests PRIVATE cache_db_core GTest::gtest_main)
  
    enable_warnings(db_tests)
    add_test(NAME db_tests COMMAND db_tests)
endif()