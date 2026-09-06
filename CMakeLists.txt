cmake_minimum_required(VERSION 3.22)

# macOS universal binary (x86_64 + arm64).
# These MUST be set before project() - project() creates the cache entries itself,
# and a later set(... CACHE ...) without FORCE would then be a silent no-op.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS build architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(krain VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Global build settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json so CLion / clangd see the exact flags.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(KRAIN_BUILD_TESTS "Build the Catch2 unit tests" ON)
option(KRAIN_COPY_PLUGIN_AFTER_BUILD "Install the built plug-ins into the user plug-in folders" OFF)

# Warning flags applied only to *our* targets - JUCE's own sources are compiled
# by JUCE's own targets and are not affected by this.
add_library(krain_warnings INTERFACE)
if(MSVC)
    target_compile_options(krain_warnings INTERFACE /W4)
else()
    target_compile_options(krain_warnings INTERFACE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# DSP core - a plain static library so the unit tests can link the DSP
# without dragging in the whole plug-in wrapper.
# ---------------------------------------------------------------------------
add_library(KrainDsp STATIC
    Source/dsp/DelayBuffer.cpp
    Source/dsp/GrainEngine.cpp)

target_include_directories(KrainDsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_link_libraries(KrainDsp
    PUBLIC
        juce::juce_audio_basics
    PRIVATE
        krain_warnings
        juce::juce_recommended_config_flags)

# The plug-in bundles are shared libraries, so everything linked into them must be
# position independent code.
set_target_properties(KrainDsp PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# The plug-in
# ---------------------------------------------------------------------------
juce_add_plugin(krain
    COMPANY_NAME            "Herhoffer"
    BUNDLE_ID               "net.herhoffer.krain"
    PLUGIN_MANUFACTURER_CODE Hrhf
    PLUGIN_CODE             Krai
    FORMATS                 AU VST3 Standalone
    PRODUCT_NAME            "krain"
    DESCRIPTION             "Creative granular delay"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${KRAIN_COPY_PLUGIN_AFTER_BUILD}
    VST3_CATEGORIES         Fx Delay
    AU_MAIN_TYPE            kAudioUnitType_Effect)

target_sources(krain PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/gui/ParameterComponents.cpp
    Source/gui/KrainLookAndFeel.cpp
    Source/gui/GrainCloudView.cpp)

target_compile_definitions(krain PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(krain
    PRIVATE
        KrainDsp
        krain_warnings
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Developer tools: offline renderer + the A/B compare app
# ---------------------------------------------------------------------------
add_subdirectory(tools)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(KRAIN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
