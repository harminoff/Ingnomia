cmake_minimum_required(VERSION 3.24)
project(IngnomiaUiStage17 LANGUAGES CXX)
include(CTest)
find_package(Qt6 REQUIRED COMPONENTS Core Gui)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(INGNOMIA_RMLUI_BUILD_DIR "${ROOT}/build-wave8-root-msvc-link-priority2" CACHE PATH "Canonical package build")
add_executable(ui_Stage17 hud_tests.cpp
 "${ROOT}/src/gui/ui/screens/hud/HudRmlBinding.cpp"
 "${ROOT}/src/gui/ui/screens/hud/HudController.cpp"
 "${ROOT}/src/gui/ui/localization/UiText.cpp")
target_compile_features(ui_Stage17 PRIVATE cxx_std_20)
target_compile_definitions(ui_Stage17 PRIVATE RMLUI_STATIC_LIB NOMINMAX)
target_include_directories(ui_Stage17 PRIVATE "${ROOT}/src" "${INGNOMIA_RMLUI_BUILD_DIR}/_deps/rmlui-src/Include")
set_property(TARGET ui_Stage17 PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(ui_Stage17 PRIVATE Qt6::Core Qt6::Gui "${INGNOMIA_RMLUI_BUILD_DIR}/rmlui.lib" "${INGNOMIA_RMLUI_BUILD_DIR}/_deps/freetype-build/freetype.lib")
add_custom_command(TARGET ui_Stage17 POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Qt6::Core>" "$<TARGET_FILE:Qt6::Gui>" "$<TARGET_FILE_DIR:ui_Stage17>")
add_test(NAME ui_Stage17 COMMAND ui_Stage17 "${ROOT}/content/rmlui")

# HUD controller tests and contracts.
add_subdirectory("${ROOT}/tests/ui-hud" hud)
