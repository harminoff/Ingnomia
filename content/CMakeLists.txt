file(
	GLOB_RECURSE SOURCE_LIST
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	"*.xml"
	"*.json"
	"*.glsl"
	"*.ttf"
	"*.otf"
	"*.png"
	"*.sql"
	"*.wav"
	"*.rml"
	"*.rcss"
	"*.tga"
	"*License*"
	"*.txt"
	"*.TXT"
)

target_sources(${PROJECT_NAME}
	PRIVATE
		${SOURCE_LIST}
		${CMAKE_CURRENT_LIST_FILE}
)

foreach ( FILE ${SOURCE_LIST} )
    get_filename_component( DIR ${FILE} DIRECTORY )
    install(
		FILES ${FILE}
		DESTINATION "${CMAKE_INSTALL_BINDIR}/content/${DIR}"
	)
endforeach()

# RmlUi runs with content/rmlui as its sandboxed asset root. Install the
# generated DB-backed crops there as well as in the canonical tilesheet folder.
install(
	DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tilesheet/"
	DESTINATION "${CMAKE_INSTALL_BINDIR}/content/rmlui/tilesheet"
)
