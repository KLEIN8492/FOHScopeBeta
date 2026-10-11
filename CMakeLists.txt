cmake_minimum_required(VERSION 3.22)
# Must be set before project() or CMake ignores it.
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")
project(FOHScope VERSION 0.2.0 LANGUAGES C CXX)

# Windows: link the C++ runtime statically, so FOHScope.exe runs on a fresh Windows without the
# VC++ redistributable. Set before any target (JUCE / Catch2 included).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Reuse the JUCE / Catch2 checkouts already on this Mac (LiveRack, VenueTuner) when present;
# otherwise FetchContent downloads them.
include(FetchContent)
set(_juce_local ${CMAKE_CURRENT_SOURCE_DIR}/../LiveRack/External/JUCE)
set(_catch_local ${CMAKE_CURRENT_SOURCE_DIR}/../VenueTuner/engine/build/_deps/catch2-src)
if(EXISTS ${_juce_local}/CMakeLists.txt AND NOT FETCHCONTENT_SOURCE_DIR_JUCE)
  set(FETCHCONTENT_SOURCE_DIR_JUCE ${_juce_local})
endif()
if(EXISTS ${_catch_local}/CMakeLists.txt AND NOT FETCHCONTENT_SOURCE_DIR_CATCH2)
  set(FETCHCONTENT_SOURCE_DIR_CATCH2 ${_catch_local})
endif()
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.15 GIT_SHALLOW TRUE)
FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.7.1 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE Catch2)

# ---- fs_core: DSP + analysis, no UI. Shared by the app and the tests. ----
file(GLOB FS_CORE_SOURCES CONFIGURE_DEPENDS Source/dsp/*.cpp Source/core/*.cpp)
add_library(fs_core STATIC ${FS_CORE_SOURCES})
target_include_directories(fs_core PUBLIC Source)
target_compile_definitions(fs_core PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_STANDALONE_APPLICATION=1
  JUCE_USE_FLAC=0
  JUCE_USE_OGGVORBIS=0
  JUCE_APPLICATION_NAME_STRING="FOHScope"
  JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
# Audio drivers per OS (DeviceHub opens every type that is compiled in).
if(WIN32)
  # ASIO uses the Steinberg headers bundled with JUCE 8 (ASIO licence or GPLv3; FOHScope is AGPLv3).
  target_compile_definitions(fs_core PUBLIC JUCE_ASIO=1 JUCE_WASAPI=1 JUCE_DIRECTSOUND=0 _USE_MATH_DEFINES NOMINMAX)
  if(MSVC)
    target_compile_options(fs_core PUBLIC /utf-8 /bigobj)
  endif()
elseif(UNIX AND NOT APPLE)
  target_compile_definitions(fs_core PUBLIC JUCE_ALSA=1 JUCE_JACK=1)
endif()

add_library(fs_warning_tweaks INTERFACE)
if(NOT MSVC)
  target_compile_options(fs_warning_tweaks INTERFACE -Wno-float-equal) # GCC / Clang only
endif()
# Interface font for Windows / Linux (macOS uses Menlo): JetBrains Mono, SIL OFL 1.1.
juce_add_binary_data(fs_fonts NAMESPACE FontData HEADER_NAME FontData.h SOURCES
  Resources/fonts/JetBrainsMono-Regular.ttf
  Resources/fonts/JetBrainsMono-Bold.ttf)
set_target_properties(fs_fonts PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(fs_core PUBLIC fs_fonts)

# JUCE modules are compiled once, into fs_core (PRIVATE); dependents get headers + defines only.
target_link_libraries(fs_core
  PRIVATE juce::juce_audio_utils juce::juce_audio_devices juce::juce_dsp juce::juce_gui_extra
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags fs_warning_tweaks)
target_include_directories(fs_core INTERFACE $<TARGET_PROPERTY:fs_core,INCLUDE_DIRECTORIES>)
target_compile_definitions(fs_core INTERFACE $<TARGET_PROPERTY:fs_core,COMPILE_DEFINITIONS>)

# ---- app ----
juce_add_gui_app(FOHScope
  PRODUCT_NAME "FOHScope"
  BUNDLE_ID "com.thomaschaudon.fohscope"
  COMPANY_NAME "FOHScope"
  VERSION ${PROJECT_VERSION}
  ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/AppIcon.png"
  MICROPHONE_PERMISSION_ENABLED TRUE
  HARDENED_RUNTIME_ENABLED FALSE
  MICROPHONE_PERMISSION_TEXT "FOHScope analyses the audio inputs of your interface. It never records or sends audio anywhere.")
file(GLOB FS_APP_SOURCES CONFIGURE_DEPENDS Source/ui/*.cpp Source/App/*.cpp)
target_sources(FOHScope PRIVATE ${FS_APP_SOURCES})
target_link_libraries(FOHScope PRIVATE fs_core)

# ---- tests ----
enable_testing()
file(GLOB FS_TEST_SOURCES CONFIGURE_DEPENDS Tests/*.cpp)
add_executable(FOHScopeTests ${FS_TEST_SOURCES})
target_include_directories(FOHScopeTests PRIVATE Tests)
target_link_libraries(FOHScopeTests PRIVATE fs_core Catch2::Catch2WithMain)

# ---- test signal source (plays into BlackHole for testing without hardware) ----
add_executable(FOHScopeSignalGen Tools/SignalGen.cpp)
target_include_directories(FOHScopeSignalGen PRIVATE Tests)
target_link_libraries(FOHScopeSignalGen PRIVATE fs_core)
