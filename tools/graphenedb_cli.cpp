// Generated source wrapper. The implementation is split into reviewable fragments.
// Include implementation parts with error handling wrapper.

// Declare main_impl before including parts so we can wrap it
int main_impl(int argc, char** argv);

// Include implementation parts
#include "graphenedb_cli_part_00.inc"
#include "graphenedb_cli_part_01.inc"
#include "graphenedb_cli_part_02.inc"

// Entry point: wrap main_impl with error handling for malformed CLI arguments
int main(int argc, char** argv) {
  try {
    return main_impl(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 2;
  }
}
