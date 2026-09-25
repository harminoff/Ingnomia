file(READ "${CMAKE_CURRENT_LIST_DIR}/../../content/rmlui/screens/game_hud.rml" RML)
file(READ "${CMAKE_CURRENT_LIST_DIR}/../../content/rmlui/screens/orders_tools.rml" ORDERS_TOOLS_RML)
file(READ "${CMAKE_CURRENT_LIST_DIR}/../../src/gui/ui/screens/hud/HudRmlBinding.cpp" BINDING)
file(READ "${CMAKE_CURRENT_LIST_DIR}/../../src/gui/aggregatorinventory.cpp" INVENTORY)
file(READ "${CMAKE_CURRENT_LIST_DIR}/../../content/rmlui/screens/hud.rcss" STYLE)
file(READ "${CMAKE_CURRENT_LIST_DIR}/../../src/gui/MainWindow.cpp" MAIN_WINDOW)
# Stage 17: the game window has one toolbar along the top and a status bar along the bottom (PDF p.151-155, p.288-289);
# tools with several orders open drop-down menus; Build, the tutorial and events are palette windows and message boxes.
foreach(ID hud_root hud_top_rail hud_tool_shelf hud_status_bar hud_status hud_active_tool hud_kingdom hud_level hud_date hud_clock hud_daylight hud_gnomes hud_animals hud_items hud_watch_rows
    hud_pause hud_speed_normal hud_speed_fast hud_level_down hud_level_up hud_open_inventory hud_open_population hud_open_military hud_open_diplomacy
    hud_tool_inspect hud_tool_build hud_tool_deconstruct hud_tool_mine hud_tool_agriculture hud_tool_designations hud_tool_jobs hud_tool_cancel hud_tool_rotate hud_tool_view
    hud_mine_menu hud_agriculture_menu hud_designations_menu hud_jobs_menu hud_view_menu hud_overlay_designations hud_overlay_jobs hud_overlay_walls hud_overlay_axles
    hud_mine_walls hud_mine_ramp_down hud_tool_fell_tree hud_tool_stockpile hud_tool_remove_designation hud_tool_suspend_job hud_tool_lower_priority
    hud_build_panel hud_build_heading hud_build_close hud_build_categories hud_build_categories_page hud_build_furniture hud_build_fence hud_build_types_page hud_build_type_list hud_build_catalog hud_build_items hud_build_details
    hud_tutorial_panel tutorial_title tutorial_progress tutorial_explanation tutorial_objective tutorial_steps tutorial_checklist tutorial_warning tutorial-continue tutorial-skip tutorial-restart tutorial-hints tutorial-finish
    hud_event_blocker hud_event_title hud_event_body hud_event_ack hud_event_yes hud_event_no)
  string(REGEX MATCHALL "id=\"${ID}\"" MATCHES "${RML}")
  list(LENGTH MATCHES COUNT)
  if(NOT COUNT EQUAL 1)
    message(FATAL_ERROR "HUD ID must exist exactly once: ${ID}")
  endif()
endforeach()
foreach(ID hud_build_panel hud_build_close hud_build_categories_page hud_build_furniture hud_build_type_list hud_build_catalog hud_build_items hud_build_details)
  string(FIND "${ORDERS_TOOLS_RML}" "id=\"${ID}\"" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "Build window missing ${ID}")
  endif()
endforeach()
# Retired: the slide-out sidebar, its pages and Back buttons, the floating hint strip, and card-style build rows.
foreach(FORBIDDEN "hud_sidebar_edge" "hud_sidebar_root" "hud_sidebar_tooltip" "l-hud-sidebar" "hud_mine_back" "hud_jobs_back" "hud_hint_strip" "hud_kingdom_panel" "c-hud-build-card" "c-button" "data-action=")
  string(FIND "${RML}${ORDERS_TOOLS_RML}" "${FORBIDDEN}" POS)
  if(NOT POS EQUAL -1)
    message(FATAL_ERROR "HUD still contains a retired surface: ${FORBIDDEN}")
  endif()
endforeach()
foreach(NEEDLE "class=\"w98-toolbar l-hud-toolbar\"" "role=\"toolbar\"" "class=\"w98-tool\"" "class=\"w98-toolbar__separator\"" "class=\"w98-tool__caret\"" "aria-haspopup=\"true\"" "class=\"w98-menu is-hidden\"" "role=\"menuitemradio\"" "role=\"menuitemcheckbox\""
    "class=\"w98-menu__separator\"" "class=\"w98-statusbar l-hud-statusbar\"" "w98-statusbar__pane--grow" "w98-sheet w98-palette l-hud-build-panel" "w98-sheet w98-palette l-hud-tutorial" "w98-sheet w98-msgbox" "w98-msgbox__symbol is-info" "role=\"alertdialog\"" "href=\"win98_classic.rcss\"")
  string(FIND "${RML}" "${NEEDLE}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "HUD is missing its Windows 98 composition: ${NEEDLE}")
  endif()
endforeach()
foreach(NEEDLE "34.9091em" "34.5455em" ".l-hud-toolbar" ".l-hud-statusbar" ".l-hud-modal" "pointer-events: none" "pointer-events: auto")
  string(FIND "${STYLE}" "${NEEDLE}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "HUD layout missing ${NEEDLE}")
  endif()
endforeach()
# Binding: status bar messages, option-set and menu marks, one message box at a time with the focus on its button,
# and lists that are never rebuilt under the element a click is dispatched to.
foreach(NEEDLE "AddEventListener" "aria-pressed" "aria-expanded" "is-chosen" "is-checked" "is-open" "statusTexts" "Unavailable because" "Esc cancels" "\"Ready\"" "focusedPrompt_" "button->Focus( true )"
    "renderedBuildList_" "renderedBuildDetails_" "a click must not destroy" "toolCursor_" "closeBuildMenu" "buildMenuOpen_" "data-build-action='FillHole'" "data-build-action='Replace'" "data-build-action='Build'"
    "hud_build_action_Build_" "Place Blueprint" "KI_ESCAPE")
  string(FIND "${BINDING}" "${NEEDLE}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "HUD binding contract missing ${NEEDLE}")
  endif()
endforeach()
foreach(NEEDLE "BuildSelection::Furniture" "BuildSelection::Workshop" "BuildSelection::Utility" "BuildSelection::Wall" "BuildSelection::Ramps")
  string(FIND "${BINDING}" "${NEEDLE}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "Build must request the complete category for ${NEEDLE}")
  endif()
endforeach()
foreach(NEEDLE "gbi.type = row.value( \"Tab\" ).toString()" "gbi.type = row.value( \"ItemGroup\" ).toString()" "gbi.type = row.value( \"Category\" ).toString()"
    "DB::selectRows( \"Constructions\", \"Type\", m_buildSelection2String.value( buildSelection ) )" "DB::selectRows( \"Workshops\" )" "DB::selectRows( \"Items\", \"Category\", \"Furniture\" )" "DB::selectRows( \"Items\", \"Category\", \"Utility\" )" "Type\", \"WallFloor\"" "Type\", \"RampCorner\"")
  string(FIND "${INVENTORY}" "${NEEDLE}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "Build catalog contract missing ${NEEDLE}")
  endif()
endforeach()
foreach(NEEDLE "The Build window is a Windows 98 palette window" "setToolCursorHandler" "setMapCursor" "row.type = item.type.toStdString();")
  string(FIND "${MAIN_WINDOW}" "${NEEDLE}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "Game window HUD integration missing ${NEEDLE}")
  endif()
endforeach()
message(STATUS "HUD RML contract passed")
