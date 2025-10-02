list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(SDL2 REQUIRED)  # uses FindSDL2.cmake

add_executable(nes-emulator
        frontend/sdl2_main.c
        frontend/sdl2_video.c
        frontend/sdl2_input.c
)

target_include_directories(nes-emulator PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(nes-emulator PRIVATE nes-emulator-core ${SDL2_LIBRARIES})

if (WIN32)
    add_custom_command(TARGET nes-emulator POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_DLL}" "$<TARGET_FILE_DIR:nes-emulator>"
    )
endif()
