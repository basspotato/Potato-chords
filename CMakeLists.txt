cmake_minimum_required(VERSION 3.22)

project(PotatoChords VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime so the VST3 runs on any Windows 11 machine without extra redistributables
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# JUCE: use a local checkout if JUCE_DIR is given, otherwise download it
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Identity is permanent after the first release: never change these codes
# VST3 has no MIDI-effect type, so this is an instrument with MIDI out that hosts route to other tracks
juce_add_plugin(PotatoChords
    COMPANY_NAME "Basspotato"
    COMPANY_WEBSITE "https://github.com/basspotato/potato-chords"
    BUNDLE_ID "com.basspotato.potatochords"
    PLUGIN_MANUFACTURER_CODE Bpot
    PLUGIN_CODE PtCh
    PRODUCT_NAME "Potato-Chords"
    DESCRIPTION "Chord sequencer"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Generator
    FORMATS VST3
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(PotatoChords PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(PotatoChords PRIVATE Source)

target_compile_definitions(PotatoChords PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# The GUI font (Barlow Condensed, SIL OFL, the same as potato-synth), embedded
juce_add_binary_data(PotatoChordsFonts SOURCES
    Resources/Fonts/BarlowCondensed-SemiBold.ttf
    Resources/Fonts/BarlowCondensed-Bold.ttf)

target_link_libraries(PotatoChords
    PRIVATE
        PotatoChordsFonts
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_audio_plugin_client
        juce::juce_gui_basics
        juce::juce_graphics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Headless developer tools (Linux cloud sessions): editor-to-PNG renderer and tests
option(POTATO_DEV_TOOLS "Build headless developer tools" OFF)
if(POTATO_DEV_TOOLS)
    # Reuses the plugin's shared-code library (which already contains the JUCE modules)
    foreach(tool PotatoChordsSnapshot PotatoChordsTests)
        if(tool STREQUAL "PotatoChordsSnapshot")
            add_executable(${tool} tools/Snapshot.cpp)
        else()
            add_executable(${tool} tests/Tests.cpp)
        endif()
        target_include_directories(${tool} PRIVATE $<TARGET_PROPERTY:PotatoChords,INCLUDE_DIRECTORIES>)
        target_compile_definitions(${tool} PRIVATE $<TARGET_PROPERTY:PotatoChords,COMPILE_DEFINITIONS>)
        target_compile_options(${tool} PRIVATE $<TARGET_PROPERTY:PotatoChords,COMPILE_OPTIONS>)
        target_link_libraries(${tool} PRIVATE PotatoChords PotatoChordsFonts)
    endforeach()

    enable_testing()
    add_test(NAME chords COMMAND PotatoChordsTests)
endif()
