if(NOT DEFINED PROOF_LOG OR NOT EXISTS "${PROOF_LOG}")
  message(FATAL_ERROR "PROOF_LOG must name the native --management-6c-self-test log")
endif()
if(NOT DEFINED RUNNER_LOG OR NOT EXISTS "${RUNNER_LOG}")
  message(FATAL_ERROR "RUNNER_LOG must name the captured native runner output")
endif()
if(NOT DEFINED FRAMEBUFFER OR NOT EXISTS "${FRAMEBUFFER}")
  message(FATAL_ERROR "FRAMEBUFFER must name management-6c-framebuffer.bmp")
endif()
if(NOT DEFINED LOWER_FRAMEBUFFER OR NOT EXISTS "${LOWER_FRAMEBUFFER}")
  message(FATAL_ERROR "LOWER_FRAMEBUFFER must name management-6c-lower-framebuffer.bmp")
endif()
file(READ "${PROOF_LOG}" proof)
file(READ "${RUNNER_LOG}" runner)
foreach(required IN ITEMS
    "MANAGEMENT_6C_SELF_TEST_PASS"
    "MilitaryMutationQueued=true"
    "MissionMutationQueued=true"
    "StableRowKeyboard=true"
    "ConfirmationBlockedAndScoped=true"
    "DiscoveryMasked=true"
    "LargeListPaged=true"
    "UnrelatedDomStable=true"
    "StaleEpochRejected=true"
    "RouteCloseAndInputOwnership=true"
    "ReadableListDetailLayout=true"
    "LowerMissionControlsReachable=true"
    "LifecycleClean=true"
    "Lifecycle load/unload cycles: 100"
    "OpenGL state/error warnings: 0")
  string(FIND "${proof}" "${required}" found)
  if(found LESS 0)
    message(FATAL_ERROR "Missing strict management 6C proof: ${required}")
  endif()
endforeach()
string(TOLOWER "${runner}" runner_lower)
if(runner_lower MATCHES "rmlui.*(error|warning)" OR runner_lower MATCHES "failed to (load|open)"
    OR runner_lower MATCHES "qfatal|assertion failed|self-test-failure")
  message(FATAL_ERROR "Native management 6C runner contains RmlUi/resource/fatal warnings")
endif()
file(SIZE "${FRAMEBUFFER}" framebuffer_size)
file(SIZE "${LOWER_FRAMEBUFFER}" lower_framebuffer_size)
if(framebuffer_size LESS 100000)
  message(FATAL_ERROR "Management 6C framebuffer is unexpectedly small: ${framebuffer_size}")
endif()
if(lower_framebuffer_size LESS 100000)
  message(FATAL_ERROR "Management 6C lower framebuffer is unexpectedly small: ${lower_framebuffer_size}")
endif()
message(STATUS "Management 6C native proof verified (${framebuffer_size}/${lower_framebuffer_size} framebuffer bytes)")
