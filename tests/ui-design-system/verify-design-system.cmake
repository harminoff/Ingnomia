cmake_minimum_required(VERSION 3.24)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(UI "${ROOT}/content/rmlui")

function(require_file relative)
    if(NOT EXISTS "${UI}/${relative}")
        message(FATAL_ERROR "Missing Wave 2 artifact: content/rmlui/${relative}")
    endif()
endfunction()

function(require_text text pattern label)
    if(NOT "${text}" MATCHES "${pattern}")
        message(FATAL_ERROR "Missing ${label}: ${pattern}")
    endif()
endfunction()

function(strip_rcss_comments input output)
    # CSS comments are inert; remove them before style/token assertions so
    # comments cannot satisfy checks for active production declarations.
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" active_text "${input}")
    set(${output} "${active_text}" PARENT_SCOPE)
endfunction()

foreach(relative IN ITEMS
    styles/tokens.json
    styles/base.rcss
    styles/base.rcss.in
    styles/components.rcss
    styles/components.rcss.in
    styles/accessibility.rcss
    styles/accessibility.rcss.in
    styles/management_window.rcss.in
    templates/framed_window.rml
    templates/panel.rml
    templates/modal_shell.rml
    templates/state_panel.rml
    templates/inspector_section.rml
    components/README.md
    fixtures/components.rml
    fixtures/README.md
    icons/chrome-arrows.tga
    fonts/LatoLatin-Regular.ttf
    fonts/notices/RmlUi-Samples-Lato-OFL-1.1.txt
    notices/RmlUi-LICENSE.txt
    notices/FreeType-LICENSE.TXT
    ASSET_LICENSES.md)
    require_file("${relative}")
endforeach()
if(NOT EXISTS "${ROOT}/cmake/GenerateUiTheme.cmake")
    message(FATAL_ERROR "Missing shared theme generator: cmake/GenerateUiTheme.cmake")
endif()

# The runtime face and notices are copied from the pinned dependency trees, not
# from an untracked host font. Keep the binary/license boundary deterministic.
file(SHA256 "${UI}/fonts/LatoLatin-Regular.ttf" LATO_SHA)
if(NOT LATO_SHA STREQUAL "d785334ac4e7810f571def986bbad41161f68ac385db8813f798bf04d71478e1")
    message(FATAL_ERROR "Pinned LatoLatin-Regular.ttf changed")
endif()
file(READ "${UI}/fonts/notices/RmlUi-Samples-Lato-OFL-1.1.txt" LATO_NOTICE)
file(READ "${UI}/notices/RmlUi-LICENSE.txt" RMLUI_NOTICE)
file(READ "${UI}/notices/FreeType-LICENSE.TXT" FREETYPE_NOTICE)
require_text("${LATO_NOTICE}" "SIL OPEN FONT LICENSE Version 1\\.1" "Lato OFL 1.1 notice")
require_text("${RMLUI_NOTICE}" "MIT License" "RmlUi MIT notice")
require_text("${FREETYPE_NOTICE}" "FREETYPE LICENSES" "FreeType license notice")

# Keep a content-integrity boundary for the current committed smoke assets.
# Refresh these hashes only when the corresponding source asset intentionally
# changes; this catches accidental replacement without freezing a historical file.
file(SHA256 "${UI}/smoke.rml" SMOKE_RML_SHA)
file(SHA256 "${UI}/smoke.rcss" SMOKE_RCSS_SHA)
file(SHA256 "${UI}/smoke.tga" SMOKE_TGA_SHA)
if(NOT SMOKE_RML_SHA STREQUAL "2b168523838a58fe969725e56f4f1df5ab02a359f4f875b5651bfd7b3cb55188")
    message(FATAL_ERROR "Current smoke.rml integrity check failed")
endif()
if(NOT SMOKE_RCSS_SHA STREQUAL "9a126d84c6d3a67c2cf85a5e9f54bc0c6778d8d81009b7da8124554578b0b543")
    message(FATAL_ERROR "Current smoke.rcss integrity check failed")
endif()
if(NOT SMOKE_TGA_SHA STREQUAL "4224413c9036c68880dac3fa0c65202b4b712aa6015b8d55f4b1ea1548741c06")
    message(FATAL_ERROR "Current smoke.tga integrity check failed")
