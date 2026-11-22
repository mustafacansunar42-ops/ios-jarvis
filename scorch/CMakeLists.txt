cmake_minimum_required(VERSION 3.22)

project(SCORCH VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows: link the C/C++ runtime statically, so the plugin loads even on
# machines without (the right version of) the Visual C++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ------------------------------------------------------------------------------
# Identity. The developer name lives in exactly one place: this variable.
# It reaches C++ through JUCE's generated JucePlugin_Manufacturer macro,
# which Source/Identity.h wraps.
# ------------------------------------------------------------------------------
set(SCORCH_MANUFACTURER "Sirhoz Industries" CACHE STRING "Developer / manufacturer name")

# ------------------------------------------------------------------------------
# JUCE 8 via FetchContent.
# To use a local checkout instead of downloading:
#   cmake -B build -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
# ------------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SCORCH
    COMPANY_NAME                "${SCORCH_MANUFACTURER}"
    BUNDLE_ID                   com.sirhozindustries.scorch
    PLUGIN_MANUFACTURER_CODE    Sirh
    PLUGIN_CODE                 Scor
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "SCORCH"
    DESCRIPTION                 "Saturation and distortion"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Distortion)

target_sources(SCORCH PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/dsp/SaturationEngine.cpp
    Source/dsp/Oversampler.cpp
    Source/gui/ScorchLookAndFeel.cpp
    Source/gui/Knob.cpp
    Source/gui/LevelMeter.cpp
    Source/gui/PixelVisualizer.cpp)

target_include_directories(SCORCH PRIVATE Source)

target_compile_definitions(SCORCH PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(SCORCH
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
# Optional offline tests + GUI snapshots (not needed to build the plugin).
#   cmake -B build -DSCORCH_BUILD_TESTS=ON && cmake --build build --target ScorchTests
# ------------------------------------------------------------------------------
option(SCORCH_BUILD_TESTS "Build the ScorchTests console app" OFF)

if(SCORCH_BUILD_TESTS)
    # the snapshot test pumps the message loop while audio runs
    target_compile_definitions(SCORCH PUBLIC JUCE_MODAL_LOOPS_PERMITTED=1)

    add_executable(ScorchTests Tests/ScorchTests.cpp)
    target_include_directories(ScorchTests PRIVATE $<TARGET_PROPERTY:SCORCH,INCLUDE_DIRECTORIES>)
    target_compile_definitions(ScorchTests PRIVATE $<TARGET_PROPERTY:SCORCH,COMPILE_DEFINITIONS>)
    target_link_libraries(ScorchTests PRIVATE SCORCH)
endif()
