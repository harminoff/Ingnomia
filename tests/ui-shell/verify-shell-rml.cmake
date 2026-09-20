cmake_minimum_required(VERSION 3.16)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(DOCUMENTS
    "documents/app_shell.rml"
    "screens/main_menu.rml"
    "screens/new_game.rml"
    "screens/load_game.rml"
    "screens/loading.rml"
    "screens/settings.rml"
    "screens/pause_menu.rml"
    "modals/confirm_destructive.rml"
)

foreach(relative IN LISTS DOCUMENTS)
    set(path "${ROOT}/content/rmlui/${relative}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing shell document: ${relative}")
    endif()
    file(READ "${path}" text)
    if(text MATCHES "on(click|change|submit)[ \\t]*=" OR text MATCHES "data-event-")
        message(FATAL_ERROR "RML-authored callback/action string in ${relative}")
    endif()
    string(REGEX MATCHALL "id=\"[^\"]+\"" ids "${text}")
    set(seen "")
    foreach(id IN LISTS ids)
        if(id IN_LIST seen)
            message(FATAL_ERROR "Duplicate element ${id} in ${relative}")
        endif()
        list(APPEND seen "${id}")
    endforeach()
endforeach()

file(READ "${ROOT}/content/rmlui/screens/settings.rml" settings)
foreach(dead IN ITEMS "Master volume" "Language" "Key bindings" "High contrast" "Reduced motion" "Tooltip delay" "Notifications")
    if(settings MATCHES "${dead}")
        message(FATAL_ERROR "Unsupported setting '${dead}' is visible")
    endif()
endforeach()

file(READ "${ROOT}/content/rmlui/screens/load_game.rml" load_game)
if(NOT load_game MATCHES "load-error-detail")
    message(FATAL_ERROR "Load Game must expose the live save-list error detail")
endif()
foreach(fabricated IN ITEMS "Screenshot" "Play time" "Population" "Delete save" "Rename save" "Repair")
    if(load_game MATCHES "${fabricated}")
        message(FATAL_ERROR "Uncontracted save metadata '${fabricated}' is visible")
    endif()
endforeach()

file(READ "${ROOT}/content/rmlui/screens/loading.rml" loading)
if(loading MATCHES "[0-9]+%" OR loading MATCHES "ETA")
    message(FATAL_ERROR "Loading document fabricates percent or ETA")
endif()

file(READ "${ROOT}/content/rmlui/screens/new_game.rml" new_game)
foreach(needle IN ITEMS
    "new-tab-world" "new-tab-settlement" "new-tab-terrain" "new-tab-review"
    "new-panel-world" "new-panel-settlement" "new-panel-terrain" "new-panel-review"
    "l-new-game-layout" "new-summary-world" "new-summary-life")
    if(NOT new_game MATCHES "${needle}")
        message(FATAL_ERROR "New-game setup contract missing ${needle}")
    endif()
endforeach()

foreach(needle IN ITEMS "role=\"tablist\"" "role=\"tab\"" "aria-selected=\"true\"")
    string(FIND "${new_game}" "${needle}" tab_contract)
    if(tab_contract LESS 0)
        message(FATAL_ERROR "New Game property-sheet tab contract missing ${needle}")
    endif()
endforeach()
file(READ "${ROOT}/content/rmlui/screens/shell.rcss" shell_css)
foreach(needle IN ITEMS "gap: 0" "margin: 0 0 -1dp 0" "border-right-color: #dce4e6")
    string(FIND "${shell_css}" "${needle}" tab_style)
    if(tab_style LESS 0)
        message(FATAL_ERROR "New Game property-sheet tab style missing ${needle}")
    endif()
endforeach()
if(NOT shell_css MATCHES "\\.l-shell-root[^{]*\\{[^}]*pointer-events:[ \\t]*none")
    message(FATAL_ERROR "The compositor shell must be click-through so routed documents receive input")
endif()
if(NOT shell_css MATCHES "\\.l-shell-screen[^{]*\\{[^}]*pointer-events:[ \\t]*auto")
    message(FATAL_ERROR "Routed shell screens must explicitly own pointer input")
endif()

message(STATUS "Wave 3 shell RML contract check passed")
