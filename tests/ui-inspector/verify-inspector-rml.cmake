set(ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
file(READ "${ROOT}/content/rmlui/screens/inspector.rml" RML)
file(READ "${ROOT}/content/rmlui/screens/inspector.rcss" RCSS)
# Stage 16: both inspectors are Windows 98 property inspectors in palette windows (PDF p.167, p.180-181).
foreach(ID inspector_root inspector_panel inspector_drag_handle inspector_title inspector_kind inspector_position inspector_close inspector_back inspector_locate inspector_refresh inspector_status inspector_scroll
    creature_preview creature_preview_drag_handle creature_preview_title creature_preview_close creature_preview_tabs creature_preview_kind creature_preview_camera_image creature_preview_camera_position creature_preview_activity creature_preview_camera_actions creature_preview_locate creature_preview_locate_label
    creature_preview_nav_camera creature_preview_nav_stats creature_preview_nav_expertise creature_preview_nav_equipment creature_preview_nav_inventory
    creature_preview_camera_panel creature_preview_stats_panel creature_preview_expertise_panel creature_preview_equipment_panel creature_preview_inventory_panel
    creature_preview_attributes_heading creature_preview_needs_heading creature_preview_stats_empty creature_preview_strength creature_preview_charisma creature_preview_hunger creature_preview_happiness
    creature_preview_profession creature_preview_profession_heading creature_preview_professions_empty creature_preview_skill_sort creature_preview_skill_sort_name creature_preview_skill_sort_level creature_preview_skill_sort_active creature_preview_skills_scroll creature_preview_skills creature_preview_skills_empty
    creature_preview_equipment_scope creature_preview_equipment creature_equipment_slot_head creature_equipment_slot_back creature_preview_equipment_editor creature_preview_equipment_editor_title creature_preview_equipment_type creature_preview_equipment_material creature_preview_equipment_apply creature_preview_equipment_cancel
    creature_preview_inventory creature_preview_inventory_empty
    tile_inspection_empty inspector_tile live_tile_columns live_tile_rows live_tile_commands
    inspector_blueprint blueprint_status blueprint_missing_items blueprint_resources_ready blueprint_worker blueprint_priority blueprint_skill_row blueprint_skill blueprint_tool_row blueprint_tool blueprint_cancel_job blueprint_raise_job blueprint_lower_job
    inspector_workshop workshop_state workshop_priority workshop_products workshop_queued workshop_generated workshop_auto workshop_link workshop_toggle_suspended
    inspector_stockpile stockpile_status stockpile_priority stockpile_capacity stockpile_reserved stockpile_pull_from_others stockpile_allow_pull stockpile_contents stockpile_contents_empty stockpile_toggle_suspended
    inspector_agriculture agriculture_kind agriculture_state agriculture_priority agriculture_product agriculture_plots agriculture_planted agriculture_ready agriculture_harvest agriculture_toggle_suspended agriculture_toggle_primary
    selection_configuration selection_action selection_geometry selection_rotate selection_cancel selection_pointer_tip selection_pointer_tool selection_pointer_size tile_world_label tile_world_label_text)
  string(REGEX MATCHALL "id=\"${ID}\"" MATCHES "${RML}")
  list(LENGTH MATCHES COUNT)
  if(NOT COUNT EQUAL 1)
    message(FATAL_ERROR "Inspector ID must exist exactly once: ${ID}")
  endif()
endforeach()
# Retired surfaces: the vertical view rail, the custom profession menu, the paper doll's decorative slots, meters,
# the second creature detail (POP-05), and the Status column of the blueprint list.
foreach(FORBIDDEN "onclick=" "onchange=" "creature_preview_rail" "c-creature-preview-nav" "creature_preview_profession_toggle" "creature_preview_profession_menu" "c-equipment-slot--decorative" "c-equipment-doll" "_meter\"" "id=\"inspector_creature\"" "id=\"creature_professions\"" ">Status</span><span class=\"w98-w-amount\"" "c-button--danger" "Game\\*" "Aggregator" "workshop.set_basics")
  string(REGEX MATCH "${FORBIDDEN}" HIT "${RML}")
  if(HIT)
    message(FATAL_ERROR "Inspector RML still contains a retired or forbidden surface: ${FORBIDDEN}")
  endif()
endforeach()
foreach(NEEDLE "class=\"l-creature-preview w98-sheet w98-palette is-hidden\"" "class=\"l-inspector-panel w98-sheet w98-palette\"" "class=\"w98-caption\"" "class=\"w98-caption__close\"" "role=\"tablist\"" "role=\"tab\"" "role=\"tabpanel\"" "class=\"c-tabs w98-tabs\""
    "class=\"w98-page-frame\"" "class=\"w98-well" ">Attributes<" ">Needs<" "<select id=\"creature_preview_profession\" class=\"w98-select\"" "class=\"w98-listview__heading"
    ">Skill<" ">Level<" ">Active<" ">Slot<" "Carried item" "Center on Map" "Cancel Blueprint" "class=\"w98-separator\"" "Missing materials" "Construction job" "This stockpile is empty." "w98-tooltip"
    "move_target=\"inspector_panel\"" "move_target=\"creature_preview\"" "role=\"dialog\"" "href=\"win98_classic.rcss\"")
  string(FIND "${RML}" "${NEEDLE}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "Inspector is missing its Windows 98 palette composition: ${NEEDLE}")
  endif()
endforeach()
# Pages never scroll: only list boxes and list views inside them do.
string(REGEX MATCH "overflow-y: (auto|scroll)" SCROLLING "${RCSS}")
if(SCROLLING)
  message(FATAL_ERROR "Inspector pages must not scroll; only lists may: ${SCROLLING}")
endif()
foreach(NEEDLE "pointer-events: none" "pointer-events: auto" "34.9091em" "34.5455em" ".l-inspector-panel.is-detached" ".l-creature-preview.is-detached" "right: 12dp; top: 76dp" "right: 16dp; bottom: 62dp" ".c-selection-pointer-tip" ".c-tile-world-label")
  string(FIND "${RCSS}" "${NEEDLE}" POS)
  if(POS EQUAL -1)
    message(FATAL_ERROR "Inspector palette layout is missing: ${NEEDLE}")
  endif()
endforeach()
message(STATUS "Inspector palette IDs, retired surfaces and fixed layout passed")
