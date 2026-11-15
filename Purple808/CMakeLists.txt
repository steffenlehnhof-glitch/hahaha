cmake_minimum_required(VERSION 3.22)

# Muss vor project() stehen
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimale macOS-Version")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")   # keine VC-Redistributables noetig

project(Purple808 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------------------------
#  Optionen
# ------------------------------------------------------------------------------
set(P808_JUCE_PATH "" CACHE PATH "Lokaler JUCE-Ordner (leer = JUCE wird automatisch heruntergeladen)")
set(P808_JUCE_TAG "8.0.8" CACHE STRING "JUCE-Version, die heruntergeladen wird")
set(P808_FORMATS VST3 Standalone CACHE STRING "Plugin-Formate (VST3, Standalone, AU ...)")
set(P808_EXTRA_DEFINITIONS "" CACHE STRING "Zusaetzliche Compiler-Definitionen")
option(P808_BUILD_TESTS "Test-Programme mitbauen" OFF)

# ------------------------------------------------------------------------------
#  JUCE
# ------------------------------------------------------------------------------
if(P808_JUCE_PATH)
    add_subdirectory("${P808_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${P808_JUCE_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ------------------------------------------------------------------------------
#  Plugin
# ------------------------------------------------------------------------------
juce_add_plugin(Purple808
    COMPANY_NAME               "Purple 808"
    BUNDLE_ID                  "com.purple808.plugin"
    PLUGIN_MANUFACTURER_CODE   Purr
    PLUGIN_CODE                P808
    FORMATS                    ${P808_FORMATS}
    PRODUCT_NAME               "Purple 808"
    IS_SYNTH                   TRUE
    NEEDS_MIDI_INPUT           TRUE
    NEEDS_MIDI_OUTPUT          TRUE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE
    VST3_CATEGORIES            Instrument Synth)

target_sources(Purple808 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Purple808 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    ${P808_EXTRA_DEFINITIONS})

if(MSVC)
    target_compile_options(Purple808 PRIVATE /utf-8)   # Quelltext ist UTF-8 (Umlaute)
endif()

target_link_libraries(Purple808
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
#  Tests (optional):  cmake -B build -DP808_BUILD_TESTS=ON
# ------------------------------------------------------------------------------
if(P808_BUILD_TESTS)
    add_executable(p808_engine_test tests/engine_test.cpp)
    target_include_directories(p808_engine_test PRIVATE Source)

    # Rendert das echte Overlay in PNG-Dateien und prueft den Prozessor
    add_executable(p808_snapshot tests/snapshot.cpp)
    target_link_libraries(p808_snapshot PRIVATE Purple808)
    target_include_directories(p808_snapshot PRIVATE $<TARGET_PROPERTY:Purple808,INCLUDE_DIRECTORIES>)
    target_compile_definitions(p808_snapshot PRIVATE $<TARGET_PROPERTY:Purple808,COMPILE_DEFINITIONS>)
endif()
