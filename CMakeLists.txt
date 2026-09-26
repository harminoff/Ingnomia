cmake_minimum_required(VERSION 3.16.0)
# Disable release-builds without debug info
set(CMAKE_CONFIGURATION_TYPES Debug RelWithDebInfo CACHE STRING INTERNAL FORCE)

# Project version, 4 numbers representation
set(DEFAULT_VERSION "0.10.0.0" CACHE STRING "Version" FORCE)
# Set from CI only
set(BUILD_ID "0" CACHE INTERNAL "")
set(GIT_REPO "" CACHE INTERNAL "")
set(GIT_REF "" CACHE INTERNAL "")
set(GIT_SHA "" CACHE INTERNAL "")

project(Ingnomia
	VERSION ${DEFAULT_VERSION}
	DESCRIPTION "Gnome city builder"
	HOMEPAGE_URL "https://github.com/rschurade/Ingnomia"
	LANGUAGES C CXX
)

include(CTest)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules")

find_package(Qt6 REQUIRED QUIET COMPONENTS Core Xml Sql Gui Widgets)
find_package(OpenGL REQUIRED)

find_package(Steam REQUIRED)
find_package(OpenAL REQUIRED)

set(BUGSPLAT_DB "" CACHE STRING "BSDB")

# Without this, paths are not relative in the sources list
cmake_policy(SET CMP0076 NEW)

include(GNUInstallDirs)

# The program itself
add_executable(${PROJECT_NAME})
option(INGNOMIA_ENABLE_IPO "Enable interprocedural optimization for the game" ON)

# C++ version
set_target_properties(${PROJECT_NAME}
	PROPERTIES
		CXX_STANDARD 23
		AUTOMOC ON
		INTERPROCEDURAL_OPTIMIZATION ${INGNOMIA_ENABLE_IPO}
		INSTALL_RPATH "$ORIGIN"
)
if(MSVC AND NOT INGNOMIA_ENABLE_IPO)
	target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY AUTOGEN_SOURCE_GROUP "Generated")

add_subdirectory(content) # Deployable content
# Stage content to target directory whenever any content file changes. Using a stamp
# file with file-level DEPENDS lets ninja/VS re-copy only when content actually changes,
# without requiring the C++ target to rebuild.
file(GLOB_RECURSE INGNOMIA_CONTENT_FILES CONFIGURE_DEPENDS
	"${CMAKE_SOURCE_DIR}/content/*"
)
set(INGNOMIA_CONTENT_STAMP "${CMAKE_BINARY_DIR}/content.stamp")
set(INGNOMIA_COPY_DIRECTORY_COMMAND copy_directory)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.26")
	set(INGNOMIA_COPY_DIRECTORY_COMMAND copy_directory_if_different)
endif()
if(CMAKE_CONFIGURATION_TYPES)
	set(INGNOMIA_RUNTIME_CONTENT_DIR "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/content")
else()
	set(INGNOMIA_RUNTIME_CONTENT_DIR "${CMAKE_BINARY_DIR}/content")
endif()
add_custom_command(
	OUTPUT ${INGNOMIA_CONTENT_STAMP}
	COMMAND ${CMAKE_COMMAND} -E ${INGNOMIA_COPY_DIRECTORY_COMMAND}
		${CMAKE_SOURCE_DIR}/content
		${INGNOMIA_RUNTIME_CONTENT_DIR}
	# RmlUi's file interface intentionally sandboxes resources under content/rmlui.
	# Keep the generated DB-backed item crops available inside that root as well as
	# in the canonical content/tilesheet package used by the game world.
	COMMAND ${CMAKE_COMMAND} -E ${INGNOMIA_COPY_DIRECTORY_COMMAND}
		${CMAKE_SOURCE_DIR}/content/tilesheet
		${INGNOMIA_RUNTIME_CONTENT_DIR}/rmlui/tilesheet
	COMMAND ${CMAKE_COMMAND}
		-DUI_THEME_SOURCE_ROOT=${CMAKE_SOURCE_DIR}
		-DUI_THEME_OUTPUT_DIR=${INGNOMIA_RUNTIME_CONTENT_DIR}/rmlui/styles
		-P ${CMAKE_SOURCE_DIR}/cmake/GenerateUiTheme.cmake
	COMMAND ${CMAKE_COMMAND} -E touch ${INGNOMIA_CONTENT_STAMP}
	DEPENDS ${INGNOMIA_CONTENT_FILES} ${CMAKE_SOURCE_DIR}/cmake/GenerateUiTheme.cmake
	COMMENT "Incrementally staging content to ${INGNOMIA_RUNTIME_CONTENT_DIR}"
	VERBATIM
)
add_custom_target(stage_content DEPENDS ${INGNOMIA_CONTENT_STAMP})
add_dependencies(${PROJECT_NAME} stage_content)

if(WIN32)
	set(RC_SOURCES
		${PROJECT_NAME}.ico
		windows/${PROJECT_NAME}.rc
		windows/${PROJECT_NAME}.manifest
		windows/resource.h
	)
	target_sources(${PROJECT_NAME}
		PRIVATE
			${RC_SOURCES}
	)

