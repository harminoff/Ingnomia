cmake_minimum_required(VERSION 3.16)
project(IngnomiaUiShellTests LANGUAGES CXX)

include(CTest)
find_package(Qt6 REQUIRED COMPONENTS Core)

get_filename_component(INGNOMIA_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

add_executable(ui_shell_controller
    controller_tests.cpp
    "${INGNOMIA_REPOSITORY_ROOT}/src/gui/ui/screens/shell/ShellController.cpp"
    "${INGNOMIA_REPOSITORY_ROOT}/src/gui/ui/screens/shell/ShellDataAdapter.cpp"
    "${INGNOMIA_REPOSITORY_ROOT}/src/gui/ui/screens/shell/ShellRmlAdapter.cpp"
)
target_compile_features(ui_shell_controller PRIVATE cxx_std_20)
target_include_directories(ui_shell_controller PRIVATE "${INGNOMIA_REPOSITORY_ROOT}/src")
target_link_libraries(ui_shell_controller PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(ui_shell_controller PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(ui_shell_controller PRIVATE -Wall -Wextra -Wpedantic -Werror)
    if(MINGW)
        # UiActionPayload is intentionally a large closed variant. GCC 11's COFF
        # debug names overflow the string table without big objects and debug stripping.
        target_compile_options(ui_shell_controller PRIVATE -Wa,-mbig-obj -g0 -O1)
        target_link_options(ui_shell_controller PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()

add_test(NAME ui_shell_controller COMMAND ui_shell_controller)
add_test(NAME ui_shell_rml_contract COMMAND "${CMAKE_COMMAND}"
    -P "${CMAKE_CURRENT_LIST_DIR}/verify-shell-rml.cmake")
add_test(NAME ui_shell_integration_contract COMMAND "${CMAKE_COMMAND}"
    -P "${CMAKE_CURRENT_LIST_DIR}/verify-shell-integration.cmake")
