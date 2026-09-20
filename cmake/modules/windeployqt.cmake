find_package(Qt6 REQUIRED QUIET COMPONENTS Core)
set(INGNOMIA_WINDEPLOYQT_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}")
if(TARGET Qt6::qmake)
	get_target_property(_qt6_qmake_location Qt6::qmake IMPORTED_LOCATION)

	execute_process(
		COMMAND "${_qt6_qmake_location}" -query QT_INSTALL_PREFIX
		RESULT_VARIABLE return_code
		OUTPUT_VARIABLE qt6_install_prefix
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	if(NOT TARGET Qt6::windeployqt)
		set(imported_location "${qt6_install_prefix}/bin/windeployqt6.exe")

		if(EXISTS ${imported_location})
			add_executable(Qt6::windeployqt IMPORTED)

			set_target_properties(Qt6::windeployqt PROPERTIES
				IMPORTED_LOCATION ${imported_location}
			)
		endif()
	endif()
endif()

function(windeployqt foo)
	if(TARGET Qt6::windeployqt)
		set(deploy_script "${INGNOMIA_WINDEPLOYQT_MODULE_DIR}/DeployQtRuntime.cmake")
		set(deploy_staging_dir "${CMAKE_CURRENT_BINARY_DIR}/windeployqt_$<CONFIG>")
		set(deploy_signature "${qt6_install_prefix},${Qt6Core_VERSION},$<CONFIG>,no-translations,no-system-d3d-compiler,no-opengl-sw")

		# Keep development relinks cheap. The script runs windeployqt only when
		# the Qt installation/configuration changes or the deployed runtime is absent.
		add_custom_command(TARGET ${foo}
			POST_BUILD
			COMMAND ${CMAKE_COMMAND}
				"-DDEPLOY_TOOL=$<TARGET_FILE:Qt6::windeployqt>"
				"-DQT_BIN_DIR=${qt6_install_prefix}/bin"
				"-DTARGET_FILE=$<TARGET_FILE:${foo}>"
				"-DTARGET_DIR=$<TARGET_FILE_DIR:${foo}>"
				"-DSTAGING_DIR=${deploy_staging_dir}"
				"-DDEPLOY_SIGNATURE=${deploy_signature}"
				-DFORCE_DEPLOY=OFF
				-P "${deploy_script}"
			VERBATIM
		)

		# Packaging and troubleshooting can still request an unconditional refresh.
		add_custom_target(${foo}_deploy_runtime
			COMMAND ${CMAKE_COMMAND}
				"-DDEPLOY_TOOL=$<TARGET_FILE:Qt6::windeployqt>"
				"-DQT_BIN_DIR=${qt6_install_prefix}/bin"
				"-DTARGET_FILE=$<TARGET_FILE:${foo}>"
				"-DTARGET_DIR=$<TARGET_FILE_DIR:${foo}>"
				"-DSTAGING_DIR=${deploy_staging_dir}"
				"-DDEPLOY_SIGNATURE=${deploy_signature}"
				-DFORCE_DEPLOY=ON
				-P "${deploy_script}"
			DEPENDS ${foo}
			COMMENT "Refreshing the Qt runtime deployment for ${foo}"
			VERBATIM
		)

		# copy deployment directory during installation
		install(
			DIRECTORY
			"${deploy_staging_dir}/"
			DESTINATION ${CMAKE_INSTALL_BINDIR}
			PATTERN ".deploy-signature" EXCLUDE
		)
	endif()
endfunction()
