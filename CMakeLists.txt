cmake_minimum_required(VERSION 3.16)
project(GrapheneDB VERSION 0.6.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(GRAPHENEDB_BUILD_TESTS "Build GrapheneDB tests" ON)
option(GRAPHENEDB_BUILD_BENCH "Build GrapheneDB benchmarks" ON)
option(GRAPHENEDB_USE_FAISS "Compile with real FAISS. Fails if FAISS is missing." OFF)
option(GRAPHENEDB_BUILD_FUZZERS "Build LLVM libFuzzer targets" OFF)
option(GRAPHENEDB_BUILD_EXAMPLES "Build GrapheneDB API examples and demos" ON)

set(GRAPHENEDB_PLATFORM_SOURCE src/platform_posix.cpp)
if(WIN32)
  set(GRAPHENEDB_PLATFORM_SOURCE src/platform_windows.cpp)
endif()

add_library(graphenedb
  src/c_api.cpp
  src/db.cpp
  src/kosh_adapter.cpp
  src/lattice_placement.cpp
  ${GRAPHENEDB_PLATFORM_SOURCE}
)
add_library(GrapheneDB::graphenedb ALIAS graphenedb)
target_include_directories(graphenedb
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
find_package(Threads REQUIRED)
target_link_libraries(graphenedb PUBLIC Threads::Threads)

if(GRAPHENEDB_USE_FAISS)
  find_path(FAISS_INCLUDE_DIR faiss/IndexFlat.h)
  find_library(FAISS_LIBRARY faiss)
  if(NOT FAISS_INCLUDE_DIR OR NOT FAISS_LIBRARY)
    message(FATAL_ERROR "GRAPHENEDB_USE_FAISS=ON but FAISS headers/library were not found. Install FAISS or turn the option OFF.")
  endif()
  target_include_directories(graphenedb PRIVATE ${FAISS_INCLUDE_DIR})
  target_link_libraries(graphenedb PRIVATE ${FAISS_LIBRARY})
  target_compile_definitions(graphenedb PUBLIC GRAPHENEDB_HAS_FAISS=1)
endif()

if(NOT GRAPHENEDB_BUILD_FUZZERS)
  add_executable(graphenedb_cli tools/graphenedb_cli.cpp)
  target_link_libraries(graphenedb_cli PRIVATE graphenedb)
  add_executable(graphenedb_server tools/graphenedb_server.cpp)
  target_link_libraries(graphenedb_server PRIVATE graphenedb)
  if(NOT WIN32)
    add_executable(graphenedb_healthcheck tools/graphenedb_healthcheck.cpp)
  endif()
endif()

if(GRAPHENEDB_BUILD_TESTS)
  enable_testing()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_executable(graphenedb_tests tests/test_graphenedb.cpp)
  target_link_libraries(graphenedb_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_tests COMMAND graphenedb_tests)

  add_executable(graphenedb_c_api_tests tests/test_c_api.c)
  target_link_libraries(graphenedb_c_api_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_c_api_tests COMMAND graphenedb_c_api_tests)

  add_executable(graphenedb_rc_crash_tests tests/test_rc_gate_crash.cpp)
  target_link_libraries(graphenedb_rc_crash_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_rc_crash_tests COMMAND graphenedb_rc_crash_tests)

  add_executable(graphenedb_rc_fuzz_tests tests/test_rc_gate_fuzz.cpp)
  target_link_libraries(graphenedb_rc_fuzz_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_rc_fuzz_tests COMMAND graphenedb_rc_fuzz_tests)

  add_executable(graphenedb_rc_kosh_adapter_tests tests/test_rc_gate_kosh_adapter.cpp)
  target_link_libraries(graphenedb_rc_kosh_adapter_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_rc_kosh_adapter_tests COMMAND graphenedb_rc_kosh_adapter_tests)

  add_executable(graphenedb_rc_stress_tests tests/test_rc_gate_stress.cpp)
  target_link_libraries(graphenedb_rc_stress_tests PRIVATE graphenedb)
  # Keep default CTest bounded and deterministic. The explicit 100k/1m scripts
  # remain the scale gates; running them inside the ordinary unit-test suite
  # caused CI wall-clock blow-ups and obscured real failures.
  add_test(NAME graphenedb_rc_stress_tests COMMAND graphenedb_rc_stress_tests --incidents 1000 --queries 50 --dim 64 --reopen 1)
  set_tests_properties(graphenedb_rc_stress_tests PROPERTIES TIMEOUT 120 LABELS "stress-smoke")

  add_executable(graphenedb_rc_lock_rotation_metadata_tests tests/test_rc_gate_lock_rotation_metadata.cpp)
  target_link_libraries(graphenedb_rc_lock_rotation_metadata_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_rc_lock_rotation_metadata_tests COMMAND graphenedb_rc_lock_rotation_metadata_tests)

  add_executable(graphenedb_lattice_tests tests/test_graphene_lattice.cpp)
  target_link_libraries(graphenedb_lattice_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_lattice_tests COMMAND graphenedb_lattice_tests)

  add_executable(graphenedb_physical_lattice_storage_tests tests/test_physical_lattice_storage.cpp)
  target_link_libraries(graphenedb_physical_lattice_storage_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_physical_lattice_storage_tests COMMAND graphenedb_physical_lattice_storage_tests)

  add_executable(graphenedb_physical_lattice_primary_tests tests/test_physical_lattice_primary.cpp)
  target_link_libraries(graphenedb_physical_lattice_primary_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_physical_lattice_primary_tests COMMAND graphenedb_physical_lattice_primary_tests)

  add_executable(graphenedb_dense_hex_lattice_index_tests tests/test_dense_hex_lattice_index.cpp)
  target_link_libraries(graphenedb_dense_hex_lattice_index_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_dense_hex_lattice_index_tests COMMAND graphenedb_dense_hex_lattice_index_tests 12)

  add_executable(graphenedb_dense_hex_lattice_stress_tests tests/test_dense_hex_lattice_stress.cpp)
  target_link_libraries(graphenedb_dense_hex_lattice_stress_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_dense_hex_lattice_stress_tests COMMAND graphenedb_dense_hex_lattice_stress_tests 30 1000)

  add_executable(graphenedb_acid_lattice_tests tests/test_acid_lattice.cpp)
  target_link_libraries(graphenedb_acid_lattice_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_acid_lattice_tests COMMAND graphenedb_acid_lattice_tests)

  add_executable(graphenedb_lattice_placement_tests tests/test_lattice_placement.cpp)
  target_link_libraries(graphenedb_lattice_placement_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_lattice_placement_tests COMMAND graphenedb_lattice_placement_tests)

  add_executable(graphenedb_extraction_ingest_tests tests/test_extraction_ingest.cpp)
  target_link_libraries(graphenedb_extraction_ingest_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_extraction_ingest_tests COMMAND graphenedb_extraction_ingest_tests)

  if(TARGET graphenedb_cli)
    add_executable(graphenedb_cli_import_tests tests/test_cli_import.cpp)
    add_test(NAME graphenedb_cli_import_tests COMMAND graphenedb_cli_import_tests $<TARGET_FILE:graphenedb_cli>)

    if(WIN32)
      add_test(NAME graphenedb_cli_extract_tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli_extract.py $<TARGET_FILE:graphenedb_cli>)
    else()
      add_executable(graphenedb_cli_extract_tests tests/test_cli_extract.cpp)
      add_test(NAME graphenedb_cli_extract_tests COMMAND graphenedb_cli_extract_tests $<TARGET_FILE:graphenedb_cli>)
    endif()
  endif()

  add_executable(graphenedb_rc5_crash_matrix_tests tests/test_rc5_crash_matrix.cpp)
  target_link_libraries(graphenedb_rc5_crash_matrix_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_rc5_crash_matrix_tests COMMAND graphenedb_rc5_crash_matrix_tests)

  add_executable(graphenedb_rc5_fault_injection_tests tests/test_rc5_fault_injection.cpp)
  target_link_libraries(graphenedb_rc5_fault_injection_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_rc5_fault_injection_tests COMMAND graphenedb_rc5_fault_injection_tests)

  if(NOT WIN32)
    add_executable(graphenedb_rc_process_kill_tests tests/test_rc_gate_process_kill.cpp)
    target_link_libraries(graphenedb_rc_process_kill_tests PRIVATE graphenedb)
    add_test(NAME graphenedb_rc_process_kill_tests COMMAND graphenedb_rc_process_kill_tests)

    add_executable(graphenedb_real_filesystem_failure_tests tests/test_real_filesystem_failures.cpp)
    target_link_libraries(graphenedb_real_filesystem_failure_tests PRIVATE graphenedb)
    add_test(NAME graphenedb_real_filesystem_failure_tests COMMAND graphenedb_real_filesystem_failure_tests)

    add_executable(graphenedb_disk_pressure_tests tests/test_disk_pressure.cpp)
    target_link_libraries(graphenedb_disk_pressure_tests PRIVATE graphenedb)
    add_test(NAME graphenedb_disk_pressure_tests COMMAND graphenedb_disk_pressure_tests)
  endif()

  add_executable(graphenedb_rc_real_kosh_adapter_tests tests/test_rc_gate_real_kosh_adapter.cpp)
  target_link_libraries(graphenedb_rc_real_kosh_adapter_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_rc_real_kosh_adapter_tests COMMAND graphenedb_rc_real_kosh_adapter_tests)

  add_executable(graphenedb_rc_soak_tests tests/test_rc_gate_soak.cpp)
  target_link_libraries(graphenedb_rc_soak_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_rc_soak_tests COMMAND graphenedb_rc_soak_tests --seconds 2 --dim 16)

  add_executable(graphenedb_rc_1m_storage_tests tests/test_rc_gate_1m_storage.cpp)
  target_link_libraries(graphenedb_rc_1m_storage_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_rc_1m_storage_smoke COMMAND graphenedb_rc_1m_storage_tests --nodes 10000 --queries 3 --dim 2 --reopen 1)

  add_test(
    NAME graphenedb_release_candidate_bundle_meta_tests
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_release_candidate_bundle_meta.py
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/validate_release_candidate_bundle_meta.py
  )

  if(TARGET graphenedb_server AND NOT WIN32)
    add_test(
      NAME graphenedb_server_launch_hardening_tests
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/server_launch_hardening_test.py $<TARGET_FILE:graphenedb_server>
    )
    set_tests_properties(graphenedb_server_launch_hardening_tests PROPERTIES TIMEOUT 60)

    add_test(
      NAME graphenedb_server_pilot_contract_tests
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/server_pilot_contract_test.py $<TARGET_FILE:graphenedb_server>
    )
    set_tests_properties(graphenedb_server_pilot_contract_tests PROPERTIES TIMEOUT 120)
  endif()

  add_test(
    NAME graphenedb_docker_security_static_tests
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/validate_docker_security.py ${CMAKE_CURRENT_SOURCE_DIR}
  )

endif()

if(GRAPHENEDB_BUILD_BENCH)
  add_executable(graphenedb_bench bench/bench_graphenedb.cpp)
  target_link_libraries(graphenedb_bench PRIVATE graphenedb)

  add_executable(graphenedb_rc5_storage_retrieval_bench bench/bench_rc5_storage_retrieval.cpp)
  target_link_libraries(graphenedb_rc5_storage_retrieval_bench PRIVATE graphenedb)

  add_executable(graphenedb_extraction_ingest_bench bench/bench_extraction_ingest.cpp)
  target_link_libraries(graphenedb_extraction_ingest_bench PRIVATE graphenedb)

  add_executable(graphenedb_vector_baseline_bench bench/bench_vector_baseline_comparison.cpp)
  target_link_libraries(graphenedb_vector_baseline_bench PRIVATE graphenedb)

  add_executable(graphenedb_vector_index_recall_bench bench/bench_vector_index_recall.cpp)
  target_link_libraries(graphenedb_vector_index_recall_bench PRIVATE graphenedb)
endif()

if(GRAPHENEDB_BUILD_EXAMPLES AND NOT GRAPHENEDB_BUILD_FUZZERS)
  add_executable(graphenedb_uniqueness_demo examples/graphene_uniqueness_demo.cpp)
  target_link_libraries(graphenedb_uniqueness_demo PRIVATE graphenedb)

  add_executable(graphenedb_api_coding_memory examples/api_coding_memory.cpp)
  target_link_libraries(graphenedb_api_coding_memory PRIVATE graphenedb)

  add_executable(graphenedb_api_incident_memory examples/api_incident_memory.cpp)
  target_link_libraries(graphenedb_api_incident_memory PRIVATE graphenedb)

  add_executable(graphenedb_api_team_brain examples/api_team_brain.cpp)
  target_link_libraries(graphenedb_api_team_brain PRIVATE graphenedb)

  add_executable(graphenedb_api_contradiction_supersession examples/api_contradiction_supersession.cpp)
  target_link_libraries(graphenedb_api_contradiction_supersession PRIVATE graphenedb)

  add_executable(graphenedb_lattice_memory examples/graphene_lattice_memory.cpp)
  target_link_libraries(graphenedb_lattice_memory PRIVATE graphenedb)
endif()

if(GRAPHENEDB_BUILD_FUZZERS)
  if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "GRAPHENEDB_BUILD_FUZZERS=ON requires Clang/libFuzzer")
  endif()
  target_compile_options(graphenedb PRIVATE -fsanitize=fuzzer-no-link -fno-omit-frame-pointer)
  add_executable(graphenedb_wal_fuzzer fuzz/fuzz_wal_open.cpp)
  target_link_libraries(graphenedb_wal_fuzzer PRIVATE graphenedb)
  target_compile_options(graphenedb_wal_fuzzer PRIVATE -fsanitize=fuzzer -fno-omit-frame-pointer)
  target_link_options(graphenedb_wal_fuzzer PRIVATE -fsanitize=fuzzer)
endif()

install(TARGETS graphenedb
  EXPORT GrapheneDBTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(TARGET graphenedb_cli)
  install(TARGETS graphenedb_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(TARGET graphenedb_server)
  install(TARGETS graphenedb_server RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(TARGET graphenedb_healthcheck)
  install(TARGETS graphenedb_healthcheck RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY clients/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/clients
  FILES_MATCHING
  PATTERN "*.py"
  PATTERN "*.md"
  PATTERN "__pycache__" EXCLUDE
  PATTERN "*.pyc" EXCLUDE)
install(DIRECTORY examples/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples FILES_MATCHING PATTERN "*.cpp")
install(FILES README.md CHANGELOG.md LICENSE SECURITY.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

install(EXPORT GrapheneDBTargets
  NAMESPACE GrapheneDB::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GrapheneDB)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GrapheneDBConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/GrapheneDBConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GrapheneDB)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/GrapheneDBConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/GrapheneDBConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/GrapheneDBConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GrapheneDB)
