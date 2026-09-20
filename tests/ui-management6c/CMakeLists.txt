cmake_minimum_required(VERSION 3.16)
project(IngnomiaUiManagement6CTests LANGUAGES CXX)

include(CTest)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

add_executable(ui_management6c_controller
  controller_tests.cpp
  "${ROOT}/src/gui/ui/screens/management6c/Management6CController.cpp")
target_compile_features(ui_management6c_controller PRIVATE cxx_std_20)
target_include_directories(ui_management6c_controller PRIVATE "${ROOT}/src")
if(MSVC)
  target_compile_options(ui_management6c_controller PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(ui_management6c_controller PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME ui_management6c_controller COMMAND ui_management6c_controller)
add_test(NAME ui_management6c_rml_contract
  COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_DIR}/verify-management6c-rml.cmake")
add_test(NAME ui_management6c_integration_contract
  COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_DIR}/verify-management6c-integration.cmake")


# Optional real-RmlUi tests reuse the library from the canonical Windows package build.
set(INGNOMIA_RMLUI_BUILD_DIR "" CACHE PATH "Existing Ingnomia build containing the pinned RmlUi and FreeType libraries")
if(WIN32 AND INGNOMIA_RMLUI_BUILD_DIR)
  find_package(Qt6 REQUIRED COMPONENTS Core)
  add_executable(ui_management6c_rml_layout rml_layout_tests.cpp
    "${ROOT}/src/gui/ui/screens/management6b/Management6BRmlBinding.cpp"
    "${ROOT}/src/gui/ui/screens/management6b/Management6BController.cpp"
    "${ROOT}/src/gui/ui/screens/management6c/Management6CRmlBinding.cpp"
    "${ROOT}/src/gui/ui/screens/management6c/Management6CController.cpp"
    "${ROOT}/src/gui/ui/screens/hud/HudRmlBinding.cpp"
    "${ROOT}/src/gui/ui/screens/hud/HudController.cpp")
  target_compile_features(ui_management6c_rml_layout PRIVATE cxx_std_20)
  target_compile_definitions(ui_management6c_rml_layout PRIVATE RMLUI_STATIC_LIB NOMINMAX _CRT_SECURE_NO_WARNINGS)
  target_include_directories(ui_management6c_rml_layout PRIVATE "${ROOT}/src" "${INGNOMIA_RMLUI_BUILD_DIR}/_deps/rmlui-src/Include")
  set_property(TARGET ui_management6c_rml_layout PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_link_libraries(ui_management6c_rml_layout PRIVATE
    Qt6::Core "${INGNOMIA_RMLUI_BUILD_DIR}/rmlui.lib" "${INGNOMIA_RMLUI_BUILD_DIR}/_deps/freetype-build/freetype.lib")
  add_test(NAME ui_management6c_rml_layout COMMAND ui_management6c_rml_layout "${ROOT}/content/rmlui")
  set_tests_properties(ui_management6c_rml_layout PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
endif()
