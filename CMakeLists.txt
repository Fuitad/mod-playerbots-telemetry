if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.20)
  project(mod_playerbots_telemetry LANGUAGES CXX)

  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  include(CTest)

  add_executable(playerbots_telemetry_tests
    tests/cpp/PlayerbotTelemetryStandaloneTest.cpp
    src/Bot/Telemetry/PlayerbotTelemetryState.cpp
    src/Bot/Telemetry/PlayerbotVerificationState.cpp)
  target_include_directories(playerbots_telemetry_tests PRIVATE
    tests/standalone
    src)
  target_compile_options(playerbots_telemetry_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME playerbots_telemetry_tests COMMAND playerbots_telemetry_tests)
else()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-playerbots/src/Bot/PlayerbotAI.h")
    message(FATAL_ERROR "mod-playerbots-telemetry requires mod-playerbots")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-playerbots-personality/src/Bot/Personality/PlayerbotPersonality.h")
    message(FATAL_ERROR "mod-playerbots-telemetry requires mod-playerbots-personality")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-playerbots-economy/src/Bot/Economy/PlayerbotEconomyTelemetry.h")
    message(FATAL_ERROR "mod-playerbots-telemetry requires mod-playerbots-economy")
  endif()

  AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src")

  if(BUILD_TESTING)
    set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_SOURCES
      "${CMAKE_CURRENT_LIST_DIR}/tests/cpp/PlayerbotInspectorTest.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/tests/cpp/PlayerbotTelemetryTest.cpp")
    set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_INCLUDES
      "${CMAKE_CURRENT_LIST_DIR}/src")
  endif()
endif()
