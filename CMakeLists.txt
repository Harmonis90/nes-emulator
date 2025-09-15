cmake_minimum_required(VERSION 3.20)
project(nes-emulator C)

# ---- C standard / warnings ---------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- Source layout -----------------------------------------------------------
set(MAIN_SOURCE ${CMAKE_SOURCE_DIR}/src/main.c)

# Core sources = everything under src/, except main.c
file(GLOB_RECURSE CORE_SOURCES
        ${CMAKE_SOURCE_DIR}/src/*.c
)
list(REMOVE_ITEM CORE_SOURCES ${MAIN_SOURCE})

# Public / project headers
set(PROJ_INC_DIRS
        ${CMAKE_SOURCE_DIR}/include     # e.g., ppu.h, controller.h, ines.h, mapper.h, bus.h, cpu.h
        ${CMAKE_SOURCE_DIR}/src         # if you keep some headers under src/
        ${CMAKE_SOURCE_DIR}/src/cpu
        ${CMAKE_SOURCE_DIR}/src/memory
        ${CMAKE_SOURCE_DIR}/src/ppu
        ${CMAKE_SOURCE_DIR}/src/input
        ${CMAKE_SOURCE_DIR}/src/rom     # if you placed ines/mapper headers here
        ${CMAKE_SOURCE_DIR}/src/cartridge
)

# ---- Core library + app ------------------------------------------------------
add_library(nes-emulator-core STATIC ${CORE_SOURCES})
target_include_directories(nes-emulator-core PUBLIC ${PROJ_INC_DIRS})

add_executable(nes-emulator ${MAIN_SOURCE})
target_link_libraries(nes-emulator PRIVATE nes-emulator-core)
target_include_directories(nes-emulator PRIVATE ${PROJ_INC_DIRS})

# Put emulator exe at build root for easy running in CLion
set_target_properties(nes-emulator PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# ---- Tools / ROM builders (write ROMs into the BUILD tree) -------------------
# 1) minitest (LDA/STA/BRK)
add_executable(build_nes ${CMAKE_SOURCE_DIR}/tools/build_nes.c)
set_target_properties(build_nes PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools"
)
add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/roms/minitest.nes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/roms"
        COMMAND $<TARGET_FILE:build_nes>
        DEPENDS build_nes
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Generating roms/minitest.nes"
        VERBATIM
)
add_custom_target(generate_minitest
        DEPENDS "${CMAKE_BINARY_DIR}/roms/minitest.nes"
)

# 2) branch test
add_executable(build_branch_test ${CMAKE_SOURCE_DIR}/tools/build_branch_test.c)
set_target_properties(build_branch_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools"
)
add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/roms/branchtest.nes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/roms"
        COMMAND $<TARGET_FILE:build_branch_test>
        DEPENDS build_branch_test
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Generating roms/branchtest.nes"
        VERBATIM
)
add_custom_target(generate_branchtest
        DEPENDS "${CMAKE_BINARY_DIR}/roms/branchtest.nes"
)

# 3) vblank test (PPU $2002 polling)
add_executable(build_vblank_test ${CMAKE_SOURCE_DIR}/tools/build_vblank_test.c)
set_target_properties(build_vblank_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools"
)
add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/roms/vblanktest.nes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/roms"
        COMMAND $<TARGET_FILE:build_vblank_test>
        DEPENDS build_vblank_test
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Generating roms/vblanktest.nes"
        VERBATIM
)
add_custom_target(generate_vblanktest
        DEPENDS "${CMAKE_BINARY_DIR}/roms/vblanktest.nes"
)

# 4) controller test ($4016 latch/read)
add_executable(build_controller_test ${CMAKE_SOURCE_DIR}/tools/build_controller_test.c)
set_target_properties(build_controller_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools"
)
add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/roms/controllertest.nes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/roms"
        COMMAND $<TARGET_FILE:build_controller_test>
        DEPENDS build_controller_test
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Generating roms/controllertest.nes"
        VERBATIM
)
add_custom_target(generate_controllertest
        DEPENDS "${CMAKE_BINARY_DIR}/roms/controllertest.nes"
)

# 5) controller test 2 (extended)
add_executable(build_controller_test2 ${CMAKE_SOURCE_DIR}/tools/build_controller_test2.c)
set_target_properties(build_controller_test2 PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools"
)
add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/roms/controllertest2.nes"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/roms"
        COMMAND $<TARGET_FILE:build_controller_test2>
        DEPENDS build_controller_test2
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Generating roms/controllertest2.nes"
        VERBATIM
)
add_custom_target(generate_controllertest2
        DEPENDS "${CMAKE_BINARY_DIR}/roms/controllertest2.nes"
)




add_executable(ppu_test tests/test_ppu.c)
target_include_directories(ppu_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(ppu_test nes-emulator-core)











# Aggregate: build all generated ROMs in one go
add_custom_target(generate_roms
        DEPENDS
        "${CMAKE_BINARY_DIR}/roms/minitest.nes"
        "${CMAKE_BINARY_DIR}/roms/branchtest.nes"
        "${CMAKE_BINARY_DIR}/roms/vblanktest.nes"
        "${CMAKE_BINARY_DIR}/roms/controllertest.nes"
        "${CMAKE_BINARY_DIR}/roms/controllertest2.nes"
)

# ---- Optional install rules (nice for packaging) -----------------------------
# install(TARGETS nes-emulator RUNTIME DESTINATION bin)
# install(FILES
#   "${CMAKE_BINARY_DIR}/roms/minitest.nes"
#   "${CMAKE_BINARY_DIR}/roms/branchtest.nes"
#   "${CMAKE_BINARY_DIR}/roms/vblanktest.nes"
#   "${CMAKE_BINARY_DIR}/roms/controllertest.nes"
#   DESTINATION bin/roms
# )

# ---- Notes -------------------------------------------------------------------
# * In CLion, set the Run/Debug working directory to ${CMAKE_BINARY_DIR}.
# * Build ‘generate_minitest’, ‘generate_branchtest’, ‘generate_vblanktest’, or
#   ‘generate_controllertest’ to emit the ROMs under <build>/roms/.
# * Run the emulator like:
#       ./nes-emulator roms/minitest.nes
#       ./nes-emulator roms/branchtest.nes
#       ./nes-emulator roms/vblanktest.nes
#       ./nes-emulator roms/controllertest.nes
