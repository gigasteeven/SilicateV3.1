cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(Trakines VERSION 1.0.0)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/mod.json" MOD_JSON)
string(JSON MOD_ID GET "${MOD_JSON}" id)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/global.cpp

    src/layout/layout_mode.cpp

    src/spout/spout_sender.cpp

    src/mirror/mirror_renderer.cpp

    src/hooks/playlayer_hooks.cpp
    src/hooks/leveltools_hooks.cpp
    src/hooks/loading_hooks.cpp

    src/ui/trakines_menu.cpp
)

# ── Geode SDK ──────────────────────────────────────────────
if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Set GEODE_SDK env var.")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(${PROJECT_NAME})

# ── Spout2 (Windows only, dynamic DLL loading) ─────────────
# SpoutLibrary.dll is bundled in resources/ and loaded at runtime.
# We only need the header (SpoutLibrary.h) which defines a pure
# virtual interface — no GL headers, no static lib needed.
if (WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/Spout2/include
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        TRAKINES_SPOUT_ENABLED
    )
endif()

# ── MSVC flags (from XDBotFork — prevent CL.exe crash on big files) ──
if (MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
    set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO FALSE)
    set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /Zm200 /bigobj /d2ReducedOptimizeHugeFunctions
    )
endif()
