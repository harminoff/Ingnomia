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
foreach(dead IN ITEMS "Key bindings" "High contrast" "Reduced motion" "Tooltip delay" "Notifications")
    if(settings MATCHES "${dead}")
        message(FATAL_ERROR "Unsupported setting '${dead}' is visible")
    endif()
endforeach()
foreach(needle IN ITEMS "setting-master-volume" "setting-autosave-interval" "setting-autosave-continue" "settings-page-display" "settings-page-saving")
    if(NOT settings MATCHES "${needle}")
        message(FATAL_ERROR "Settings page is missing ${needle}")
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

# Stage 18: Custom Game is an advanced wizard (PDF p.304-309): Welcome, three interior pages, Completion, with
# < Back, Next >, Finish and Cancel; Enter chooses Next or Finish.
file(READ "${ROOT}/content/rmlui/screens/new_game.rml" new_game)
foreach(needle IN ITEMS
    "new-panel-welcome" "new-panel-world" "new-panel-settlement" "new-panel-terrain" "new-panel-review"
    "id=\"new-back\"" "id=\"new-next\"" "id=\"new-start\"" "id=\"shell-back\"" "Welcome to the Custom Game Wizard" "Completing the Custom Game Wizard"
    "w98-sheet w98-wizard" "w98-wizard__header" "w98-wizard__buttons" "l-wizard-watermark" "class=\"w98-slider\"" "w98-spin" "new-summary-world" "new-summary-life")
    string(FIND "${new_game}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Custom Game wizard contract missing ${needle}")
    endif()
endforeach()
foreach(retired IN ITEMS "role=\"tablist\"" "new-tab-world" "l-new-game-layout" "STEP 1 OF 4")
    string(FIND "${new_game}" "${retired}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Custom Game still has the retired tabbed layout: ${retired}")
    endif()
endforeach()
# Stage 18: the main menu is a dialog box with one column of commands; Load Game follows the Open dialog box.
file(READ "${ROOT}/content/rmlui/screens/main_menu.rml" main_menu)
foreach(needle IN ITEMS "w98-sheet l-shell-dialog" "w98-caption" "Load Game..." "Custom Game..." "Settings<" "Exit<" "l-shell-desktop" "shell-continue-reason")
    string(FIND "${main_menu}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Main menu dialog contract missing ${needle}")
    endif()
endforeach()
foreach(needle IN ITEMS "Look in:" "<select id=\"load-kingdoms\"" "w98-listview" ">Name<" ">Modified<" "id=\"load-file-name\"" "Open<" ">Cancel<" "data-default-action")
    string(FIND "${load_game}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Load Game dialog contract missing ${needle}")
    endif()
endforeach()
file(READ "${ROOT}/content/rmlui/screens/shell.rcss" shell_css)
if(NOT shell_css MATCHES "\\.l-shell-root[^{]*\\{[^}]*pointer-events:[ \\t]*none")
    message(FATAL_ERROR "The compositor shell must be click-through so routed documents receive input")
endif()
if(NOT shell_css MATCHES "\\.l-shell-screen[^{]*\\{[^}]*pointer-events:[ \\t]*auto")
    message(FATAL_ERROR "Routed shell screens must explicitly own pointer input")
endif()

# Stage 19: Settings is a property sheet whose settings apply at once (Close only); Pause is a dialog box; Loading is a
# progress message box that turns into a Warning message box with Retry and Cancel.
foreach(needle IN ITEMS "w98-sheet l-shell-dialog l-settings-sheet" "c-connected-tabs w98-tabs" "role=\"tab\"" ">Display<" ">Controls<" ">Sound<" ">Saving<" "class=\"w98-slider\"" ">Close<" "Defaults<")
    string(FIND "${settings}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Settings property sheet contract missing ${needle}")
    endif()
endforeach()
foreach(retired IN ITEMS ">OK<" ">Apply<" "l-settings-grid")
    string(FIND "${settings}" "${retired}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "Settings must not show ${retired}: its settings apply at once")
    endif()
endforeach()
file(READ "${ROOT}/content/rmlui/screens/pause_menu.rml" pause_menu)
foreach(needle IN ITEMS "w98-sheet l-shell-dialog" ">Pause<" "Resume<" "Save Game<" "Load Game...<" "Settings<" "Main Menu<" "is-in-game")
    string(FIND "${pause_menu}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Pause dialog contract missing ${needle}")
    endif()
endforeach()
foreach(needle IN ITEMS "w98-sheet w98-msgbox" "w98-msgbox__symbol" ">Retry<" ">Cancel<" "loading-error-detail")
    string(FIND "${loading}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Loading message box contract missing ${needle}")
    endif()
endforeach()

message(STATUS "Wave 3 shell RML contract check passed")

# Stage 04 opts an explicit default command into the shared helper.
foreach(marker IN ITEMS "data-default-action=\"true\"")
    string(FIND "${new_game}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing Stage 04 production contract: ${marker}")
    endif()
endforeach()

# Stage 21c: access keys travel with the localized string ("&" marks the underlined letter).
file(READ "${ROOT}/src/gui/ui/localization/UiTextWin98Entries.inc" WIN98_TEXT)
foreach(needle IN ITEMS "&Load Game..." "C&ustom Game..." "&Settings" "E&xit" "&Open" "&Defaults" "&Resume" "&Save Game" "Se&ttings" "&Main Menu")
  string(FIND "${WIN98_TEXT}" "${needle}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "Windows 98 access key missing from the catalog: ${needle}")
  endif()
endforeach()
