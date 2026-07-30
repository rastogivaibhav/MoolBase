cmake_minimum_required(VERSION 3.16)
project(GrapheneDBInstalledConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GrapheneDB CONFIG REQUIRED)

add_executable(graphenedb_installed_consumer main.cpp)
target_link_libraries(graphenedb_installed_consumer PRIVATE GrapheneDB::graphenedb)
