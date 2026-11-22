cmake_minimum_required(VERSION 3.22)

project(AutoMech VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# Link the MSVC runtime statically so the plugin loads on PCs without the
# Visual C++ Redistributable installed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(AUTOMECH_BUILD_TOOLS "Build the offline DSP test and the GUI snapshot tool" OFF)
set(AUTOMECH_JUCE_DIR "" CACHE PATH "Optional path to a local JUCE checkout (skips the download)")

# ---------------------------------------------------------------- JUCE
if(AUTOMECH_JUCE_DIR)
    add_subdirectory(${AUTOMECH_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------- Plugin
set(AUTOMECH_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND AUTOMECH_FORMATS AU)
endif()

juce_add_plugin(AutoMech
    COMPANY_NAME                "Sirhoz AutoTune+"
    COMPANY_WEBSITE             ""
    PLUGIN_MANUFACTURER_CODE    Sirh
    PLUGIN_CODE                 Amch
    PRODUCT_NAME                "AutoMech"
    DESCRIPTION                 "Real-time vocal pitch corrector with mechanical voice mode"
    FORMATS                     ${AUTOMECH_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Pitch Shift
    AU_MAIN_TYPE                kAudioUnitType_MusicEffect)

set(AUTOMECH_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/PitchDetector.cpp
    Source/DSP/PsolaShifter.cpp
    Source/UI/AutoMechLookAndFeel.cpp
    Source/UI/PitchDisplay.cpp
    Source/UI/PixelVisualizer.cpp
    Source/UI/PianoKeyboard.cpp
    Source/UI/Controls.cpp)

target_sources(AutoMech PRIVATE ${AUTOMECH_SOURCES})
target_include_directories(AutoMech PRIVATE Source)

set(AUTOMECH_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_definitions(AutoMech PUBLIC ${AUTOMECH_DEFINITIONS})

target_link_libraries(AutoMech
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- Tools (optional)
if(AUTOMECH_BUILD_TOOLS)
    juce_add_console_app(AutoMechTools PRODUCT_NAME "AutoMechTools")
    target_sources(AutoMechTools PRIVATE Tools/AutoMechTools.cpp ${AUTOMECH_SOURCES})
    target_include_directories(AutoMechTools PRIVATE Source)
    target_compile_definitions(AutoMechTools PRIVATE ${AUTOMECH_DEFINITIONS} AUTOMECH_TOOLS=1)
    target_link_libraries(AutoMechTools
        PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC  juce::juce_recommended_config_flags)
endif()
