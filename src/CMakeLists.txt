file(GLOB_RECURSE SOURCE_LIST
	"*.h"
	"*.hpp"
	"*.cpp"
)

# The beehive dependency is a header-only submodule with its own optional
# GoogleTest sources.  Keep those sources out of the application glob; the
# submodule is not part of the Ingnomia target and its tests require a
# separate dependency setup.
list(FILTER SOURCE_LIST EXCLUDE REGEX "[/\\\\]beehive[/\\\\]")

# The RmlUi UI foundation has explicit includes and is excluded from the
# application precompiled header so it does not inherit renderer globals.
file(GLOB_RECURSE UI_FOUNDATION_SOURCE_LIST
	"gui/ui/*.h"
	"gui/ui/*.hpp"
	"gui/ui/*.cpp"
)
set_source_files_properties(${UI_FOUNDATION_SOURCE_LIST}
	PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

target_sources(${PROJECT_NAME}
	PRIVATE
		${SOURCE_LIST}
		${CMAKE_CURRENT_LIST_FILE}
)

target_precompile_headers(${PROJECT_NAME}
	PRIVATE
		pch.h
)
