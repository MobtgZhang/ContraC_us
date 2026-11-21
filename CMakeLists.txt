cmake_minimum_required(VERSION 3.21)
project(Contra LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CTest)
find_package(PkgConfig REQUIRED)

set(SDL "2" CACHE STRING "SDL major version (2 or 3)")
set_property(CACHE SDL PROPERTY STRINGS 2 3)
if(SDL STREQUAL "2")
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2>=2.0.10)
    pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer>=2.0.2)
    set(sdl_libraries PkgConfig::SDL2 PkgConfig::SDL2_MIXER)
    set(sdl_dummy_drivers "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
elseif(SDL STREQUAL "3")
    pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3>=3.2.0)
    set(sdl_libraries PkgConfig::SDL3)
    set(sdl_dummy_drivers "SDL_VIDEO_DRIVER=dummy;SDL_AUDIO_DRIVER=dummy")
else()
    message(FATAL_ERROR "SDL must be 2 or 3")
endif()

add_library(contra_game STATIC
    src/app.c src/assets.c src/data.c src/enemies.c src/funcs.c src/game.c
    src/globals.c src/graphics.c src/player.c src/sound.c src/timing.c
)
target_include_directories(contra_game PUBLIC src)
target_compile_definitions(contra_game PUBLIC CONTRA_SDL=${SDL})
target_link_libraries(contra_game PUBLIC ${sdl_libraries})

add_executable(contra src/main.c)
target_link_libraries(contra PRIVATE contra_game)
set_target_properties(contra PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_BINARY_DIR}>")

set(asset_dir "${CMAKE_CURRENT_BINARY_DIR}/resource")
configure_file(contra.ini "${asset_dir}/contra.ini" COPYONLY)
configure_file(resource/gfx.dat "${asset_dir}/gfx.dat" COPYONLY)
file(GLOB sound_effects CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resource/sfx/*.wav")
foreach(effect IN LISTS sound_effects)
    get_filename_component(name "${effect}" NAME)
    configure_file("${effect}" "${asset_dir}/sfx/${name}" COPYONLY)
endforeach()

set(music_archive "${CMAKE_CURRENT_SOURCE_DIR}/resource/BGMS.dat")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${music_archive}")
file(ARCHIVE_EXTRACT INPUT "${music_archive}" DESTINATION "${asset_dir}/bgm")

file(RELATIVE_PATH installed_assets "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_DATADIR}/contra")
target_compile_definitions(contra PRIVATE CONTRA_INSTALLED_ASSETS="${installed_assets}")
install(TARGETS contra RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY "${asset_dir}/" DESTINATION "${CMAKE_INSTALL_DATADIR}/contra")

if(BUILD_TESTING)
    add_executable(contra_smoke tests/smoke.c)
    target_link_libraries(contra_smoke PRIVATE contra_game)
    add_test(NAME contra_help COMMAND contra --help)
    foreach(scenario IN ITEMS input audio 1 2 3 4 5 6 7 8 ending)
        add_test(NAME "contra_${scenario}" COMMAND contra_smoke "${asset_dir}" "${scenario}")
        set_tests_properties("contra_${scenario}" PROPERTIES
            ENVIRONMENT "${sdl_dummy_drivers}"
            TIMEOUT 45
        )
    endforeach()
endif()
