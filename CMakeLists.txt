cmake_minimum_required(VERSION 3.16)
project(GrapheneDB VERSION 0.6.0 LANGUAGES C CXX)

# GrapheneDB top-level build. The established storage/server build remains in
# cmake/GrapheneDBBase.cmake; this layer materialises the complete experimental
# HypoKosh runtime and its direct validation targets.
include(${CMAKE_CURRENT_LIST_DIR}/cmake/GrapheneDBBase.cmake)

target_sources(graphenedb PRIVATE
  src/entity_resolution.cpp
  src/epistemic_control.cpp
  src/escape.cpp
  src/fiber_bundle.cpp
  src/generic_relation.cpp
  src/hypokosh_runtime.cpp
  src/model_world.cpp
  src/relation_ontology.cpp
  src/self_healing.cpp
  src/stability_critic.cpp
)

# The retained stress harness uses GCC's popcount spelling. Force-include a
# typed compatibility shim on MSVC without altering production code or
# weakening the stress assertion.
if(MSVC AND TARGET graphenedb_rc_stress_tests)
  target_compile_options(
    graphenedb_rc_stress_tests
    PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/tests/msvc_compat.hpp")
endif()

if(GRAPHENEDB_BUILD_TESTS)
  add_executable(graphenedb_cli_reason_tests tests/test_cli_reason.cpp)
  target_link_libraries(graphenedb_cli_reason_tests PRIVATE graphenedb)
  if(TARGET graphenedb_cli)
    add_test(NAME graphenedb_cli_reason_tests
             COMMAND graphenedb_cli_reason_tests $<TARGET_FILE:graphenedb_cli>)
  endif()

  add_executable(graphenedb_generic_reasoning_pipeline_tests
                 tests/test_generic_reasoning_pipeline.cpp)
  target_link_libraries(graphenedb_generic_reasoning_pipeline_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_generic_reasoning_pipeline_tests
           COMMAND graphenedb_generic_reasoning_pipeline_tests)

  add_executable(graphenedb_hypokosh_runtime_tests tests/test_hypokosh_runtime.cpp)
  target_link_libraries(graphenedb_hypokosh_runtime_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_hypokosh_runtime_tests
           COMMAND graphenedb_hypokosh_runtime_tests)

  add_executable(graphenedb_stability_critic_adversarial_tests
                 tests/test_stability_critic_adversarial.cpp)
  target_link_libraries(graphenedb_stability_critic_adversarial_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_stability_critic_adversarial_tests
           COMMAND graphenedb_stability_critic_adversarial_tests)

  add_executable(graphenedb_lyapunov_critic_tests
                 tests/test_lyapunov_critic.cpp)
  target_link_libraries(graphenedb_lyapunov_critic_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_lyapunov_critic_tests
           COMMAND graphenedb_lyapunov_critic_tests)

  add_executable(graphenedb_fiber_bundle_v2_tests
                 tests/test_fiber_bundle_v2.cpp)
  target_link_libraries(graphenedb_fiber_bundle_v2_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_fiber_bundle_v2_tests
           COMMAND graphenedb_fiber_bundle_v2_tests)

  add_executable(graphenedb_epistemic_control_tests
                 tests/test_epistemic_control.cpp)
  target_link_libraries(graphenedb_epistemic_control_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_epistemic_control_tests
           COMMAND graphenedb_epistemic_control_tests)

  if(TARGET graphenedb_server AND NOT WIN32)
    add_test(NAME graphenedb_server_hypokosh_runtime_contract_tests
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/scripts/server_hypokosh_runtime_contract_test.py
                     $<TARGET_FILE:graphenedb_server>)
    set_tests_properties(graphenedb_server_hypokosh_runtime_contract_tests
                         PROPERTIES TIMEOUT 120)
  endif()
endif()

if(GRAPHENEDB_BUILD_EXAMPLES AND NOT GRAPHENEDB_BUILD_FUZZERS)
  add_executable(graphenedb_hypokosh_runtime_demo examples/hypokosh_runtime.cpp)
  target_link_libraries(graphenedb_hypokosh_runtime_demo PRIVATE graphenedb)
endif()
