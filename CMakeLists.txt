cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(Trakines VERSION 1.0.0)

# ─── Spout2 SDK via FetchContent ─────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    Spout2
    GIT_REPOSITORY https://github.com/leadedge/Spout2.git
    GIT_TAG 2.007.013
    GIT_SHALLOW TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
)
FetchContent_Populate(Spout2)

set(SPOUT_DIR "${spout2_SOURCE_DIR}/SPOUTSDK/SpoutGL")

# ─── Mod sources ─────────────────────────────────────────────────────────────
add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/spout/spout_sender.cpp

    # Spout2 SDK embedded sources
    ${SPOUT_DIR}/Spout.cpp
    ${SPOUT_DIR}/SpoutCopy.cpp
    ${SPOUT_DIR}/SpoutDirectX.cpp
    ${SPOUT_DIR}/SpoutFrameCount.cpp
    ${SPOUT_DIR}/SpoutGL.cpp
    ${SPOUT_DIR}/SpoutGLextensions.cpp
    ${SPOUT_DIR}/SpoutSenderNames.cpp
    ${SPOUT_DIR}/SpoutSharedMemory.cpp
    ${SPOUT_DIR}/SpoutUtils.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${SPOUT_DIR}
)

# Spout2 needs DirectX and OpenGL system libs
target_link_libraries(${PROJECT_NAME}
    d3d11
    dxgi
    opengl32
    shlwapi
    Version
)

# Suppress Spout2 warnings (third-party code)
if(MSVC)
    set_source_files_properties(
        ${SPOUT_DIR}/Spout.cpp
        ${SPOUT_DIR}/SpoutCopy.cpp
        ${SPOUT_DIR}/SpoutDirectX.cpp
        ${SPOUT_DIR}/SpoutFrameCount.cpp
        ${SPOUT_DIR}/SpoutGL.cpp
        ${SPOUT_DIR}/SpoutGLextensions.cpp
        ${SPOUT_DIR}/SpoutSenderNames.cpp
        ${SPOUT_DIR}/SpoutSharedMemory.cpp
        ${SPOUT_DIR}/SpoutUtils.cpp
        PROPERTIES COMPILE_FLAGS "/w"
    )
endif()

# ─── Geode SDK ───────────────────────────────────────────────────────────────
if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK environment variable not set")
endif()

set(GEODE_LINK_SOURCE ON)
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})
