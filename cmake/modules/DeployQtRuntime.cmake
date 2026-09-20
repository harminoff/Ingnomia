cmake_minimum_required(VERSION 3.16)

foreach(required_variable IN ITEMS
	DEPLOY_TOOL
	QT_BIN_DIR
	TARGET_FILE
	TARGET_DIR
	STAGING_DIR
	DEPLOY_SIGNATURE
)
	if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
		message(FATAL_ERROR "DeployQtRuntime.cmake requires ${required_variable}")
	endif()
endforeach()

set(signature_file "${STAGING_DIR}/.deploy-signature")
set(runtime_is_present FALSE)
if(EXISTS "${TARGET_DIR}/Qt6Core.dll" OR EXISTS "${TARGET_DIR}/Qt6Cored.dll")
	set(runtime_is_present TRUE)
endif()

set(signature_matches FALSE)
if(EXISTS "${signature_file}")
	file(READ "${signature_file}" current_signature)
	if(current_signature STREQUAL DEPLOY_SIGNATURE)
		set(signature_matches TRUE)
	endif()
endif()

if(NOT FORCE_DEPLOY AND runtime_is_present AND signature_matches)
	message(STATUS "Qt runtime deployment is current; skipping windeployqt")
	return()
endif()

file(REMOVE_RECURSE "${STAGING_DIR}")
file(MAKE_DIRECTORY "${STAGING_DIR}")

execute_process(
	COMMAND "${CMAKE_COMMAND}" -E env "PATH=${QT_BIN_DIR};$ENV{PATH}"
		"${DEPLOY_TOOL}"
		--no-translations
		--no-system-d3d-compiler
		--no-opengl-sw
		--dir "${STAGING_DIR}"
		"${TARGET_FILE}"
	RESULT_VARIABLE deploy_result
	OUTPUT_VARIABLE deploy_output
	ERROR_VARIABLE deploy_error
)
if(NOT deploy_result EQUAL 0)
	message(FATAL_ERROR
		"windeployqt failed (${deploy_result})\n${deploy_output}\n${deploy_error}")
endif()

set(copy_runtime_directory copy_directory)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.26")
	set(copy_runtime_directory copy_directory_if_different)
endif()
execute_process(
	COMMAND "${CMAKE_COMMAND}" -E ${copy_runtime_directory}
		"${STAGING_DIR}/" "${TARGET_DIR}"
	RESULT_VARIABLE copy_result
	ERROR_VARIABLE copy_error
)
if(NOT copy_result EQUAL 0)
	message(FATAL_ERROR "Failed to copy the Qt runtime (${copy_result})\n${copy_error}")
endif()

file(WRITE "${signature_file}" "${DEPLOY_SIGNATURE}")
message(STATUS "Qt runtime deployment refreshed")