endif()

file(READ "${UI}/fixtures/components.rml" FIXTURE)
set(FIXTURE_MARKUP "${FIXTURE}")
foreach(template IN ITEMS framed_window panel modal_shell state_panel inspector_section)
    file(READ "${UI}/templates/${template}.rml" TEMPLATE_TEXT)
    string(APPEND FIXTURE_MARKUP "\n${TEMPLATE_TEXT}")
endforeach()
file(READ "${UI}/styles/components.rcss" COMPONENTS)
file(READ "${UI}/styles/base.rcss" BASE)
file(READ "${UI}/styles/accessibility.rcss" ACCESS)
file(READ "${UI}/styles/tokens.json" TOKENS)
strip_rcss_comments("${BASE}" ACTIVE_BASE)
strip_rcss_comments("${COMPONENTS}" ACTIVE_COMPONENTS)
strip_rcss_comments("${ACCESS}" ACTIVE_ACCESS)
set(ALL_SHARED "${ACTIVE_BASE}\n${ACTIVE_COMPONENTS}\n${ACTIVE_ACCESS}")
require_text("${ACTIVE_BASE}" "font-family:[ \\t]*LatoLatin" "pinned runtime font family")

foreach(component IN ITEMS
    panel window title-bar toolbar icon-button button toggle segmented tabs
    list table tree scroll-region field numeric-input slider progress badge
    status-chip tooltip popover modal alert-row state-panel key-hint
    context-action inspector-section)
    require_text("${ALL_SHARED}" "\\.c-${component}([^A-Za-z0-9_-]|$)" "component style c-${component}")
    require_text("${FIXTURE_MARKUP}" "c-${component}([^A-Za-z0-9_-]|&quot;|$)" "component fixture c-${component}")
endforeach()

foreach(feature IN ITEMS
    "<template src=\"framed-window\""
    "href=\"/templates/modal_shell.rml\""
    "type=\"checkbox\""
    "type=\"range\""
    "<select"
    "<progress"
    "<table"
    "disabled=\"disabled\""
    "is-selected"
    "is-pending"
    "is-invalid"
    "is-visible"
    "is-open")
    require_text("${FIXTURE}" "${feature}" "fixture feature")
endforeach()

require_text("${FIXTURE}" "id=\"fixture-generated-select\"" "generated select probe target")
require_text("${FIXTURE}" "id=\"fixture-generated-scroll\"" "generated scrollbar probe target")
require_text("${ACTIVE_COMPONENTS}" "c-scroll-region scrollbarvertical slidertrack" "RmlUi scrollbar track selector")
require_text("${ACTIVE_COMPONENTS}" "c-scroll-region scrollbarvertical sliderbar" "RmlUi scrollbar bar selector")
file(GLOB_RECURSE PRODUCTION_RCSS "${UI}/*.rcss")
foreach(path IN LISTS PRODUCTION_RCSS)
    file(READ "${path}" TEXT)
    strip_rcss_comments("${TEXT}" ACTIVE_TEXT)
    if(ACTIVE_TEXT MATCHES "scrollbar(vertical|horizontal)[ \\t]+(track|slider)([^A-Za-z0-9_-]|$)")
        message(FATAL_ERROR "Scrollbar styles target non-generated 'track' or 'slider' parts instead of RmlUi slidertrack/sliderbar: ${path}")
    endif()
endforeach()

