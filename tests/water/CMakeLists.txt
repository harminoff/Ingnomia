add_executable(water_flow_tests
    ../ui-inspector/water_flow_tests.cpp
    ../../src/game/waterflowsolver.cpp
)
target_compile_features(water_flow_tests PRIVATE cxx_std_20)
target_include_directories(water_flow_tests PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(water_flow_tests PRIVATE Qt6::Core Qt6::Xml)
set_target_properties(water_flow_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
if(MSVC)
    target_compile_options(water_flow_tests PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(water_flow_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME water_flow_tests COMMAND water_flow_tests)
set_tests_properties(water_flow_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
