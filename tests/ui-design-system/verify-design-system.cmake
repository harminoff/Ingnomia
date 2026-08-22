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

foreach(relative IN ITEMS
    styles/tokens.json
    styles/base.rcss
    styles/components.rcss
    styles/accessibility.rcss
    templates/framed_window.rml
    templates/panel.rml
    templates/modal_shell.rml
    templates/state_panel.rml
    templates/inspector_section.rml
    components/README.md
    fixtures/components.rml
    fixtures/README.md
    fonts/LatoLatin-Regular.ttf
    fonts/notices/RmlUi-Samples-Lato-OFL-1.1.txt
    notices/RmlUi-LICENSE.txt
    notices/FreeType-LICENSE.TXT
    ASSET_LICENSES.md)
    require_file("${relative}")
endforeach()

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

# Wave 1 foundation evidence is a frozen compatibility boundary for this wave.
file(SHA256 "${UI}/smoke.rml" SMOKE_RML_SHA)
file(SHA256 "${UI}/smoke.rcss" SMOKE_RCSS_SHA)
file(SHA256 "${UI}/smoke.tga" SMOKE_TGA_SHA)
if(NOT SMOKE_RML_SHA STREQUAL "4e05a6a1ea6ec1a2fe75279b687303bee8c09fdee2f0cf1a5abc39c7ffffc737")
    message(FATAL_ERROR "Wave 1 smoke.rml changed")
endif()
if(NOT SMOKE_RCSS_SHA STREQUAL "c52b0a6044c74177a4ad35b84ecaab9bd8937f5eeb62989703dd66196113cbbd")
    message(FATAL_ERROR "Wave 1 smoke.rcss changed")
endif()
if(NOT SMOKE_TGA_SHA STREQUAL "4224413c9036c68880dac3fa0c65202b4b712aa6015b8d55f4b1ea1548741c06")
    message(FATAL_ERROR "Wave 1 smoke.tga changed")
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
set(ALL_SHARED "${BASE}\n${COMPONENTS}\n${ACCESS}")
require_text("${BASE}" "font-family:[ \\t]*LatoLatin" "pinned runtime font family")

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

foreach(token IN ITEMS
    "#101315" "#252d31" "#13191b" "#f1eee6" "#ffd27a"
    "#d19a55" "#71b7d3" "#77be78" "#e0b45d" "#e27b70")
    require_text("${TOKENS}" "${token}" "canonical design token")
    require_text("${ALL_SHARED}" "${token}" "expanded token use")
endforeach()

file(GLOB_RECURSE RUNTIME_TEXT
    "${UI}/*.rml"
    "${UI}/*.rcss")
foreach(path IN LISTS RUNTIME_TEXT)
    file(READ "${path}" TEXT)
    foreach(forbidden IN ITEMS
        "@media" "display:[ \t]*grid" "position:[ \t]*sticky" "calc\\("
        "var\\(--" ":has\\(" "::before" "::after" "url\\([ \t]*https?://"
        "transition:[ \t]*background-color"
        "border-style:[ \t]*dashed" "border-collapse:[ \t]*collapse")
        if(TEXT MATCHES "${forbidden}")
            message(FATAL_ERROR "Unsupported/browser-only assumption '${forbidden}' in ${path}")
        endif()
    endforeach()
endforeach()

# New screens must consume shared values. Fixture inline layout is allowed, but
# colors and timing values stay in the centralized shared styles/token source.
string(REGEX REPLACE "&#[0-9]+;" "" FIXTURE_NO_ENTITIES "${FIXTURE}")
if(FIXTURE_NO_ENTITIES MATCHES "#[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]")
    message(FATAL_ERROR "Fixture contains an inline color instead of shared RCSS")
endif()

message(STATUS "Wave 2 design-system source verification passed")
