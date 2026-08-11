cmake_minimum_required(VERSION 3.16)

project(M5CardputerZeroPiano VERSION 0.1.0 LANGUAGES C CXX ASM)

option(PIANO_USE_SDL "Use LVGL's SDL backend for desktop testing" ON)
option(PIANO_BUILD_TESTS "Build Piano unit tests" OFF)
if(PIANO_USE_SDL)
    set(PIANO_USE_PULSEAUDIO_DEFAULT OFF)
else()
    set(PIANO_USE_PULSEAUDIO_DEFAULT ON)
endif()
option(PIANO_USE_PULSEAUDIO "Restrict miniaudio to the PulseAudio backend" ${PIANO_USE_PULSEAUDIO_DEFAULT})
unset(PIANO_USE_PULSEAUDIO_DEFAULT)
set(PIANO_BIN_NAME "M5CardputerZero-Piano" CACHE STRING "Output executable name")
set(PIANO_DEPS_DIR "${CMAKE_CURRENT_LIST_DIR}/dependencies" CACHE PATH "Third-party source directory")
set(PIANO_OUTPUT_DIR "${CMAKE_CURRENT_LIST_DIR}/dist" CACHE PATH "Runtime output directory")
set(PIANO_DEFAULT_SOUNDFONT_PATH "${CMAKE_CURRENT_LIST_DIR}/assets/soundfonts/piano.sf2" CACHE FILEPATH
    "Default SoundFont path used when PIANO_SOUNDFONT_PATH is unset"
)

