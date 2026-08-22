if(NOT DEFINED PROOF_LOG OR NOT EXISTS "${PROOF_LOG}")
  message(FATAL_ERROR "PROOF_LOG must name the native --management-6a-self-test log")
endif()
file(READ "${PROOF_LOG}" LOG)
foreach(NEEDLE
    "MANAGEMENT_6A_SELF_TEST_PASS"
    "ProductionMutationQueued=true"
    "StableRowKeyboard=true"
    "SearchFocusRestored=true"
    "ConfirmationBlockedInput=true"
    "ConfirmationOriginScoped=true"
    "LargeListPaged=true"
    "StockpileMutationQueued=true"
    "AgricultureMutationQueued=true"
    "StaleRevisionRejected=true"
    "InputOwnership=true"
    "LifecycleClean=true"
    "OpenGL state/error warnings: 0")
  if(NOT LOG MATCHES "${NEEDLE}")
    message(FATAL_ERROR "Native management 6A proof missing ${NEEDLE}")
  endif()
endforeach()
string(TOLOWER "${LOG}" LOWER_LOG)
if(LOWER_LOG MATCHES "rmlui.*(error|warning)" OR LOWER_LOG MATCHES "failed to (load|open)")
  message(FATAL_ERROR "Native management 6A proof contains RmlUi/resource warnings")
endif()
message(STATUS "Management 6A native proof log verified")
