cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(Trakines VERSION 1.0.0)

# ─── Spout2 SDK via FetchContent ──────────────────────────────────────────────
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

# Embedded Spout2 SDK sources (compiled directly into the mod).
set(SPOUT_SOURCES
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

# ─── Mod sources ──────────────────────────────────────────────────────────────
add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/spout/spout_sender.cpp
    ${SPOUT_SOURCES}
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

# ─── Isolate Spout2 from Geode's precompiled header ───────────────────────────
# Geode's PCH force-includes cocos2d (and its bundled GLEW) into every source
# file of this target. The embedded Spout2 SDK ships its own OpenGL extension
# loader whose function-pointer typedefs collide with GLEW's, causing
# "redeclaration of '__glewShaderSource' with a different type" errors.
#
# Skipping the PCH on the Spout SDK translation units (and on our thin wrapper,
# which is the ONLY file that includes Spout headers) keeps GLEW out of those
# units so Spout's own loader is used with no conflict. We also enable SSSE3
# (SpoutCopy.cpp uses _mm_shuffle_epi8) and silence third-party warnings.
set_source_files_properties(
    ${SPOUT_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/spout/spout_sender.cpp
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
        COMPILE_OPTIONS "-mssse3;-w"
)

# ─── Geode SDK ────────────────────────────────────────────────────────────────
if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK environment variable not set")
endif()

set(GEODE_LINK_SOURCE ON)
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})
