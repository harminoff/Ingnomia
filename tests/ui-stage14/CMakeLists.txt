cmake_minimum_required(VERSION 3.24)
project(IngnomiaUiStage14 LANGUAGES CXX)
include(CTest)
find_package(Qt6 REQUIRED COMPONENTS Core Gui)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(INGNOMIA_RMLUI_BUILD_DIR "${ROOT}/build-wave8-root-msvc-link-priority2" CACHE PATH "Canonical package build")
add_executable(ui_Stage14 military_tests.cpp
 "${ROOT}/src/gui/ui/runtime/RmlUiQtInputAdapter.cpp"
 "${ROOT}/src/gui/ui/screens/management6c/Management6CRmlBinding.cpp"
 "${ROOT}/src/gui/ui/screens/management6c/Management6CController.cpp"
 "${ROOT}/src/gui/ui/screens/shell/ShellRmlBinding.cpp"
 "${ROOT}/src/gui/ui/screens/shell/ShellController.cpp"
 "${ROOT}/src/gui/ui/screens/shell/ShellRmlAdapter.cpp"
 "${ROOT}/src/gui/ui/state/UiRegistries.cpp"
 "${ROOT}/src/gui/ui/screens/management6a/Management6ARmlBinding.cpp"
 "${ROOT}/src/gui/ui/screens/management6a/Management6AController.cpp")
target_compile_features(ui_Stage14 PRIVATE cxx_std_20)
target_compile_definitions(ui_Stage14 PRIVATE RMLUI_STATIC_LIB NOMINMAX)
target_include_directories(ui_Stage14 PRIVATE "${ROOT}/src" "${INGNOMIA_RMLUI_BUILD_DIR}/_deps/rmlui-src/Include")
set_property(TARGET ui_Stage14 PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(ui_Stage14 PRIVATE Qt6::Core Qt6::Gui "${INGNOMIA_RMLUI_BUILD_DIR}/rmlui.lib" "${INGNOMIA_RMLUI_BUILD_DIR}/_deps/freetype-build/freetype.lib")
add_custom_command(TARGET ui_Stage14 POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Qt6::Core>" "$<TARGET_FILE:Qt6::Gui>" "$<TARGET_FILE_DIR:ui_Stage14>")
add_test(NAME ui_Stage14 COMMAND ui_Stage14 "${ROOT}/content/rmlui")

add_subdirectory("${ROOT}/tests/ui-shell" shell)

add_subdirectory("${ROOT}/tests/ui-management6a" management6a)

add_subdirectory("${ROOT}/tests/ui-management6b" management6b)
# Management 6C controller and contracts (the stale 6C layout test is left out; Stage 21).
add_executable(ui_management6c_controller "${ROOT}/tests/ui-management6c/controller_tests.cpp" "${ROOT}/src/gui/ui/screens/management6c/Management6CController.cpp")
target_compile_features(ui_management6c_controller PRIVATE cxx_std_20)
target_include_directories(ui_management6c_controller PRIVATE "${ROOT}/src")
add_test(NAME ui_management6c_controller COMMAND ui_management6c_controller)
add_test(NAME ui_management6c_rml_contract COMMAND "${CMAKE_COMMAND}" -P "${ROOT}/tests/ui-management6c/verify-management6c-rml.cmake")
add_test(NAME ui_management6c_integration_contract COMMAND "${CMAKE_COMMAND}" -P "${ROOT}/tests/ui-management6c/verify-management6c-integration.cmake")
