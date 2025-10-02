cmake_minimum_required(VERSION 3.20)
project(nes-emulator C)

# -------------------------------------------------
# Language & warnings
# -------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# -------------------------------
# Options
# -------------------------------
option(DEBUG_TRACE "Enable verbose CPU trace logs" OFF)
option(ENABLE_PPU_TRACE "Log one line per frame and VBL edges" OFF)

# -------------------------------
# Include paths used across targets
# -------------------------------
set(PROJ_INC_DIRS
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/cpu
        ${CMAKE_SOURCE_DIR}/src/ppu
        ${CMAKE_SOURCE_DIR}/src/memory
        ${CMAKE_SOURCE_DIR}/src/input
        ${CMAKE_SOURCE_DIR}/src/cartridge
        ${CMAKE_SOURCE_DIR}/src/rom
        ${CMAKE_SOURCE_DIR}/src/nes
        ${CMAKE_SOURCE_DIR}/src/apu
        ${CMAKE_SOURCE_DIR}/frontend
)

# -------------------------------
# Core library
# -------------------------------
add_library(nes-emulator-core
        # Bus
        src/memory/bus.c

        # CPU
        src/cpu/cpu.c
        src/cpu/cpu_addr.c
        src/cpu/cpu_internal.c
        src/cpu/cpu_ops.c
        src/cpu/cpu_table.c
        src/cpu/cpu_timing.c

        # PPU
        src/ppu/ppu.c
        src/ppu/ppu_mem.c
        src/ppu/ppu_oam.c
        src/ppu/ppu_regs.c
        src/ppu/ppu_timing.c
        src/ppu/ppu_render.c
        src/ppu/nes_palette.c

        # Cartridge + mappers
        src/cartridge/cartridge.c
        src/cartridge/ines.c
        src/cartridge/mapper.c
        src/cartridge/mapper_nrom.c
        src/cartridge/mapper_mmc3.c

        # Input
        src/input/controller.c

        # NES
        src/nes/nes.c
        src/nes/rom_loader.c

        # Audio
        src/apu/apu.c
        src/apu/apu_pulse.c
        src/apu/apu_triangle.c
        src/apu/apu_noise.c
        src/apu/apu_dmc.c
        src/apu/apu_mixer.c
)
target_include_directories(nes-emulator-core PUBLIC ${PROJ_INC_DIRS})
if (DEBUG_TRACE)
    target_compile_definitions(nes-emulator-core PRIVATE DEBUG_TRACE=1)
endif()
if (ENABLE_PPU_TRACE)
    target_compile_definitions(nes-emulator-core PRIVATE PPU_TRACE=1)
endif()

# --------------------------------
# SDL2 (subproject) — ensure this is SDL2
# --------------------------------
set(SDL_SHARED  ON  CACHE BOOL "" FORCE)   # build DLL on Windows
set(SDL_STATIC  OFF CACHE BOOL "" FORCE)
set(SDL_TEST    OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(USE_WINSDK_DIRECTX TRUE CACHE BOOL "" FORCE)

add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

# Resolve SDL2 target to link
set(_SDL2_LIB "")
if (TARGET SDL2::SDL2)
    set(_SDL2_LIB SDL2::SDL2)
elseif (TARGET SDL2)
    set(_SDL2_LIB SDL2)
elseif (TARGET SDL2-static)
    set(_SDL2_LIB SDL2-static)
endif()
if (NOT _SDL2_LIB)
    message(FATAL_ERROR "SDL2 subproject added, but no SDL2 target found. Is external/SDL the SDL2 repo/branch?")
endif()

# -------------------------------
# SDL2 frontend library (no main)
# -------------------------------
add_library(nes-frontend-sdl2
        frontend/sdl2_frontend.c
        frontend/sdl2_frontend.h
        frontend/sdl2_audio.c
        frontend/sdl2_audio.h
)
target_include_directories(nes-frontend-sdl2
        PUBLIC  ${PROJECT_SOURCE_DIR}/frontend
        ${PROJECT_SOURCE_DIR}/include
        PRIVATE ${PROJECT_SOURCE_DIR}/external/SDL/include
)
target_link_libraries(nes-frontend-sdl2 PRIVATE ${_SDL2_LIB})

# -------------------------------
# Emulator executable (console app) — NO SDL2main
# -------------------------------
add_executable(nes-emulator
        frontend/sdl2_main.c     # contains int main()
        # DO NOT add src/memory/bus.c here; it's already in nes-emulator-core
)
target_include_directories(nes-emulator PRIVATE ${PROJ_INC_DIRS})

# We are NOT linking SDL2main; tell SDL we provide our own main()
target_compile_definitions(nes-emulator PRIVATE SDL_MAIN_HANDLED)

target_link_libraries(nes-emulator
        PRIVATE
        nes-emulator-core
        nes-frontend-sdl2
        ${_SDL2_LIB}
)

if (DEBUG_TRACE)
    target_compile_definitions(nes-emulator PRIVATE DEBUG_TRACE=1)
endif()
if (ENABLE_PPU_TRACE)
    target_compile_definitions(nes-emulator PRIVATE PPU_TRACE=1)
endif()

# Put binary at build root (handy for CLion run configs)
set_target_properties(nes-emulator PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Copy SDL2 runtime next to the exe (Windows + shared build)
if (WIN32 AND SDL_SHARED)
    add_custom_command(TARGET nes-emulator POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/external/SDL/$<IF:$<CONFIG:Debug>,SDL2d.dll,SDL2.dll>"
            "$<TARGET_FILE_DIR:nes-emulator>"
            VERBATIM
    )
endif()

# -------------------------------
# Tests (optional)
# -------------------------------
enable_testing()

add_executable(cpu-core-tests
        tests/test_bus.c
        tests/test_cpu_core.c
)
target_include_directories(cpu-core-tests PRIVATE ${PROJ_INC_DIRS} ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(cpu-core-tests PRIVATE nes-emulator-core)
if (DEBUG_TRACE)
    target_compile_definitions(cpu-core-tests PRIVATE DEBUG_TRACE=1)
endif()
add_test(NAME cpu-core-tests COMMAND cpu-core-tests)

add_executable(sprite-smoke-test
        tests/sprite_smoke_test.c
        tests/bus_stub.c
        tests/mapper_chr_stub.c
        src/ppu/ppu.c
        src/ppu/ppu_mem.c
        src/ppu/ppu_regs.c
        src/ppu/ppu_render.c
        src/ppu/ppu_timing.c
)
target_include_directories(sprite-smoke-test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(sprite-smoke-test PRIVATE nes-emulator-core)
add_test(NAME sprite-smoke-test COMMAND sprite-smoke-test)

add_executable(ppu-bus-integration-test
        tests/ppu_bus_integration_test.c
        src/ppu/ppu_regs.c
        src/ppu/ppu_mem.c
)
target_include_directories(ppu-bus-integration-test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(ppu-bus-integration-test PRIVATE nes-emulator-core)
add_test(NAME ppu-bus-integration-test COMMAND ppu-bus-integration-test)

add_executable(run_sanity tests/run_sanity.c)
target_include_directories(run_sanity PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(run_sanity PRIVATE nes-emulator-core)
