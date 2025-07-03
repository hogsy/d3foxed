cmake_minimum_required(VERSION 3.10.0)
project(d3Foxed)

OPTION(ID_ALLOW_TOOLS "Compile Tools" ON)
OPTION(ID_ALLOW_MAYA "Compile Maya Tools" ON)
OPTION(ID_UNICODE "Use unicode version of WIN32 API" OFF)
OPTION(ID_ENFORCE32BIT "Build 32bit on 64bit linux platform" OFF)
# New options start here
option(FX_BUILD_D3 "Compile the original Doom 3 game" OFF)

include(setup.cmake)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Get the current working branch
execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
# Get the latest abbreviated commit hash of the working branch
execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
# Get the number of commits on the working branch
execute_process(
        COMMAND git rev-list HEAD --count
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_COMMIT_COUNT
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_definitions(-DGIT_COMMIT_HASH="${GIT_COMMIT_HASH}")
add_definitions(-DGIT_COMMIT_COUNT=${GIT_COMMIT_COUNT})
add_definitions(-DGIT_BRANCH="${GIT_BRANCH}")

include_directories(neo/renderer)
include_directories(neo/glew)
add_subdirectory(neo/idlib)
IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    add_subdirectory(neo/curl)
    add_subdirectory(neo/TypeInfo)
    if (ID_ALLOW_MAYA)
        add_subdirectory(neo/maya2md5)
    endif ()
ENDIF ()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/release/base/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/release/base/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/release/base/)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/release/base/)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/release/base/)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/release/base/)

if (WIN32 AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
endif ()
set(CMAKE_SHARED_LIBRARY_PREFIX "")
if (FX_BUILD_D3)
    add_subdirectory(neo/game)
    add_subdirectory(neo/d3xp)
else ()
    add_subdirectory(neo/game_fracture)
endif ()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/release/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/release/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/release/)

add_subdirectory(neo)
