cmake_minimum_required(VERSION 3.24)
project(IngnomiaUiStage12 LANGUAGES CXX)
include(CTest)
find_package(Qt6 REQUIRED COMPONENTS Core Gui)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(INGNOMIA_RMLUI_BUILD_DIR "${ROOT}/build-wave8-root-msvc-link-priority2" CACHE PATH "Canonical package build")
add_executable(ui_Stage12 population_tests.cpp
 "${ROOT}/src/gui/ui/runtime/RmlUiQtInputAdapter.cpp"
 "${ROOT}/src/gui/ui/screens/management6b/Management6BRmlBinding.cpp"
 "${ROOT}/src/gui/ui/screens/management6b/Management6BController.cpp"
 "${ROOT}/src/gui/ui/screens/shell/ShellRmlBinding.cpp"
 "${ROOT}/src/gui/ui/screens/shell/ShellController.cpp"
 "${ROOT}/src/gui/ui/screens/shell/ShellRmlAdapter.cpp"
 "${ROOT}/src/gui/ui/state/UiRegistries.cpp"
 "${ROOT}/src/gui/ui/screens/management6a/Management6ARmlBinding.cpp"
 "${ROOT}/src/gui/ui/screens/management6a/Management6AController.cpp")
target_compile_features(ui_Stage12 PRIVATE cxx_std_20)
target_compile_definitions(ui_Stage12 PRIVATE RMLUI_STATIC_LIB NOMINMAX)
target_include_directories(ui_Stage12 PRIVATE "${ROOT}/src" "${INGNOMIA_RMLUI_BUILD_DIR}/_deps/rmlui-src/Include")
set_property(TARGET ui_Stage12 PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(ui_Stage12 PRIVATE Qt6::Core Qt6::Gui "${INGNOMIA_RMLUI_BUILD_DIR}/rmlui.lib" "${INGNOMIA_RMLUI_BUILD_DIR}/_deps/freetype-build/freetype.lib")
add_custom_command(TARGET ui_Stage12 POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Qt6::Core>" "$<TARGET_FILE:Qt6::Gui>" "$<TARGET_FILE_DIR:ui_Stage12>")
add_test(NAME ui_Stage12 COMMAND ui_Stage12 "${ROOT}/content/rmlui")

add_subdirectory("${ROOT}/tests/ui-shell" shell)

add_subdirectory("${ROOT}/tests/ui-management6a" management6a)

add_subdirectory("${ROOT}/tests/ui-management6b" management6b)
