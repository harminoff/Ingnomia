add_subdirectory(exprtk)
add_subdirectory(fastnoise)
add_subdirectory(glad)

include(${CMAKE_SOURCE_DIR}/cmake/RmlUiDependencies.cmake)

add_library(ingnomia_rmlui_gl3 STATIC
	${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp
	${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.h
	${CMAKE_SOURCE_DIR}/src/gui/ui/runtime/rmlui_gl_loader.h
)
target_compile_definitions(ingnomia_rmlui_gl3 PRIVATE
	RMLUI_GL3_CUSTOM_LOADER="rmlui_gl_loader.h"
)
target_include_directories(ingnomia_rmlui_gl3
	PUBLIC
		${rmlui_SOURCE_DIR}/Backends
	PRIVATE
		${CMAKE_SOURCE_DIR}/src/gui/ui/runtime
		${CMAKE_CURRENT_SOURCE_DIR}/glad/include
)
target_link_libraries(ingnomia_rmlui_gl3 PUBLIC RmlUi::Core)
set_target_properties(exprtk PROPERTIES FOLDER 3rdParty)
set_target_properties(fastnoise PROPERTIES FOLDER 3rdParty)
set_target_properties(glad PROPERTIES FOLDER 3rdParty)
set_target_properties(ingnomia_rmlui_gl3 PROPERTIES FOLDER 3rdParty)