# Read expected colors from named JSON tokens, then require each value in active
# shared RCSS. This checks token structure and expanded use without relying on
# matches that appear only in comments.
require_text("${TOKENS}" "ingnomia\.windows98-design-tokens\.v2" "Windows 98 theme token schema")
require_text("${TOKENS}" "windows-98-classic" "single normal theme name")
foreach(token_path IN ITEMS
    "color.surface.window"
    "color.surface.inset"
    "color.surface.selection"
    "color.border.highlight"
    "color.border.shadow"
    "color.caption.active"
    "color.text.primary"
    "color.interaction.focus"
    "color.text.disabled"
    "color.semantic.information.strong"
    "color.semantic.success.strong"
    "color.semantic.warning.strong"
    "color.semantic.danger.strong"
    "accessibility.focus")
    string(JSON token_value ERROR_VARIABLE token_error GET "${TOKENS}" rcss_tokens "${token_path}")
    if(NOT token_error STREQUAL "NOTFOUND")
        message(FATAL_ERROR "Missing design token path '${token_path}': ${token_error}")
    endif()
    string(FIND "${ALL_SHARED}" "${token_value}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR "Active shared RCSS does not use token ${token_path} value ${token_value}")
    endif()
endforeach()

# Generated shared sheets must exactly match their authoring inputs, and a
# disposable token edit must reach both the component fixture base and the
# detached management-window frame consumers.
file(READ "${ROOT}/content/rmlui/styles/tokens.json" TOKENS)
file(READ "${ROOT}/content/rmlui/documents/app_shell.rml" APP_SHELL)
file(READ "${ROOT}/content/rmlui/screens/settings.rml" SETTINGS_ROUTE)
file(READ "${ROOT}/content/rmlui/fixtures/components.rml" COMPONENT_FIXTURE)
file(READ "${ROOT}/content/rmlui/templates/management_window.rml" MANAGEMENT_TEMPLATE)
foreach(consumer IN ITEMS "${APP_SHELL}" "${SETTINGS_ROUTE}" "${COMPONENT_FIXTURE}")
    if(NOT consumer MATCHES "href=\"[^\"]*base\.rcss\"")
        message(FATAL_ERROR "A fixture or production sentinel route no longer consumes generated base.rcss")
    endif()
endforeach()
if(NOT MANAGEMENT_TEMPLATE MATCHES "href=\"/styles/management_window\.rcss\"")
    message(FATAL_ERROR "Detached management template no longer consumes generated frame styles")
endif()

set(THEME_CHECK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ui-theme-generation-check")
set(THEME_CHECK_A "${THEME_CHECK_ROOT}/repeat-a")
set(THEME_CHECK_B "${THEME_CHECK_ROOT}/repeat-b")
set(THEME_CHECK_CHANGED "${THEME_CHECK_ROOT}/changed-token")
file(MAKE_DIRECTORY "${THEME_CHECK_ROOT}")
foreach(output_dir IN ITEMS "${THEME_CHECK_A}" "${THEME_CHECK_B}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DUI_THEME_SOURCE_ROOT=${ROOT}"
            "-DUI_THEME_OUTPUT_DIR=${output_dir}"
            "-P" "${ROOT}/cmake/GenerateUiTheme.cmake"
        RESULT_VARIABLE theme_generation_result
    )
    if(NOT theme_generation_result EQUAL 0)
        message(FATAL_ERROR "Shared theme generation failed for ${output_dir}")
    endif()
endforeach()
file(COPY_FILE "${ROOT}/content/rmlui/styles/tokens.json" "${THEME_CHECK_ROOT}/tokens-mutated.json")
file(READ "${THEME_CHECK_ROOT}/tokens-mutated.json" MUTATED_TOKENS)
string(REPLACE "\"color.text.primary\": \"#000000\"" "\"color.text.primary\": \"#1c1c1c\"" MUTATED_TOKENS "${MUTATED_TOKENS}")
if(MUTATED_TOKENS STREQUAL TOKENS)
    message(FATAL_ERROR "Theme test-copy token mutation did not find color.surface.window")
endif()
file(WRITE "${THEME_CHECK_ROOT}/tokens-mutated.json" "${MUTATED_TOKENS}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DUI_THEME_SOURCE_ROOT=${ROOT}"
        "-DUI_THEME_TOKEN_FILE=${THEME_CHECK_ROOT}/tokens-mutated.json"
        "-DUI_THEME_OUTPUT_DIR=${THEME_CHECK_CHANGED}"
        "-P" "${ROOT}/cmake/GenerateUiTheme.cmake"
    RESULT_VARIABLE changed_generation_result
)
if(NOT changed_generation_result EQUAL 0)
    message(FATAL_ERROR "Test-copy token regeneration failed")
endif()
foreach(name IN ITEMS base components accessibility management_window)
    file(READ "${THEME_CHECK_A}/${name}.rcss" generated_a)
    file(READ "${THEME_CHECK_B}/${name}.rcss" generated_b)
    file(READ "${ROOT}/content/rmlui/styles/${name}.rcss" committed_generated)
    string(REPLACE "\r\n" "\n" committed_generated_normalized "${committed_generated}")
    string(REPLACE "\r\n" "\n" generated_a_normalized "${generated_a}")
    string(REPLACE "\r\n" "\n" generated_b_normalized "${generated_b}")
    string(REGEX REPLACE "\n+$" "" committed_generated_normalized "${committed_generated_normalized}")
    string(REGEX REPLACE "\n+$" "" generated_a_normalized "${generated_a_normalized}")
    string(REGEX REPLACE "\n+$" "" generated_b_normalized "${generated_b_normalized}")
    if(NOT generated_a_normalized STREQUAL generated_b_normalized OR NOT generated_a_normalized STREQUAL committed_generated_normalized)
        message(FATAL_ERROR "${name}.rcss generation is not deterministic or checked in from its inputs")
    endif()
endforeach()
file(READ "${THEME_CHECK_CHANGED}/base.rcss" changed_base)
file(READ "${THEME_CHECK_CHANGED}/components.rcss" changed_components)
file(READ "${THEME_CHECK_CHANGED}/management_window.rcss" changed_management)
if(NOT changed_base MATCHES "#1c1c1c" OR NOT changed_components MATCHES "#1c1c1c" OR NOT changed_management MATCHES "#1c1c1c")
    message(FATAL_ERROR "Test-copy text token did not reach fixture, shared components, and detached-window stylesheet outputs")
endif()

file(GLOB_RECURSE RUNTIME_TEXT
    "${UI}/*.rml"
    "${UI}/*.rcss")
foreach(path IN LISTS RUNTIME_TEXT)
    file(READ "${path}" TEXT)
    if(path MATCHES "\\.rcss$")
        strip_rcss_comments("${TEXT}" ACTIVE_TEXT)
    else()
        set(ACTIVE_TEXT "${TEXT}")
    endif()
    foreach(forbidden IN ITEMS
        "display:[ \t]*grid" "position:[ \t]*sticky" "calc\\("
        "var\\(--" ":has\\(" "::before" "::after" "url\\([ \t]*https?://"
        "transition:[ \t]*background-color"
        "border-style:[ \t]*dashed" "border-collapse:[ \t]*collapse")
        if(ACTIVE_TEXT MATCHES "${forbidden}")
            message(FATAL_ERROR "Unsupported/browser-only assumption '${forbidden}' in ${path}")
        endif()
    endforeach()
endforeach()

# Optional high-contrast rules must be the last linked stylesheet in every
# concrete document that consumes them, after both shared and route styles.
file(GLOB_RECURSE RML_DOCUMENTS "${UI}/*.rml")
foreach(path IN LISTS RML_DOCUMENTS)
    file(READ "${path}" DOCUMENT_TEXT)
    if(DOCUMENT_TEXT MATCHES "accessibility\.rcss")
        string(REGEX MATCHALL "href=\"[^\"]*\.rcss\"" linked_stylesheets "${DOCUMENT_TEXT}")
        list(LENGTH linked_stylesheets linked_stylesheet_count)
        if(linked_stylesheet_count LESS 1)
            message(FATAL_ERROR "Accessibility RCSS is named but no stylesheet links were found: ${path}")
        endif()
        math(EXPR last_stylesheet_index "${linked_stylesheet_count} - 1")
        list(GET linked_stylesheets ${last_stylesheet_index} last_stylesheet)
        if(NOT last_stylesheet MATCHES "accessibility\.rcss")
            message(FATAL_ERROR "Accessibility RCSS must follow route styles in ${path}; last link is ${last_stylesheet}")
        endif()
    endif()
endforeach()

# New screens must consume shared values. Fixture inline layout is allowed, but
# colors and timing values stay in the centralized shared styles/token source.
string(REGEX REPLACE "&#[0-9]+;" "" FIXTURE_NO_ENTITIES "${FIXTURE}")
if(FIXTURE_NO_ENTITIES MATCHES "#[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]")
    message(FATAL_ERROR "Fixture contains an inline color instead of shared RCSS")
endif()

message(STATUS "Wave 2 design-system source verification passed")
