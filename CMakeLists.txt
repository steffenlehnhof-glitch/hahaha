cmake_minimum_required(VERSION 3.22)

# Muss vor project() stehen
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimale macOS-Version")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")   # keine VC-Redistributables noetig

project(Purrgression VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------------------------
#  Optionen
# ------------------------------------------------------------------------------
set(PURR_JUCE_PATH "" CACHE PATH "Lokaler JUCE-Ordner (leer = JUCE wird automatisch heruntergeladen)")
set(PURR_JUCE_TAG "8.0.8" CACHE STRING "JUCE-Version, die heruntergeladen wird")
set(PURR_FORMATS VST3 Standalone CACHE STRING "Plugin-Formate (VST3, Standalone, AU ...)")
set(PURR_EXTRA_DEFINITIONS "" CACHE STRING "Zusaetzliche Compiler-Definitionen")
option(PURR_BUILD_TESTS "Test-Programme mitbauen" OFF)

# ------------------------------------------------------------------------------
#  JUCE
# ------------------------------------------------------------------------------
if(PURR_JUCE_PATH)
    add_subdirectory("${PURR_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${PURR_JUCE_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ------------------------------------------------------------------------------
#  Plugin
# ------------------------------------------------------------------------------
juce_add_plugin(Purrgression
    COMPANY_NAME               "Purrgression"
    BUNDLE_ID                  "com.purrgression.plugin"
    PLUGIN_MANUFACTURER_CODE   Purr
    PLUGIN_CODE                Pgrs
    FORMATS                    ${PURR_FORMATS}
    PRODUCT_NAME               "Purrgression"
    IS_SYNTH                   TRUE
    NEEDS_MIDI_INPUT           TRUE
    NEEDS_MIDI_OUTPUT          TRUE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE
    VST3_CATEGORIES            Instrument Synth)

target_sources(Purrgression PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Purrgression PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    ${PURR_EXTRA_DEFINITIONS})

if(MSVC)
    target_compile_options(Purrgression PRIVATE /utf-8)   # Quelltext ist UTF-8 (Umlaute)
endif()

target_link_libraries(Purrgression
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
#  Tests (optional):  cmake -B build -DPURR_BUILD_TESTS=ON
# ------------------------------------------------------------------------------
if(PURR_BUILD_TESTS)
    add_executable(purr_engine_test tests/engine_test.cpp)
    target_include_directories(purr_engine_test PRIVATE Source)

    # Rendert das echte Overlay in PNG-Dateien und prueft den Prozessor
    add_executable(purr_snapshot tests/snapshot.cpp)
    target_link_libraries(purr_snapshot PRIVATE Purrgression)
    target_include_directories(purr_snapshot PRIVATE $<TARGET_PROPERTY:Purrgression,INCLUDE_DIRECTORIES>)
    target_compile_definitions(purr_snapshot PRIVATE $<TARGET_PROPERTY:Purrgression,COMPILE_DEFINITIONS>)
endif()
