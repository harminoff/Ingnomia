cmake_minimum_required(VERSION 3.24)

project(IngnomiaRmlUiQtSpike LANGUAGES C CXX)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui)
find_package(OpenGL REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/RmlUiDependencies.cmake")

add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../../3rdparty/glad"
    "${CMAKE_CURRENT_BINARY_DIR}/glad"
)

add_library(ingnomia_rmlui_gl3 STATIC
    "${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp"
    "${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.h"
    src/rmlui_gl_loader.h
)
target_include_directories(ingnomia_rmlui_gl3
    PUBLIC
        "${rmlui_SOURCE_DIR}/Backends"
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src"
)
target_compile_definitions(ingnomia_rmlui_gl3
    PRIVATE
        RMLUI_GL3_CUSTOM_LOADER="rmlui_gl_loader.h"
)
target_link_libraries(ingnomia_rmlui_gl3
    PUBLIC
        RmlUi::Core
    PRIVATE
        glad
)

add_executable(ingnomia_rmlui_qt_spike
    src/main.cpp
    ../../src/gui/ui/runtime/IngnomiaRmlUiRenderer.cpp
    ../../src/gui/ui/runtime/IngnomiaRmlUiRenderer.h
    ../../src/gui/ui/runtime/QtRmlSystemInterface.cpp
    ../../src/gui/ui/runtime/QtRmlSystemInterface.h
    ../../src/gui/ui/runtime/QtRmlFileInterface.cpp
    ../../src/gui/ui/runtime/QtRmlFileInterface.h
    ../../src/gui/ui/runtime/RmlUiQtInputAdapter.cpp
    ../../src/gui/ui/runtime/RmlUiQtInputAdapter.h
    ../../src/gui/ui/runtime/RmlUiHost.cpp
    ../../src/gui/ui/runtime/RmlUiHost.h
    ../../src/gui/ui/screens/shell/ShellController.cpp
    ../../src/gui/ui/screens/shell/ShellController.h
    ../../src/gui/ui/screens/shell/ShellDataAdapter.cpp
    ../../src/gui/ui/screens/shell/ShellDataAdapter.h
    ../../src/gui/ui/screens/shell/ShellRmlAdapter.cpp
    ../../src/gui/ui/screens/shell/ShellRmlAdapter.h
    ../../src/gui/ui/screens/shell/ShellRmlBinding.cpp
    ../../src/gui/ui/screens/shell/ShellRmlBinding.h
    ../../src/gui/ui/screens/shell/ShellState.h
    ../../src/gui/ui/screens/hud/HudController.cpp
    ../../src/gui/ui/screens/hud/HudController.h
    ../../src/gui/ui/screens/hud/HudRmlBinding.cpp
    ../../src/gui/ui/screens/hud/HudRmlBinding.h
    ../../src/gui/ui/screens/inspector/InspectorController.cpp
    ../../src/gui/ui/screens/inspector/InspectorController.h
    ../../src/gui/ui/screens/inspector/InspectorRmlBinding.cpp
    ../../src/gui/ui/screens/inspector/InspectorRmlBinding.h
    ../../src/gui/ui/screens/inspector/InspectorState.h
    ../../src/gui/ui/screens/management6b/Management6BController.cpp
    ../../src/gui/ui/screens/management6b/Management6BController.h
    ../../src/gui/ui/screens/management6b/Management6BRmlBinding.cpp
    ../../src/gui/ui/screens/management6b/Management6BRmlBinding.h
    ../../src/gui/ui/screens/management6b/Management6BState.h
    ../../src/gui/ui/screens/management6a/Management6AController.cpp
    ../../src/gui/ui/screens/management6a/Management6AController.h
    ../../src/gui/ui/screens/management6a/Management6ARmlBinding.cpp
    ../../src/gui/ui/screens/management6a/Management6ARmlBinding.h
    ../../src/gui/ui/screens/management6a/Management6AState.h
    ../../src/gui/ui/screens/management6c/Management6CController.cpp
    ../../src/gui/ui/screens/management6c/Management6CController.h
    ../../src/gui/ui/screens/management6c/Management6CDomWindow.h
    ../../src/gui/ui/screens/management6c/Management6CRmlBinding.cpp
    ../../src/gui/ui/screens/management6c/Management6CRmlBinding.h
    ../../src/gui/ui/screens/management6c/Management6CState.h
    ../../src/gui/ui/screens/developer_ui/DebugController.cpp
    ../../src/gui/ui/screens/developer_ui/DebugController.h
    ../../src/gui/ui/screens/developer_ui/DebugRmlBinding.cpp
    ../../src/gui/ui/screens/developer_ui/DebugRmlBinding.h
    ../../src/gui/ui/screens/developer_ui/DebugState.h
    ../../src/gui/ui/localization/UiText.cpp
    ../../src/gui/ui/localization/UiText.h
    ../../src/gui/ui/state/UiRegistries.cpp
    ../../src/gui/ui/state/UiRegistries.h
)

add_library(ingnomia_shell_qt_port_compile OBJECT
    ../../src/gui/ui/controllers/shell/ShellQtCommandPort.cpp
    ../../src/gui/ui/controllers/shell/ShellQtCommandPort.h
)
target_compile_features(ingnomia_shell_qt_port_compile PRIVATE cxx_std_20)
target_include_directories(ingnomia_shell_qt_port_compile PRIVATE ../../src)
target_link_libraries(ingnomia_shell_qt_port_compile PRIVATE Qt6::Core)
target_compile_features(ingnomia_rmlui_qt_spike PRIVATE cxx_std_20)
target_compile_definitions(ingnomia_rmlui_qt_spike PRIVATE INGNOMIA_DEVELOPER_UI=1)
target_include_directories(ingnomia_rmlui_qt_spike PRIVATE
    ../../src/gui/ui/runtime
    ../../src
)
target_link_libraries(ingnomia_rmlui_qt_spike
    PRIVATE
        Qt6::Core
        Qt6::Gui
        OpenGL::GL
        RmlUi::Core
        RmlUi::Debugger
        ingnomia_rmlui_gl3
        glad
)

add_custom_command(TARGET ingnomia_rmlui_qt_spike POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/assets"
        "$<TARGET_FILE_DIR:ingnomia_rmlui_qt_spike>/assets"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/../../content/rmlui"
        "$<TARGET_FILE_DIR:ingnomia_rmlui_qt_spike>/assets"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/../../content/tilesheet"
        "$<TARGET_FILE_DIR:ingnomia_rmlui_qt_spike>/assets/tilesheet"
    VERBATIM
)

if(WIN32)
    add_custom_command(TARGET ingnomia_rmlui_qt_spike POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:ingnomia_rmlui_qt_spike>/platforms"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::Core>"
            "$<TARGET_FILE_DIR:ingnomia_rmlui_qt_spike>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::Gui>"
            "$<TARGET_FILE_DIR:ingnomia_rmlui_qt_spike>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:ingnomia_rmlui_qt_spike>/platforms"
        VERBATIM
    )
endif()
