# Included by AzerothCore's module configuration to register telemetry integration tests.

if(BUILD_TESTING)
  set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/tests/cpp/PlayerbotInspectorTest.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/tests/cpp/PlayerbotTelemetryTest.cpp")
  set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_INCLUDES
    "${CMAKE_CURRENT_LIST_DIR}/src")
endif()
