cmake_minimum_required(VERSION 3.16)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CONTROLLER "${ROOT}/src/gui/ui/screens/management6c/Management6CController.cpp")
set(ADAPTER "${ROOT}/src/gui/ui/controllers/management6c/Management6CQtDataAdapter.cpp")
set(PORT "${ROOT}/src/gui/ui/controllers/management6c/Management6CQtCommandPort.cpp")
set(BRIDGE "${ROOT}/src/gui/ui/controllers/management6c/Management6CQtBridge.cpp")
set(NEIGHBORS "${ROOT}/src/gui/aggregatorneighbors.cpp")
set(ACTIONS "${ROOT}/src/gui/ui/actions/UiActions.h")

foreach(path IN LISTS CONTROLLER ADAPTER PORT BRIDGE NEIGHBORS ACTIONS)
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "Missing integration source: ${path}")
  endif()
endforeach()
file(READ "${CONTROLLER}" controller)
file(READ "${ADAPTER}" adapter)
file(READ "${PORT}" port)
file(READ "${BRIDGE}" bridge)
file(READ "${NEIGHBORS}" neighbors)
file(READ "${ACTIONS}" actions)

foreach(action IN ITEMS
    "military.refresh" "military.add_squad" "military.remove_squad" "military.rename_squad"
    "military.move_squad" "military.remove_gnome" "military.move_gnome" "military.set_attitude"
    "military.move_priority" "military.add_role" "military.remove_role" "military.rename_role"
    "military.assign_role" "military.set_role_civilian" "military.set_uniform_slot"
    "diplomacy.refresh" "diplomacy.refresh_available_gnomes" "diplomacy.start_mission")
  if(NOT controller MATCHES "\"${action}\"")
    message(FATAL_ERROR "Controller omits registered action ${action}")
  endif()
endforeach()

foreach(required IN ITEMS
    "Qt::QueuedConnection" "signalSquads" "signalPriorities" "signalRoles" "signalPossibleMaterials"
    "signalNeighborsUpdate" "signalAvailableGnomes" "signalMissions" "signalUpdateMission")
  if(NOT bridge MATCHES "${required}")
    message(FATAL_ERROR "Queued inbound bridge is missing ${required}")
  endif()
endforeach()
string(FIND "${bridge}" "QMetaObject::invokeMethod( this, [this, role, slot, values]" deferred_materials)
if(deferred_materials LESS 0)
  message(FATAL_ERROR "Uniform material subset must be deferred until after the authoritative role/type snapshot")
endif()

foreach(required IN ITEMS
    "QMetaObject::invokeMethod" "Qt::QueuedConnection" "aggregatorMilitary" "aggregatorNeighbors"
    "validMissionCombination" "MissionType::Spy" "MissionType::Emissary" "MissionType::Raid"
    "MissionType::Sabotage" "MissionAction::None" "DispatchOriginKind::DestructiveConfirmation")
  if(NOT port MATCHES "${required}")
    message(FATAL_ERROR "Queued command adapter is missing ${required}")
  endif()
endforeach()
string(FIND "${port}" "context.topModal = *origin.modal()" tracked_top_modal)
string(FIND "${port}" "context.sourceModal = *origin.modal()" tracked_source_modal)
if(tracked_top_modal LESS 0 OR tracked_source_modal LESS 0)
  message(FATAL_ERROR "Destructive confirmation must validate the exact tracked modal instance")
endif()

foreach(slot IN ITEMS HeadArmor ChestArmor ArmArmor HandArmor LegArmor FootArmor LeftHandHeld RightHandHeld Back)
  if(NOT actions MATCHES "${slot}" OR NOT adapter MATCHES "${slot}" OR NOT port MATCHES "${slot}")
    message(FATAL_ERROR "Authoritative uniform slot ${slot} lacks exhaustive conversion")
  endif()
endforeach()
foreach(value IN ITEMS FLEE DEFEND ATTACK HUNT NOMISSION EXPLORE SPY EMISSARY RAID SABOTAGE
    IMPROVE INSULT INVITE_TRADER INVITE_AMBASSADOR LEAVE_MAP TRAVEL ACTION RETURN RETURNED)
  if(NOT adapter MATCHES "${value}" AND NOT port MATCHES "${value}")
    message(FATAL_ERROR "Authoritative enum name ${value} lacks an explicit conversion")
  endif()
endforeach()
foreach(forbidden IN ITEMS "static_cast<MilitaryAttitude>" "static_cast<MilAttitude>"
    "static_cast<MissionType>" "static_cast<MissionAction>" "static_cast<UniformSlot>")
  string(FIND "${adapter}${port}" "${forbidden}" found_forbidden)
  if(NOT found_forbidden LESS 0)
    message(FATAL_ERROR "Domain enum ordinals must not cross the UI adapter: ${forbidden}")
  endif()
endforeach()

string(FIND "${adapter}" "if( row.discovered )" discovered_projection)
string(FIND "${adapter}" "CatalogId{ \"undiscovered\"" invented_placeholder)
if(discovered_projection LESS 0 OR NOT invented_placeholder LESS 0)
  message(FATAL_ERROR "Undiscovered display fields must map to absent optionals, not placeholder strings")
endif()
string(FIND "${neighbors}" "if( kingdom.discovered || Global::debugMode )" discovery_gate)
if(discovery_gate LESS 0)
  message(FATAL_ERROR "Authoritative discovery gate is not active")
endif()
string(FIND "${neighbors}" "kingdom.attitude > 75" gt75)
string(FIND "${neighbors}" "kingdom.attitude > 50" gt50)
string(FIND "${neighbors}" "kingdom.attitude > 25" gt25)
if(gt75 LESS 0 OR gt50 LESS 0 OR gt25 LESS 0 OR NOT gt75 LESS gt50 OR NOT gt50 LESS gt25)
  message(FATAL_ERROR "Positive neighbor attitude thresholds must be strongest-first")
endif()

if(controller MATCHES "alert|Alert|notification history")
  message(FATAL_ERROR "6C must not fabricate an alert stream")
endif()
if(NOT controller MATCHES "MissionType::Explore: return false" OR NOT port MATCHES "type == MissionType::Spy")
  message(FATAL_ERROR "Unsupported mission combinations are not rejected explicitly")
endif()

message(STATUS "Management 6C source/bridge integration contract verified")
