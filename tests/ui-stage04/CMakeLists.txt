cmake_minimum_required(VERSION 3.24)
project(IngnomiaUiStage04 LANGUAGES CXX)
include(CTest)
find_package(Qt6 REQUIRED COMPONENTS Core Gui)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(INGNOMIA_RMLUI_BUILD_DIR "${ROOT}/build-wave8-root-msvc-link-priority2" CACHE PATH "Canonical package build")
add_executable(ui_stage04 interaction_tests.cpp
 "${ROOT}/src/gui/ui/runtime/RmlUiQtInputAdapter.cpp"
 "${ROOT}/src/gui/ui/screens/shell/ShellRmlBinding.cpp"
 "${ROOT}/src/gui/ui/screens/shell/ShellController.cpp"
 "${ROOT}/src/gui/ui/screens/shell/ShellRmlAdapter.cpp"
 "${ROOT}/src/gui/ui/state/UiRegistries.cpp")
target_compile_features(ui_stage04 PRIVATE cxx_std_20)
target_compile_definitions(ui_stage04 PRIVATE RMLUI_STATIC_LIB NOMINMAX)
target_include_directories(ui_stage04 PRIVATE "${ROOT}/src" "${INGNOMIA_RMLUI_BUILD_DIR}/_deps/rmlui-src/Include")
set_property(TARGET ui_stage04 PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(ui_stage04 PRIVATE Qt6::Core Qt6::Gui "${INGNOMIA_RMLUI_BUILD_DIR}/rmlui.lib" "${INGNOMIA_RMLUI_BUILD_DIR}/_deps/freetype-build/freetype.lib")
add_custom_command(TARGET ui_stage04 POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Qt6::Core>" "$<TARGET_FILE:Qt6::Gui>" "$<TARGET_FILE_DIR:ui_stage04>")
add_test(NAME ui_stage04 COMMAND ui_stage04 "${ROOT}/content/rmlui")

add_subdirectory("${ROOT}/tests/ui-shell" shell)
