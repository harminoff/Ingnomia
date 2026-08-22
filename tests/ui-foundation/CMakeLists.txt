cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(IngnomiaUiFoundationTests LANGUAGES CXX)
    include(CTest)
endif()

get_filename_component(INGNOMIA_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

function(add_ui_foundation_test target)
    add_executable(${target} ${ARGN})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE "${INGNOMIA_REPOSITORY_ROOT}/src")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_ui_foundation_test(ui_foundation_registry
    registry_tests.cpp
    "${INGNOMIA_REPOSITORY_ROOT}/src/gui/ui/state/UiRegistries.cpp"
    "${INGNOMIA_REPOSITORY_ROOT}/src/gui/ui/actions/UiActionRegistry.cpp"
)

add_ui_foundation_test(ui_foundation_navigation_store
    navigation_store_tests.cpp
    "${INGNOMIA_REPOSITORY_ROOT}/src/gui/ui/state/UiRegistries.cpp"
    "${INGNOMIA_REPOSITORY_ROOT}/src/gui/ui/state/UiNavigation.cpp"
    "${INGNOMIA_REPOSITORY_ROOT}/src/gui/ui/state/UiStore.cpp"
)

add_ui_foundation_test(ui_foundation_action_validation
    action_validation_tests.cpp
    "${INGNOMIA_REPOSITORY_ROOT}/src/gui/ui/state/UiRegistries.cpp"
    "${INGNOMIA_REPOSITORY_ROOT}/src/gui/ui/actions/UiActionRegistry.cpp"
)

add_ui_foundation_test(ui_foundation_release_registry
    release_registry_tests.cpp
    "${INGNOMIA_REPOSITORY_ROOT}/src/gui/ui/state/UiRegistries.cpp"
)

add_ui_foundation_test(ui_foundation_tutorial_types
    tutorial_types_tests.cpp
)

# These three targets exercise both public and development-only lookup paths.
# The production application defines this macro only for Debug configuration;
# release builds compile the development registrations out entirely.
target_compile_definitions(ui_foundation_registry PRIVATE INGNOMIA_DEVELOPER_UI=1)
target_compile_definitions(ui_foundation_navigation_store PRIVATE INGNOMIA_DEVELOPER_UI=1)
target_compile_definitions(ui_foundation_action_validation PRIVATE INGNOMIA_DEVELOPER_UI=1)

add_test(NAME ui_foundation_mainwindow_rmlui_wiring
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_DIR}/verify-mainwindow-rmlui-wiring.cmake"
)

add_test(NAME ui_foundation_tutorial_contract
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_DIR}/verify-tutorial-scenario.cmake"
)