endif()

target_sources(${PROJECT_NAME}
	PRIVATE
		Changelog.md
		README.md
		LICENSE
)

add_subdirectory(src) # Sources list
add_subdirectory(3rdparty) # 3rd party dependencies

# Linking
target_link_libraries(${PROJECT_NAME}
	PRIVATE
		Qt6::Core
		Qt6::Xml
		Qt6::Sql
		Qt6::Gui
		Qt6::Widgets
		OpenGL::GL
		glad
		RmlUi::Core
		ingnomia_rmlui_gl3
		$<$<CONFIG:Debug>:RmlUi::Debugger>
		Steam
		fastnoise
		exprtk
		OpenAL
)

target_compile_definitions(${PROJECT_NAME}
	PRIVATE
		$<$<CONFIG:Debug>:INGNOMIA_RMLUI_DEBUGGER=1>
		$<$<CONFIG:Debug>:INGNOMIA_DEVELOPER_UI=1>
)

option(INGNOMIA_BUILD_UI_FOUNDATION_TESTS "Build framework-neutral UI foundation tests" ${BUILD_TESTING})
option(INGNOMIA_UI_DESIGNER "Compile the in-game RmlUi visual designer" OFF)
if(INGNOMIA_UI_DESIGNER)
	target_compile_definitions(${PROJECT_NAME} PRIVATE INGNOMIA_UI_DESIGNER=1)
endif()
if(INGNOMIA_BUILD_UI_FOUNDATION_TESTS)
	add_subdirectory(tests/ui-foundation)
endif()
if(BUILD_TESTING)
	add_subdirectory(tests/water)
endif()

configure_file(
	${CMAKE_SOURCE_DIR}/src/version.h.in
	${CMAKE_SOURCE_DIR}/src/version.h
	@ONLY
)

target_sources(${PROJECT_NAME}
	PRIVATE
		${CMAKE_SOURCE_DIR}/src/version.h.in
)

if(UNIX)
	# Qt requires position independent code
	target_compile_options(${PROJECT_NAME}
		PRIVATE
			-fPIC
	)
endif()

get_target_property(SOURCE_LIST ${PROJECT_NAME} SOURCES)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCE_LIST})

if(UNIX AND NOT (DEFINED ENV{CXXFLAGS} OR CMAKE_CXX_FLAGS))
	set(CMAKE_CXX_FLAGS "-Wall -Wextra -g")
	# Kill some overly verbose warnings
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas -Wno-unused-parameter")
	# But treat likely errors as actual errors
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=missing-field-initializers -Werror=ignored-qualifiers -Werror=delete-non-virtual-dtor -Werror=unused-value")
endif()

if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Gy /Oi /MP /GS- /W3 /permissive- /w24458 /w34456 /w24702")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /CGTHREADS:8 /PROFILE")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Ob2")
	set_target_properties(${PROJECT_NAME} PROPERTIES
		LINK_FLAGS /SUBSYSTEM:WINDOWS
	)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
endif()

if(MSVC AND NOT BUGSPLAT_DB STREQUAL "")
	find_package(BugSplat REQUIRED)

	target_link_libraries(${PROJECT_NAME}
		PRIVATE
			BugSplat
	)

	get_target_property(BUGSPLAT_DLL BugSplat LOCATION)
	get_target_property(BUGSPLAT_RC_DLL BugSplatRc LOCATION)
	get_target_property(BUGSPLAT_SEND_DLL BugSplatSendReport LOCATION)
	list(APPEND RUNTIME_DLLS ${BUGSPLAT_DLL} ${BUGSPLAT_RC_DLL} ${BUGSPLAT_SEND_DLL})
endif()

# Install target
install(
	TARGETS ${PROJECT_NAME}
	DESTINATION ${CMAKE_INSTALL_BINDIR}
)

get_target_property(STEAM_DLL Steam LOCATION)
list(APPEND RUNTIME_DLLS ${STEAM_DLL})

if(WIN32)
	# On Windows, we want to use bundled OpenAL for guaranteed OpenAL Soft support
	list(APPEND RUNTIME_DLLS ${OPENAL_DLL})
endif(WIN32)

install(
	FILES
		${RUNTIME_DLLS}
		Changelog.md
		LICENSE
	DESTINATION
		${CMAKE_INSTALL_BINDIR}
)

if(WIN32)
	install(
		FILES $<TARGET_PDB_FILE:${PROJECT_NAME}>
		DESTINATION ${CMAKE_INSTALL_BINDIR}
		OPTIONAL
	)
endif()

# Deployment helpers for Windows
if(WIN32)
	include(windeployqt)
	windeployqt(${PROJECT_NAME})

	add_custom_command(TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND
			${CMAKE_COMMAND} -E copy_if_different
			${RUNTIME_DLLS}
			$<TARGET_FILE_DIR:${PROJECT_NAME}>
	)

endif()
