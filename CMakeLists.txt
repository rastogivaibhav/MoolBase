cmake_minimum_required(VERSION 3.16)
project(graphenedb_v1 LANGUAGES CXX)

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
  src/db.cpp
  src/kosh_adapter.cpp
  ${GRAPHENEDB_PLATFORM_SOURCE}
)
target_include_directories(graphenedb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
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
  target_compile_definitions(graphenedb PRIVATE GRAPHENEDB_HAS_FAISS=1)
endif()

if(NOT GRAPHENEDB_BUILD_FUZZERS)
  add_executable(graphenedb_cli tools/graphenedb_cli.cpp)
  target_link_libraries(graphenedb_cli PRIVATE graphenedb)
endif()

if(GRAPHENEDB_BUILD_TESTS)
  enable_testing()
  add_executable(graphenedb_tests tests/test_graphenedb.cpp)
  target_link_libraries(graphenedb_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_tests COMMAND graphenedb_tests)

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
  add_test(NAME graphenedb_rc_stress_tests COMMAND graphenedb_rc_stress_tests --incidents 5000 --queries 100 --dim 64 --reopen 1)

  add_executable(graphenedb_rc_lock_rotation_metadata_tests tests/test_rc_gate_lock_rotation_metadata.cpp)
  target_link_libraries(graphenedb_rc_lock_rotation_metadata_tests PRIVATE graphenedb)
  add_test(NAME graphenedb_rc_lock_rotation_metadata_tests COMMAND graphenedb_rc_lock_rotation_metadata_tests)

  if(NOT WIN32)
    add_executable(graphenedb_rc_process_kill_tests tests/test_rc_gate_process_kill.cpp)
    target_link_libraries(graphenedb_rc_process_kill_tests PRIVATE graphenedb)
    add_test(NAME graphenedb_rc_process_kill_tests COMMAND graphenedb_rc_process_kill_tests)
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

endif()

if(GRAPHENEDB_BUILD_BENCH)
  add_executable(graphenedb_bench bench/bench_graphenedb.cpp)
  target_link_libraries(graphenedb_bench PRIVATE graphenedb)
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
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)
if(TARGET graphenedb_cli)
  install(TARGETS graphenedb_cli RUNTIME DESTINATION bin)
endif()
install(DIRECTORY include/ DESTINATION include)
