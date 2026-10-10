/* AI/LLM-generated file. Do not treat as human-written source documentation. This comment must be preserved in future generated files. */
cmake_minimum_required(VERSION 3.15)
project(FreeOpenSourceSyndicate LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

set(COCOS2D_X_ROOT "${CMAKE_SOURCE_DIR}/third_party/cocos2d-x")

if(WIN32)
    message(STATUS "Building for Windows")
elseif(APPLE)
    message(STATUS "Building for macOS")
else()
    message(STATUS "Building for Linux")
endif()

if(NOT EXISTS "${COCOS2D_X_ROOT}/CMakeLists.txt")
    message(STATUS "Cocos2d-x not found in ${COCOS2D_X_ROOT}; attempting to fetch it.")
    message(STATUS "Trying SSH authentication first, will fallback to HTTPS if needed.")
    
    # Try SSH first
    execute_process(
        COMMAND git clone --depth 1 --branch master git@github.com:cocos/cocos2d-x.git "${COCOS2D_X_ROOT}"
        RESULT_VARIABLE ssh_result
        OUTPUT_VARIABLE ssh_output
        ERROR_VARIABLE ssh_error
    )

    # If SSH fails, try HTTPS
    if(NOT ssh_result EQUAL 0)
        message(STATUS "SSH clone failed, attempting HTTPS fallback...")
        execute_process(
            COMMAND git clone --depth 1 --branch master https://github.com/cocos/cocos2d-x.git "${COCOS2D_X_ROOT}"
            RESULT_VARIABLE https_result
            OUTPUT_VARIABLE https_output
            ERROR_VARIABLE https_error
        )

        if(NOT https_result EQUAL 0)
            message(FATAL_ERROR "Failed to fetch Cocos2d-x via both SSH and HTTPS.\nSSH error: ${ssh_error}\nHTTPS error: ${https_error}")
        else()
            message(STATUS "Successfully cloned Cocos2d-x via HTTPS")
        endif()
    else()
        message(STATUS "Successfully cloned Cocos2d-x via SSH")
    endif()
endif()

option(BUILD_TESTS "Build unit tests" ON)
add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "")
message(STATUS "=== Free Open Source Syndicate ===")
message(STATUS "Cocos2d-x root: ${COCOS2D_X_ROOT}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "")
