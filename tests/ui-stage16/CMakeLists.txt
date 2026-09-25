cmake_minimum_required(VERSION 3.24)
project(IngnomiaUiStage16 LANGUAGES CXX)
include(CTest)
find_package(Qt6 REQUIRED COMPONENTS Core Gui)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(INGNOMIA_RMLUI_BUILD_DIR "${ROOT}/build-wave8-root-msvc-link-priority2" CACHE PATH "Canonical package build")
add_executable(ui_Stage16 inspector_tests.cpp
 "${ROOT}/src/gui/ui/screens/inspector/InspectorRmlBinding.cpp"
 "${ROOT}/src/gui/ui/screens/inspector/InspectorController.cpp"
 "${ROOT}/src/gui/ui/localization/UiText.cpp")
target_compile_features(ui_Stage16 PRIVATE cxx_std_20)
target_compile_definitions(ui_Stage16 PRIVATE RMLUI_STATIC_LIB NOMINMAX)
target_include_directories(ui_Stage16 PRIVATE "${ROOT}/src" "${INGNOMIA_RMLUI_BUILD_DIR}/_deps/rmlui-src/Include")
set_property(TARGET ui_Stage16 PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(ui_Stage16 PRIVATE Qt6::Core Qt6::Gui "${INGNOMIA_RMLUI_BUILD_DIR}/rmlui.lib" "${INGNOMIA_RMLUI_BUILD_DIR}/_deps/freetype-build/freetype.lib")
add_custom_command(TARGET ui_Stage16 POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Qt6::Core>" "$<TARGET_FILE:Qt6::Gui>" "$<TARGET_FILE_DIR:ui_Stage16>")
add_test(NAME ui_Stage16 COMMAND ui_Stage16 "${ROOT}/content/rmlui")

# Inspector contracts and controller tests, and the population sheet that now opens the inspector (POP-05).
add_subdirectory("${ROOT}/tests/ui-inspector" inspector)
add_subdirectory("${ROOT}/tests/ui-management6b" management6b)
