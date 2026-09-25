cmake_minimum_required(VERSION 3.24)
project(IngnomiaUiStage18 LANGUAGES CXX)
include(CTest)
find_package(Qt6 REQUIRED COMPONENTS Core Gui)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(INGNOMIA_RMLUI_BUILD_DIR "${ROOT}/build-wave8-root-msvc-link-priority2" CACHE PATH "Canonical package build")
add_executable(ui_Stage18 shell_tests.cpp
 "${ROOT}/src/gui/ui/runtime/RmlUiQtInputAdapter.cpp"
 "${ROOT}/src/gui/ui/screens/shell/ShellRmlBinding.cpp"
 "${ROOT}/src/gui/ui/screens/shell/ShellController.cpp"
 "${ROOT}/src/gui/ui/screens/shell/ShellRmlAdapter.cpp"
 "${ROOT}/src/gui/ui/state/UiRegistries.cpp")
target_compile_features(ui_Stage18 PRIVATE cxx_std_20)
target_compile_definitions(ui_Stage18 PRIVATE RMLUI_STATIC_LIB NOMINMAX)
target_include_directories(ui_Stage18 PRIVATE "${ROOT}/src" "${INGNOMIA_RMLUI_BUILD_DIR}/_deps/rmlui-src/Include")
set_property(TARGET ui_Stage18 PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(ui_Stage18 PRIVATE Qt6::Core Qt6::Gui "${INGNOMIA_RMLUI_BUILD_DIR}/rmlui.lib" "${INGNOMIA_RMLUI_BUILD_DIR}/_deps/freetype-build/freetype.lib")
add_custom_command(TARGET ui_Stage18 POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Qt6::Core>" "$<TARGET_FILE:Qt6::Gui>" "$<TARGET_FILE_DIR:ui_Stage18>")
add_test(NAME ui_Stage18 COMMAND ui_Stage18 "${ROOT}/content/rmlui")

# Shell contracts and the Stage 04/05 input tests that use the shell screens.
add_subdirectory("${ROOT}/tests/ui-shell" shell)