if(APPLE AND NOT PIANO_USE_SDL)
    message(FATAL_ERROR "macOS builds only support PIANO_USE_SDL=ON.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(PIANO_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(PIANO_SRC_DIR "${PIANO_ROOT}/src")
set(PIANO_LVGL_DIR "${PIANO_DEPS_DIR}/lvgl")
set(PIANO_SPDLOG_DIR "${PIANO_DEPS_DIR}/spdlog")
set(PIANO_SMOOTH_UI_DIR "${PIANO_DEPS_DIR}/smooth_ui_toolkit")
set(PIANO_MINIAUDIO_DIR "${PIANO_DEPS_DIR}/miniaudio")
set(PIANO_TSF_DIR "${PIANO_DEPS_DIR}/TinySoundFont")
set(PIANO_SOUNDFONT_FILE "${PIANO_ROOT}/assets/soundfonts/piano.sf2")
set(PIANO_SYSTEM_LIBS ${CMAKE_DL_LIBS})

if(UNIX AND NOT APPLE)
    list(APPEND PIANO_SYSTEM_LIBS m)
endif()

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
endif()

foreach(required_dir IN LISTS PIANO_LVGL_DIR PIANO_SPDLOG_DIR PIANO_SMOOTH_UI_DIR)
    if(NOT EXISTS "${required_dir}/CMakeLists.txt")
        message(FATAL_ERROR "Missing dependency: ${required_dir}. Run ./bootstrap.sh first or set PIANO_DEPS_DIR.")
    endif()
endforeach()
if(NOT EXISTS "${PIANO_MINIAUDIO_DIR}/miniaudio.h")
    message(FATAL_ERROR "Missing dependency: ${PIANO_MINIAUDIO_DIR}. Run ./bootstrap.sh first or set PIANO_DEPS_DIR.")
endif()
if(NOT EXISTS "${PIANO_TSF_DIR}/tsf.h")
    message(FATAL_ERROR "Missing dependency: ${PIANO_TSF_DIR}. Run ./bootstrap.sh first or set PIANO_DEPS_DIR.")
endif()
if(NOT EXISTS "${PIANO_SOUNDFONT_FILE}")
    message(FATAL_ERROR "Missing packaged SoundFont: ${PIANO_SOUNDFONT_FILE}")
endif()

find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

set(LV_BUILD_CONF_PATH "${PIANO_ROOT}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)

set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)

set(SMOOTH_UI_TOOLKIT_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SMOOTH_UI_TOOLKIT_BUILD_TEST OFF CACHE BOOL "" FORCE)

add_subdirectory("${PIANO_LVGL_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/dependencies/lvgl" EXCLUDE_FROM_ALL)
add_subdirectory("${PIANO_SPDLOG_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/dependencies/spdlog" EXCLUDE_FROM_ALL)
add_subdirectory("${PIANO_SMOOTH_UI_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/dependencies/smooth_ui_toolkit" EXCLUDE_FROM_ALL)

function(piano_add_sdl2_parent_include target imported_target)
    if(NOT TARGET "${imported_target}")
        return()
    endif()

    get_target_property(sdl2_include_dirs "${imported_target}" INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT sdl2_include_dirs)
        return()
    endif()

    foreach(sdl2_include_dir IN LISTS sdl2_include_dirs)
        if(sdl2_include_dir MATCHES "/SDL2$")
            get_filename_component(sdl2_parent_include_dir "${sdl2_include_dir}" DIRECTORY)
            target_include_directories("${target}" SYSTEM PRIVATE "${sdl2_parent_include_dir}")
        endif()
    endforeach()
endfunction()

target_compile_definitions(lvgl PUBLIC
    PIANO_LVGL_USE_SDL=$<IF:$<BOOL:${PIANO_USE_SDL}>,1,0>
)

if(PIANO_USE_SDL)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
    endif()

    if(SDL2_FOUND)
        target_link_libraries(lvgl PRIVATE PkgConfig::SDL2)
        piano_add_sdl2_parent_include(lvgl PkgConfig::SDL2)
        set(PIANO_SDL_LIBS PkgConfig::SDL2)
    else()
        find_package(SDL2 CONFIG QUIET)
        if(TARGET SDL2::SDL2)
            target_link_libraries(lvgl PRIVATE SDL2::SDL2)
            piano_add_sdl2_parent_include(lvgl SDL2::SDL2)
            set(PIANO_SDL_LIBS SDL2::SDL2)
        else()
            find_package(SDL2 REQUIRED)
            target_include_directories(lvgl SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
            foreach(sdl2_include_dir IN LISTS SDL2_INCLUDE_DIRS)
                if(sdl2_include_dir MATCHES "/SDL2$")
                    get_filename_component(sdl2_parent_include_dir "${sdl2_include_dir}" DIRECTORY)
                    target_include_directories(lvgl SYSTEM PRIVATE "${sdl2_parent_include_dir}")
                endif()
            endforeach()
            set(PIANO_SDL_LIBS ${SDL2_LIBRARIES})
        endif()
    endif()
endif()

file(GLOB PIANO_CPP CONFIGURE_DEPENDS
    "${PIANO_SRC_DIR}/main.cpp"
    "${PIANO_SRC_DIR}/audio/*.cpp"
    "${PIANO_SRC_DIR}/assets/*.cpp"
    "${PIANO_SRC_DIR}/core/*.cpp"
    "${PIANO_SRC_DIR}/hal/*.cpp"
    "${PIANO_SRC_DIR}/input/*.cpp"
    "${PIANO_SRC_DIR}/models/*.cpp"
    "${PIANO_SRC_DIR}/view_models/*.cpp"
    "${PIANO_SRC_DIR}/views/*.cpp"
)
file(GLOB_RECURSE PIANO_ASSET_C CONFIGURE_DEPENDS
    "${PIANO_SRC_DIR}/assets/res_c/*.c"
)

add_executable(${PIANO_BIN_NAME}
    ${PIANO_CPP}
    ${PIANO_ASSET_C}
    "${PIANO_SRC_DIR}/audio/miniaudio_impl.c"
)

target_include_directories(${PIANO_BIN_NAME} PRIVATE
    "${PIANO_SRC_DIR}"
    "${PIANO_SRC_DIR}/assets"
)
target_include_directories(${PIANO_BIN_NAME} SYSTEM PRIVATE
    "${PIANO_MINIAUDIO_DIR}"
    "${PIANO_TSF_DIR}"
)

target_compile_definitions(${PIANO_BIN_NAME} PRIVATE
    PIANO_USE_SDL=$<IF:$<BOOL:${PIANO_USE_SDL}>,1,0>
    PIANO_USE_PULSEAUDIO=$<IF:$<BOOL:${PIANO_USE_PULSEAUDIO}>,1,0>
    PIANO_DEFAULT_SOUNDFONT_PATH="${PIANO_DEFAULT_SOUNDFONT_PATH}"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PIANO_BIN_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(${PIANO_BIN_NAME} PRIVATE
    lvgl::lvgl
    smooth_ui_toolkit
    spdlog::spdlog
    Threads::Threads
    ${PIANO_SDL_LIBS}
    ${PIANO_SYSTEM_LIBS}
)

set_target_properties(${PIANO_BIN_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PIANO_OUTPUT_DIR}"
)

if(PIANO_BUILD_TESTS)
    enable_testing()

    add_executable(piano_soundfont_renderer_tests
        tests/soundfont_renderer_test.cpp
        src/audio/soundfont_renderer.cpp
    )
    target_include_directories(piano_soundfont_renderer_tests PRIVATE "${PIANO_SRC_DIR}")
    target_include_directories(piano_soundfont_renderer_tests SYSTEM PRIVATE "${PIANO_TSF_DIR}")
    target_compile_definitions(piano_soundfont_renderer_tests PRIVATE
        PIANO_TEST_SOUNDFONT_PATH="${PIANO_SOUNDFONT_FILE}"
    )
    target_link_libraries(piano_soundfont_renderer_tests PRIVATE ${PIANO_SYSTEM_LIBS})

    add_executable(piano_model_tests
        tests/piano_model_test.cpp
        src/core/piano_types.cpp
        src/models/piano_model.cpp
    )
    target_include_directories(piano_model_tests PRIVATE "${PIANO_SRC_DIR}")
    target_link_libraries(piano_model_tests PRIVATE smooth_ui_toolkit)

    foreach(test_target IN ITEMS piano_soundfont_renderer_tests piano_model_tests)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${test_target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()

    add_test(NAME piano_soundfont_renderer COMMAND piano_soundfont_renderer_tests)
    add_test(NAME piano_model COMMAND piano_model_tests)
endif()
